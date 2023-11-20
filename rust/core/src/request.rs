use std::cell::RefCell;
use std::rc::{Rc, Weak};

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

    pub fn cmd(&self, cmd: impl ToString) -> Rc<Request>
    {
        self.inner.borrow_mut().cmd = cmd.to_string();
        self.shared_from_this()
    }

    pub fn msg<T>(&self, msg: T) -> Rc<Request>
        where T: serde::Serialize
    {
        self.inner.borrow_mut().payload = serde_json::to_string(&msg).unwrap().into_bytes().into();
        self.shared_from_this()
    }

    pub fn rsp<F, P>(&self, cb: F) -> Rc<Request>
        where
            P: for<'de> serde::Deserialize<'de>,
            F: Fn(P) + 'static,
    {
        {
            let request_s = self.shared_from_this();
            let mut request = self.inner.borrow_mut();
            request.need_rsp = true;
            request.rsp_handle = Some(Rc::new(move |msg| -> bool {
                if request_s.is_canceled() {
                    request_s.on_finish(FinallyType::Canceled);
                    return true;
                }

                if msg.type_.contains(MsgType::NoSuchCmd) {
                    request_s.on_finish(FinallyType::NoSuchCmd);
                    return true;
                }

                if let Ok(value) = msg.unpack_as::<P>() {
                    cb(value);
                    request_s.on_finish(FinallyType::Normal);
                    true
                } else {
                    request_s.on_finish(FinallyType::RspSerializeError);
                    false
                }
            }));
        }
        self.shared_from_this()
    }

    pub fn finally<F>(&self, finally: F) -> Rc<Request>
        where F: Fn(FinallyType) + 'static
    {
        self.inner.borrow_mut().finally = Some(Box::new(finally));
        self.shared_from_this()
    }

    pub fn call(&self) {
        self.inner.borrow_mut().waiting_rsp = true;

        if self.inner.borrow().canceled {
            self.on_finish(FinallyType::Canceled);
            return;
        }

        self.inner.borrow_mut().self_keeper = Some(self.shared_from_this());

        if self.inner.borrow().rpc.is_none() || self.inner.borrow().rpc.as_ref().unwrap().strong_count() == 0 {
            self.on_finish(FinallyType::RpcExpired);
            return;
        }

        let r = self.inner.borrow().rpc.as_ref().unwrap().upgrade().unwrap();
        if !r.is_ready() {
            self.on_finish(FinallyType::RpcNotReady);
            return;
        }

        self.inner.borrow_mut().seq = r.make_seq();
        r.send_request(self);

        if !self.inner.borrow().need_rsp {
            self.on_finish(FinallyType::NoNeedRsp)
        }
    }

    pub fn call_with_rpc(&self, rpc: Rc<dyn RpcProto>) {
        self.inner.borrow_mut().rpc = Some(Rc::downgrade(&rpc));
        self.call();
    }

    pub fn ping(&self) -> Rc<Request> {
        self.inner.borrow_mut().is_ping = true;
        self.shared_from_this()
    }

    pub fn timeout_ms(&self, timeout_ms: u32) -> Rc<Request> {
        self.inner.borrow_mut().timeout_ms = timeout_ms;
        self.shared_from_this()
    }

    pub fn timeout<F>(&self, timeout_cb: F) -> Rc<Request>
        where F: Fn() + 'static,
    {
        unsafe {
            let request_ptr = self as *const _ as *mut Self;
            let request = &mut *request_ptr;
            let request_impl_ptr = self.inner.as_ptr();
            let request_impl_tmp = &mut *request_impl_ptr;
            request_impl_tmp.timeout_cb = Some(Rc::new(Box::new(move || {
                timeout_cb();
                let request_impl = &mut *request_impl_ptr;
                if request_impl.retry_count == -1 {
                    request.call();
                } else if request_impl.retry_count > 0 {
                    request_impl.retry_count -= 1;
                    request.call();
                } else {
                    request.on_finish(FinallyType::Timeout);
                }
            })));
        }
        self.shared_from_this()
    }

    pub fn add_to(&self, dispose: &mut Dispose) -> Rc<Request>
    {
        let r = self.shared_from_this();
        dispose.add(&r);
        r
    }

    pub fn cancel(&self) -> Rc<Request> {
        self.canceled(true);
        self.on_finish(FinallyType::Canceled);
        self.shared_from_this()
    }

    pub fn reset_cancel(&self) -> Rc<Request> {
        self.canceled(false);
        self.shared_from_this()
    }

    pub fn retry(&self, count: i32) -> Rc<Request> {
        self.inner.borrow_mut().retry_count = count;
        self.shared_from_this()
    }

    pub fn disable_rsp(&self) -> Rc<Request> {
        self.inner.borrow_mut().need_rsp = false;
        self.shared_from_this()
    }

    pub fn rpc(&self, rpc: Weak<dyn RpcProto>) -> Rc<Request> {
        self.inner.borrow_mut().rpc = Some(rpc);
        self.shared_from_this()
    }

    pub fn get_rpc(&self) -> Option<Weak<dyn RpcProto>> {
        self.inner.borrow_mut().rpc.clone()
    }

    pub fn is_canceled(&self) -> bool {
        self.inner.borrow_mut().canceled
    }

    pub fn canceled(&self, canceled: bool) -> Rc<Request> {
        self.inner.borrow_mut().canceled = canceled;
        self.shared_from_this()
    }
}

// private
impl Request {
    fn shared_from_this(&self) -> Rc<Request> {
        self.inner.borrow_mut().self_weak.upgrade().unwrap()
    }

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
