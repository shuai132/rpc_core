use std::cell::RefCell;
use std::future::Future;
use std::pin::Pin;
use std::rc::{Rc, Weak};
use std::task::{Context, Poll, Waker};

use log::debug;

use crate::detail::msg_dispatcher::{RspHandle, TimeoutCb};
use crate::detail::msg_wrapper::MsgType;
use crate::dispose::Dispose;
use crate::rpc::RpcProto;
use crate::type_def::{CmdType, SeqType};

pub struct RequestImpl {
    rpc: Option<Weak<dyn RpcProto>>,
    self_weak: Weak<Request>,
    self_keeper: Option<Rc<Request>>,
    pub(crate) seq: SeqType,
    pub(crate) cmd: CmdType,
    pub(crate) payload: Option<Vec<u8>>,
    pub(crate) need_rsp: bool,
    canceled: bool,
    pub(crate) rsp_handle: Option<Rc<RspHandle>>,
    pub(crate) timeout_ms: u32,
    pub(crate) timeout_cb: Option<Rc<TimeoutCb>>,
    finally_type: FinallyType,
    finally: Option<Box<dyn Fn(FinallyType)>>,
    retry_count: i32,
    waiting_rsp: bool,
    pub(crate) is_ping: bool,
}

pub struct Request {
    pub(crate) inner: RefCell<RequestImpl>,
}

pub trait DisposeProto {
    fn add(&mut self, request: &Rc<Request>);
    fn remove(&mut self, request: &Rc<Request>);
}

#[derive(Debug, Clone, PartialEq)]
pub enum FinallyType {
    Normal = 0,
    NoNeedRsp = 1,
    Timeout = 2,
    Canceled = 3,
    RpcExpired = 4,
    RpcNotReady = 5,
    RspSerializeError = 6,
    NoSuchCmd = 7,
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
            FinallyType::RspSerializeError => "rsp_serialize_error",
            FinallyType::NoSuchCmd => "no_such_cmd",
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
                cmd: "".to_string(),
                payload: None,
                need_rsp: false,
                canceled: false,
                rsp_handle: None,
                timeout_ms: 3000,
                timeout_cb: None,
                finally_type: FinallyType::Normal,
                finally: None,
                retry_count: 0,
                waiting_rsp: false,
                is_ping: false,
            }),
        });
        r.inner.borrow_mut().self_weak = Rc::downgrade(&r);
        r.timeout(|| {});
        r
    }

    pub fn create_with_rpc(rpc: Weak<dyn RpcProto>) -> Rc<Self> {
        let r = Self::new();
        r.inner.borrow_mut().rpc = Some(rpc);
        r
    }

    pub fn cmd<'a>(self: &'a Rc<Self>, cmd: impl ToString) -> &'a Rc<Self> {
        self.inner.borrow_mut().cmd = cmd.to_string();
        self
    }

    pub fn msg<'a, T>(self: &'a Rc<Self>, msg: T) -> &'a Rc<Self>
    where
        T: serde::Serialize,
    {
        self.inner.borrow_mut().payload = serde_json::to_string(&msg).unwrap().into_bytes().into();
        self
    }

    pub fn rsp<'a, F, P>(self: &'a Rc<Self>, cb: F) -> &'a Rc<Self>
    where
        P: for<'de> serde::Deserialize<'de>,
        F: Fn(P) + 'static,
    {
        {
            let weak = Rc::downgrade(&self);
            let mut request = self.inner.borrow_mut();
            request.need_rsp = true;
            request.rsp_handle = Some(Rc::new(move |msg| -> bool {
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

                if let Ok(value) = msg.unpack_as::<P>() {
                    cb(value);
                    this.on_finish(FinallyType::Normal);
                    true
                } else {
                    this.on_finish(FinallyType::RspSerializeError);
                    false
                }
            }));
        }
        self
    }

    pub fn finally<'a, F>(self: &'a Rc<Self>, finally: F) -> &'a Rc<Self>
    where
        F: Fn(FinallyType) + 'static,
    {
        self.inner.borrow_mut().finally = Some(Box::new(finally));
        self
    }

    pub fn call(self: &Rc<Self>) {
        self.inner.borrow_mut().waiting_rsp = true;

        if self.inner.borrow().canceled {
            self.on_finish(FinallyType::Canceled);
            return;
        }

        self.inner.borrow_mut().self_keeper = Some(self.clone());

        if self.inner.borrow().rpc.is_none()
            || self.inner.borrow().rpc.as_ref().unwrap().strong_count() == 0
        {
            self.on_finish(FinallyType::RpcExpired);
            return;
        }

        let r = self.inner.borrow().rpc.as_ref().unwrap().upgrade().unwrap();
        if !r.is_ready() {
            self.on_finish(FinallyType::RpcNotReady);
            return;
        }

        self.inner.borrow_mut().seq = r.make_seq();
        r.send_request(self.as_ref());

        if !self.inner.borrow().need_rsp {
            self.on_finish(FinallyType::NoNeedRsp)
        }
    }

    pub fn call_with_rpc(self: &Rc<Self>, rpc: Rc<dyn RpcProto>) {
        self.inner.borrow_mut().rpc = Some(Rc::downgrade(&rpc));
        self.call();
    }

    pub fn ping<'a>(self: &'a Rc<Self>) -> &'a Rc<Self> {
        self.inner.borrow_mut().is_ping = true;
        self
    }

    pub fn timeout_ms<'a>(self: &'a Rc<Self>, timeout_ms: u32) -> &'a Rc<Self> {
        self.inner.borrow_mut().timeout_ms = timeout_ms;
        self
    }

    pub fn timeout<'a, F>(self: &'a Rc<Self>, timeout_cb: F) -> &'a Rc<Self>
    where
        F: Fn() + 'static,
    {
        let weak = Rc::downgrade(&self);
        self.inner.borrow_mut().timeout_cb = Some(Rc::new(Box::new(move || {
            let this = weak.upgrade();
            if this.is_none() {
                return;
            }
            let this = this.unwrap();
            timeout_cb();
            if this.inner.borrow().retry_count == -1 {
                this.call();
            } else if this.inner.borrow().retry_count > 0 {
                this.inner.borrow_mut().retry_count -= 1;
                this.call();
            } else {
                this.on_finish(FinallyType::Timeout);
            }
        })));
        self
    }

    pub fn add_to<'a>(self: &'a Rc<Self>, dispose: &mut Dispose) -> &'a Rc<Self> {
        dispose.add(&self);
        self
    }

    pub fn cancel<'a>(self: &'a Rc<Self>) -> &'a Rc<Self> {
        self.canceled(true);
        self.on_finish(FinallyType::Canceled);
        self
    }

    pub fn reset_cancel<'a>(self: &'a Rc<Self>) -> &'a Rc<Self> {
        self.canceled(false);
        self
    }

    pub fn retry<'a>(self: &'a Rc<Self>, count: i32) -> &'a Rc<Self> {
        self.inner.borrow_mut().retry_count = count;
        self
    }

    pub fn disable_rsp<'a>(self: &'a Rc<Self>) -> &'a Rc<Self> {
        self.inner.borrow_mut().need_rsp = false;
        self
    }

    pub fn rpc<'a>(self: &'a Rc<Self>, rpc: Weak<dyn RpcProto>) -> &'a Rc<Self> {
        self.inner.borrow_mut().rpc = Some(rpc);
        self
    }

    pub fn get_rpc(&self) -> Option<Weak<dyn RpcProto>> {
        self.inner.borrow().rpc.clone()
    }

    pub fn is_canceled(&self) -> bool {
        self.inner.borrow().canceled
    }

    pub fn canceled<'a>(self: &'a Rc<Self>, canceled: bool) -> &'a Rc<Self> {
        self.inner.borrow_mut().canceled = canceled;
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
        struct FutureResultInner<R> {
            result: Option<FutureRet<R>>,
            waker: Option<Waker>,
        }
        struct FutureResult<R> {
            inner: Rc<RefCell<FutureResultInner<R>>>,
        }
        impl<R> Future for FutureResult<R> {
            type Output = FutureRet<R>;
            fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
                let mut result = self.inner.borrow_mut();
                if let Some(result) = result.result.take() {
                    Poll::Ready(result)
                } else {
                    if result.waker.is_none() {
                        result.waker = Some(cx.waker().clone());
                    }
                    Poll::Pending
                }
            }
        }

        let result = FutureResult {
            inner: Rc::new(RefCell::new(FutureResultInner {
                result: None,
                waker: None,
            })),
        };
        let result_c1 = result.inner.clone();
        let result_c2 = result.inner.clone();
        self.rsp(move |msg: R| {
            let mut result = result_c1.borrow_mut();
            result.result = Some(FutureRet {
                type_: FinallyType::Normal,
                result: Some(msg),
            });
        })
        .finally(move |finally| {
            let mut result = result_c2.borrow_mut();
            if result.result.is_some() {
                result.result.as_mut().unwrap().type_ = finally;
            } else {
                result.result = Some(FutureRet {
                    type_: finally,
                    result: None,
                });
            }
            if let Some(waker) = std::mem::replace(&mut result.waker, None) {
                waker.wake();
            }
        })
        .call();

        result.await
    }
}

// private
impl Request {
    fn on_finish(&self, type_: FinallyType) {
        let mut request = self.inner.borrow_mut();
        if !request.waiting_rsp {
            return;
        }
        request.waiting_rsp = false;
        debug!("on_finish: cmd:{} type:{:?}", request.cmd, type_);
        request.finally_type = type_;
        if let Some(finally) = request.finally.take() {
            finally(request.finally_type.clone());
        }
        request.self_keeper = None;
    }
}
