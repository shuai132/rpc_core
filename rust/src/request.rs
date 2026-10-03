use std::cell::RefCell;
use std::future::Future;
use std::pin::Pin;
use std::rc::{Rc, Weak};
use std::task::{Context, Poll, Waker};

use log::debug;

use crate::detail::msg_dispatcher::{RspHandle, TimeoutCb};
use crate::detail::msg_wrapper::MsgType;
use crate::dispose::Dispose;
use crate::rpc::Rpc;
use crate::type_def::{CmdType, SeqType};

pub struct RequestImpl {
    rpc: Option<Weak<Rpc>>,
    pub(crate) self_weak: Weak<Request>,
    self_keeper: Option<Rc<Request>>,
    pub(crate) seq: SeqType,
    pub(crate) call_id: u32,
    pub(crate) active: Option<Rc<CallOptions>>,
    retries_remaining: i32,
    pub(crate) cmd: CmdType,
    pub(crate) payload: Option<Vec<u8>>,
    pub(crate) serialize_error: bool,
    pub(crate) need_rsp: bool,
    canceled: bool,
    pub(crate) rsp_handle: Option<Rc<RspHandle>>,
    pub(crate) timeout_ms: u32,
    pub(crate) timeout_cb: Option<Rc<TimeoutCb>>,
    finally: Option<Rc<dyn Fn(FinallyType)>>,
    retry_count: i32,
    pub(crate) is_ping: bool,
}

pub struct Request {
    pub(crate) inner: RefCell<RequestImpl>,
}

// User code may unwind after this attempt stops accepting responses.
struct AttemptCallbackScope<'a> {
    request: &'a Request,
    call_id: u32,
    seq: SeqType,
    failure: FinallyType,
    completed: bool,
}

impl Drop for AttemptCallbackScope<'_> {
    fn drop(&mut self) {
        if !self.completed && self.request.matches_attempt(self.call_id, self.seq) {
            self.request.on_finish(self.failure.clone());
        }
    }
}

// Builder changes configure the next call, including its retries.
pub(crate) struct CallOptions {
    pub(crate) rpc: Option<Weak<Rpc>>,
    pub(crate) cmd: CmdType,
    pub(crate) payload: Option<Vec<u8>>,
    pub(crate) serialize_error: bool,
    pub(crate) need_rsp: bool,
    pub(crate) rsp_handle: Option<Rc<RspHandle>>,
    pub(crate) timeout_ms: u32,
    pub(crate) timeout_cb: Option<Rc<TimeoutCb>>,
    finally: Option<Rc<dyn Fn(FinallyType)>>,
    pub(crate) is_ping: bool,
    pub(crate) completion: RefCell<Option<FinallyType>>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum FinallyType {
    Normal = 0,
    NoNeedRsp = 1,
    Timeout = 2,
    Canceled = 3,
    RpcExpired = 4,
    RpcNotReady = 5,
    NoSuchCmd = 6,
    ReqSerializeError = 7,
    RspSerializeError = 8,
    Busy = 9,
    SessionReset = 10,
}

impl FinallyType {
    pub fn to_str(&self) -> &'static str {
        match self {
            FinallyType::Normal => "normal",
            FinallyType::NoNeedRsp => "no_need_rsp",
            FinallyType::Timeout => "timeout",
            FinallyType::Canceled => "canceled",
            FinallyType::RpcExpired => "rpc_expired",
            FinallyType::RpcNotReady => "rpc_not_ready",
            FinallyType::NoSuchCmd => "no_such_cmd",
            FinallyType::ReqSerializeError => "req_serialize_error",
            FinallyType::RspSerializeError => "rsp_serialize_error",
            FinallyType::Busy => "busy",
            FinallyType::SessionReset => "session_reset",
        }
    }
}

// public
impl Request {
    pub fn new() -> Rc<Self> {
        let r = Rc::new(Self {
            inner: RefCell::new(RequestImpl {
                rpc: None,
                self_weak: Default::default(),
                self_keeper: None,
                seq: 0,
                call_id: 0,
                active: None,
                retries_remaining: 0,
                cmd: "".to_string(),
                payload: None,
                serialize_error: false,
                need_rsp: false,
                canceled: false,
                rsp_handle: None,
                timeout_ms: 3000,
                timeout_cb: None,
                finally: None,
                retry_count: 0,
                is_ping: false,
            }),
        });
        r.inner.borrow_mut().self_weak = Rc::downgrade(&r);
        r.timeout(|| {});
        r
    }

    pub fn create_with_rpc(rpc: Weak<Rpc>) -> Rc<Self> {
        let r = Self::new();
        r.inner.borrow_mut().rpc = Some(rpc);
        r
    }

    pub fn cmd(self: &Rc<Self>, cmd: impl ToString) -> &Rc<Self> {
        self.inner.borrow_mut().cmd = cmd.to_string();
        self
    }

    pub fn msg<T>(self: &Rc<Self>, msg: T) -> &Rc<Self>
    where
        T: serde::Serialize,
    {
        let payload = serde_json::to_vec(&msg);
        let mut inner = self.inner.borrow_mut();
        inner.serialize_error = payload.is_err();
        inner.payload = payload.ok();
        self
    }

    pub fn rsp<'a, F, P>(self: &'a Rc<Self>, cb: F) -> &'a Rc<Self>
    where
        P: for<'de> serde::Deserialize<'de>,
        F: Fn(P) + 'static,
    {
        let previous = {
            let weak = Rc::downgrade(self);
            let mut request = self.inner.borrow_mut();
            request.need_rsp = true;
            request.rsp_handle.replace(Rc::new(move |msg| -> bool {
                let this = weak.upgrade();
                if this.is_none() {
                    return false;
                }

                let this = this.unwrap();
                if this.is_canceled() {
                    this.on_finish(FinallyType::Canceled);
                    return true;
                }

                if msg.type_.contains(MsgType::NoSuchCmd) {
                    this.on_finish(FinallyType::NoSuchCmd);
                    return true;
                }

                let (call_id, seq) = {
                    let inner = this.inner.borrow();
                    (inner.call_id, inner.seq)
                };
                let mut decoding = AttemptCallbackScope {
                    request: &this,
                    call_id,
                    seq,
                    failure: FinallyType::RspSerializeError,
                    completed: false,
                };
                let decoded = msg.unpack_as::<P>();
                decoding.completed = true;
                // Custom deserialization can cancel and reuse this request.
                if !this.matches_attempt(call_id, seq) {
                    return true;
                }
                if let Ok(value) = decoded {
                    this.finish_response(FinallyType::Normal, || cb(value));
                    true
                } else {
                    this.on_finish(FinallyType::RspSerializeError);
                    false
                }
            }))
        };
        // Captured values may cancel or reconfigure the request when dropped.
        drop(previous);
        self
    }

    pub fn finally<F>(self: &Rc<Self>, finally: F) -> &Rc<Self>
    where
        F: Fn(FinallyType) + 'static,
    {
        let previous = self.inner.borrow_mut().finally.replace(Rc::new(finally));
        drop(previous);
        self
    }

    /// Starts a reusable request. Busy leaves the active call untouched and
    /// does not invoke finally. Other immediate failures complete that call.
    pub fn call(self: &Rc<Self>) -> Result<(), FinallyType> {
        {
            let mut inner = self.inner.borrow_mut();
            if inner.active.is_some() {
                return Err(FinallyType::Busy);
            }
            inner.call_id = inner.call_id.wrapping_add(1);
            inner.active = Some(Rc::new(CallOptions {
                rpc: inner.rpc.clone(),
                cmd: inner.cmd.clone(),
                payload: inner.payload.clone(),
                serialize_error: inner.serialize_error,
                need_rsp: inner.need_rsp,
                rsp_handle: inner.rsp_handle.clone(),
                timeout_ms: inner.timeout_ms,
                timeout_cb: inner.timeout_cb.clone(),
                finally: inner.finally.clone(),
                is_ping: inner.is_ping,
                completion: RefCell::new(None),
            }));
            inner.retries_remaining = inner.retry_count;
            inner.self_keeper = Some(self.clone());
        }
        self.send_attempt()
    }

    fn send_attempt(self: &Rc<Self>) -> Result<(), FinallyType> {
        let (options, call_id) = {
            let inner = self.inner.borrow();
            (inner.active.as_ref().unwrap().clone(), inner.call_id)
        };

        if self.inner.borrow().canceled {
            self.on_finish(FinallyType::Canceled);
            return Err(FinallyType::Canceled);
        }

        let Some(r) = options.rpc.as_ref().and_then(Weak::upgrade) else {
            self.on_finish(FinallyType::RpcExpired);
            return Err(FinallyType::RpcExpired);
        };

        if !r.is_ready() {
            self.on_finish(FinallyType::RpcNotReady);
            return Err(FinallyType::RpcNotReady);
        }

        let seq = r.make_seq();
        self.inner.borrow_mut().seq = seq;
        // Timer registration and transport callbacks may unwind before a send completes.
        struct SendScope<'a> {
            request: &'a Request,
            rpc: &'a Rpc,
            call_id: u32,
            seq: SeqType,
            completed: bool,
        }
        impl Drop for SendScope<'_> {
            fn drop(&mut self) {
                if !self.completed && self.request.matches_attempt(self.call_id, self.seq) {
                    self.rpc.unsubscribe_rsp(self.seq);
                    if self.request.matches_attempt(self.call_id, self.seq) {
                        self.request.on_finish(FinallyType::RpcNotReady);
                    }
                }
            }
        }
        let mut sending = SendScope {
            request: self,
            rpc: &r,
            call_id,
            seq,
            completed: false,
        };
        let sent = r.send_request(self.as_ref());
        sending.completed = true;
        // Sending may reenter a timeout and start another attempt of this call.
        if !self.matches_attempt(call_id, seq) {
            return sent;
        }
        if let Err(error) = sent {
            self.on_finish(error.clone());
            return Err(error);
        }

        if !options.need_rsp {
            self.on_finish(FinallyType::NoNeedRsp)
        }
        Ok(())
    }

    pub fn call_with_rpc(self: &Rc<Self>, rpc: Rc<Rpc>) -> Result<(), FinallyType> {
        if self.inner.borrow().active.is_some() {
            return Err(FinallyType::Busy);
        }
        self.inner.borrow_mut().rpc = Some(Rc::downgrade(&rpc));
        self.call()
    }

    pub fn ping(self: &Rc<Self>) -> &Rc<Self> {
        self.inner.borrow_mut().is_ping = true;
        self
    }

    pub fn timeout_ms(self: &Rc<Self>, timeout_ms: u32) -> &Rc<Self> {
        self.inner.borrow_mut().timeout_ms = timeout_ms;
        self
    }

    pub fn timeout<F>(self: &Rc<Self>, timeout_cb: F) -> &Rc<Self>
    where
        F: Fn() + 'static,
    {
        let previous = self
            .inner
            .borrow_mut()
            .timeout_cb
            .replace(Rc::new(timeout_cb));
        drop(previous);
        self
    }

    pub fn add_to(self: &Rc<Self>, dispose: &mut Dispose) -> &Rc<Self> {
        dispose.add(self);
        self
    }

    pub fn cancel(self: &Rc<Self>) -> &Rc<Self> {
        self.inner.borrow_mut().canceled = true;
        let pending = {
            let request = self.inner.borrow();
            request
                .active
                .as_ref()
                .map(|options| (options.rpc.clone(), request.seq, options.need_rsp))
        };
        if let Some((rpc, seq, true)) = pending {
            if let Some(rpc) = rpc.and_then(|rpc| rpc.upgrade()) {
                rpc.unsubscribe_rsp(seq);
            }
        }
        self.on_finish(FinallyType::Canceled);
        self
    }

    pub fn reset_cancel(self: &Rc<Self>) -> &Rc<Self> {
        self.canceled(false);
        self
    }

    pub fn retry(self: &Rc<Self>, count: i32) -> &Rc<Self> {
        self.inner.borrow_mut().retry_count = count;
        self
    }

    pub fn disable_rsp(self: &Rc<Self>) -> &Rc<Self> {
        self.inner.borrow_mut().need_rsp = false;
        self
    }

    pub fn rpc(self: &Rc<Self>, rpc: Weak<Rpc>) -> &Rc<Self> {
        self.inner.borrow_mut().rpc = Some(rpc);
        self
    }

    pub fn get_rpc(&self) -> Option<Weak<Rpc>> {
        self.inner.borrow().rpc.clone()
    }

    pub fn is_canceled(&self) -> bool {
        self.inner.borrow().canceled
    }

    pub fn canceled(self: &Rc<Self>, canceled: bool) -> &Rc<Self> {
        if canceled {
            return self.cancel();
        }
        self.inner.borrow_mut().canceled = false;
        self
    }
}

#[derive(Debug)]
pub struct FutureRet<R> {
    pub type_: FinallyType,
    pub result: Option<R>,
}

impl<R> FutureRet<R> {
    pub fn unwrap(self) -> R {
        if self.type_ != FinallyType::Normal {
            panic!(
                "called `FutureRet::unwrap()` on FinallyType::{:?}",
                self.type_
            );
        }
        match self.result {
            Some(val) => val,
            None => panic!("called `FutureRet::unwrap()` on a `None` value"),
        }
    }
}

impl Request {
    pub async fn future<R>(self: &Rc<Self>) -> FutureRet<R>
    where
        R: for<'de> serde::Deserialize<'de> + 'static,
    {
        if self.inner.borrow().active.is_some() {
            return FutureRet {
                type_: FinallyType::Busy,
                result: None,
            };
        }
        struct FutureResultInner<R> {
            result: Option<FutureRet<R>>,
            waker: Option<Waker>,
            completed: bool,
        }
        struct FutureResult<R> {
            inner: Rc<RefCell<FutureResultInner<R>>>,
        }
        impl<R> Future for FutureResult<R> {
            type Output = FutureRet<R>;
            fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
                if let Some(result) = self.inner.borrow_mut().result.take() {
                    return Poll::Ready(result);
                }
                // Waker cloning and dropping can run user code that completes this call.
                let waker = cx.waker().clone();
                let previous = {
                    let mut result = self.inner.borrow_mut();
                    if let Some(result) = result.result.take() {
                        return Poll::Ready(result);
                    }
                    result.waker.replace(waker)
                };
                drop(previous);
                Poll::Pending
            }
        }

        struct CancelOnDrop {
            request: Weak<Request>,
            call_id: u32,
        }
        impl Drop for CancelOnDrop {
            fn drop(&mut self) {
                if let Some(request) = self.request.upgrade() {
                    let still_active = {
                        let inner = request.inner.borrow();
                        inner.active.is_some() && inner.call_id == self.call_id
                    };
                    if still_active {
                        request.cancel();
                    }
                }
            }
        }

        // Capture the logical call before sending: synchronous completion can
        // reuse the request before call() returns. Retries retain this ID.
        let _cancel = CancelOnDrop {
            request: Rc::downgrade(self),
            call_id: self.inner.borrow().call_id.wrapping_add(1),
        };

        let result = FutureResult {
            inner: Rc::new(RefCell::new(FutureResultInner {
                result: None,
                waker: None,
                completed: false,
            })),
        };
        let result_c1 = result.inner.clone();
        let result_c2 = result.inner.clone();
        self.rsp(move |msg: R| {
            let mut result = result_c1.borrow_mut();
            // The reusable builder can retain these callbacks for later calls.
            // Consuming the first result must not enable them again.
            if result.completed {
                return;
            }
            result.result = Some(FutureRet {
                type_: FinallyType::Normal,
                result: Some(msg),
            });
        })
        .finally(move |finally| {
            let mut result = result_c2.borrow_mut();
            if result.completed {
                return;
            }
            result.completed = true;
            if result.result.is_some() {
                result.result.as_mut().unwrap().type_ = finally;
            } else {
                result.result = Some(FutureRet {
                    type_: finally,
                    result: None,
                });
            }
            let waker = result.waker.take();
            drop(result);
            if let Some(waker) = waker {
                waker.wake();
            }
        })
        .call()
        .ok();

        result.await
    }
}

// private
impl Request {
    pub(crate) fn matches_attempt(&self, call_id: u32, seq: SeqType) -> bool {
        let inner = self.inner.borrow();
        inner.active.is_some() && inner.call_id == call_id && inner.seq == seq
    }

    pub(crate) fn on_timeout(self: &Rc<Self>) {
        let (options, call_id, seq) = {
            let inner = self.inner.borrow();
            (
                inner.active.as_ref().unwrap().clone(),
                inner.call_id,
                inner.seq,
            )
        };
        let mut completion = AttemptCallbackScope {
            request: self,
            call_id,
            seq,
            failure: FinallyType::Timeout,
            completed: false,
        };
        if let Some(callback) = &options.timeout_cb {
            callback();
        }
        completion.completed = true;
        if !self.matches_attempt(call_id, seq) || self.is_canceled() {
            return;
        }
        let retry = {
            let mut inner = self.inner.borrow_mut();
            let retry = inner.retries_remaining == -1 || inner.retries_remaining > 0;
            if inner.retries_remaining > 0 {
                inner.retries_remaining -= 1;
            }
            retry
        };
        if retry {
            let _ = self.send_attempt();
        } else {
            self.on_finish(FinallyType::Timeout);
        }
    }

    pub(crate) fn on_finish(&self, type_: FinallyType) {
        self.finish_response(type_, || {});
    }

    fn finish_response(&self, type_: FinallyType, response: impl FnOnce()) {
        let mut request = self.inner.borrow_mut();
        let Some(completed) = request.active.take() else {
            return;
        };
        *completed.completion.borrow_mut() = Some(type_.clone());
        debug!("on_finish: cmd:{} type:{:?}", completed.cmd, type_);
        let self_keeper = request.self_keeper.take();
        drop(request);
        // Run finally before releasing this call, including during response unwinding.
        struct FinallyScope {
            callback: Option<Rc<dyn Fn(FinallyType)>>,
            type_: FinallyType,
        }
        impl Drop for FinallyScope {
            fn drop(&mut self) {
                if let Some(callback) = self.callback.take() {
                    callback(self.type_.clone());
                }
            }
        }
        let completion = FinallyScope {
            callback: completed.finally.clone(),
            type_,
        };
        response();
        drop(completion);
        drop(self_keeper);
    }
}
