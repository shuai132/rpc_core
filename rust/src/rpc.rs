use std::cell::RefCell;
use std::rc::{Rc, Weak};

use log::debug;

use crate::connection::{Connection, DefaultConnection};
use crate::detail::coder;
use crate::detail::msg_dispatcher::{MsgDispatcher, TimeoutCb};
use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::request::FinallyType;
use crate::request::Request;
use crate::type_def::SeqType;

pub struct RpcImpl {
    weak: Weak<Rpc>,
    connection: Rc<RefCell<dyn Connection>>,
    dispatcher: Rc<RefCell<MsgDispatcher>>,
    seq: SeqType,
    is_ready: bool,
}

pub struct Rpc {
    inner: RefCell<RpcImpl>,
}

impl Rpc {
    #[allow(clippy::unwrap_or_default)]
    pub fn new(connection: Option<Rc<RefCell<dyn Connection>>>) -> Rc<Rpc> {
        let connection = connection.unwrap_or(DefaultConnection::new());
        let rpc = Rc::new(Rpc {
            inner: RefCell::new(RpcImpl {
                weak: Weak::new(),
                connection: connection.clone(),
                dispatcher: MsgDispatcher::new(connection),
                seq: 0,
                is_ready: false,
            }),
        });
        rpc.inner.borrow_mut().weak = Rc::downgrade(&rpc);
        rpc
    }

    pub fn subscribe<C, F, P, R>(&self, cmd: C, handle: F)
    where
        C: ToString,
        P: for<'de> serde::Deserialize<'de>,
        R: serde::Serialize,
        F: Fn(P) -> R + 'static,
    {
        self.inner.borrow().dispatcher.borrow_mut().subscribe_cmd(
            cmd.to_string(),
            Box::new(move |msg| -> Option<MsgWrapper> {
                if let Ok(value) = msg.unpack_as::<P>() {
                    let rsp: R = handle(value);
                    Some(MsgWrapper::make_rsp(msg.seq, rsp))
                } else {
                    None
                }
            }),
        );
    }

    pub fn unsubscribe<C>(&self, cmd: C)
    where
        C: ToString,
    {
        self.inner
            .borrow()
            .dispatcher
            .borrow_mut()
            .unsubscribe_cmd(cmd.to_string());
    }

    pub fn create_request(&self) -> Rc<Request> {
        Request::create_with_rpc(self.inner.borrow().weak.clone())
    }

    pub fn cmd<T>(&self, cmd: T) -> Rc<Request>
    where
        T: ToString,
    {
        let r = self.create_request();
        r.cmd(cmd.to_string());
        r
    }

    pub fn ping(&self) -> Rc<Request> {
        let r = self.create_request();
        r.ping();
        r
    }

    pub fn ping_msg(&self, payload: impl ToString) -> Rc<Request> {
        let r = self.create_request();
        r.ping().msg(payload.to_string());
        r
    }

    pub fn set_timer<F>(&self, timer_impl: F)
    where
        F: Fn(u32, Box<TimeoutCb>) + 'static,
    {
        self.inner
            .borrow()
            .dispatcher
            .borrow_mut()
            .set_timer_impl(timer_impl);
    }

    pub fn set_ready(&self, ready: bool) {
        self.inner.borrow_mut().is_ready = ready;
    }

    pub fn get_connection(&self) -> Rc<RefCell<dyn Connection>> {
        self.inner.borrow().connection.clone()
    }
}

impl Rpc {
    pub fn make_seq(&self) -> SeqType {
        let mut inner = self.inner.borrow_mut();
        let seq = inner.seq;
        inner.seq += 1;
        seq
    }

    pub fn send_request(&self, request: &Request) {
        let msg;
        let payload;
        let connection;
        {
            let inner = self.inner.borrow();
            let request_inner = request.inner.borrow();
            let mut type_ = MsgType::Command | MsgType::PayloadJson;
            if request_inner.is_ping {
                type_ |= MsgType::Ping;
            }
            if request_inner.need_rsp {
                type_ |= MsgType::NeedRsp;
            }
            msg = MsgWrapper {
                seq: request_inner.seq,
                type_,
                cmd: request_inner.cmd.clone(),
                data: request_inner.payload.clone().unwrap_or_default(),
                request_payload: None,
            };

            let Some(serialized) = coder::serialize(&msg) else {
                drop(request_inner);
                drop(inner);
                request.on_finish(FinallyType::RspSerializeError);
                return;
            };
            payload = serialized;
            if request_inner.need_rsp {
                inner.dispatcher.borrow_mut().subscribe_rsp(
                    request_inner.seq,
                    request_inner.rsp_handle.as_ref().unwrap().clone(),
                    request_inner.timeout_cb.clone(),
                    request_inner.timeout_ms,
                );
            }
            connection = inner.connection.clone();
        }
        debug!(
            "=> seq:{} type:{} {}",
            msg.seq,
            if msg.type_.contains(MsgType::Ping) {
                "ping"
            } else {
                "cmd"
            },
            msg.cmd
        );
        connection.borrow().send_package(payload);
    }

    pub(crate) fn unsubscribe_rsp(&self, seq: SeqType) {
        self.inner
            .borrow()
            .dispatcher
            .borrow_mut()
            .unsubscribe_rsp(seq);
    }

    pub fn is_ready(&self) -> bool {
        self.inner.borrow().is_ready
    }
}
