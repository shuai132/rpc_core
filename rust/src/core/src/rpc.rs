use std::cell::RefCell;
use std::rc::{Rc, Weak};

use log::debug;

use crate::connection::{Connection, DefaultConnection};
use crate::detail::coder;
use crate::detail::msg_dispatcher::{MsgDispatcher, TimeoutCb};
use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::request::Request;
use crate::type_def::SeqType;

pub trait RpcProto {
    fn make_seq(&self) -> SeqType;
    fn send_request(&self, request: &Request);
    fn is_ready(&self) -> bool;
}

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
        self.inner.borrow_mut().dispatcher.borrow_mut().subscribe_cmd(cmd.to_string(), Box::new(move |msg| -> Option<MsgWrapper>{
            if let Ok(value) = msg.unpack_as::<P>() {
                let rsp: R = handle(value);
                Some(MsgWrapper::make_rsp(msg.seq, rsp))
            } else {
                None
            }
        }));
    }

    pub fn unsubscribe<C>(&self, cmd: C)
        where C: ToString
    {
        self.inner.borrow_mut().dispatcher.borrow_mut().unsubscribe_cmd(cmd.to_string());
    }

    pub fn create_request(&self) -> Rc<Request>
    {
        Request::create_with_rpc(self.inner.borrow().weak.clone())
    }

    pub fn cmd<T>(&self, cmd: T) -> Rc<Request>
        where T: ToString
    {
        let r = self.create_request();
        r.cmd(cmd.to_string());
        r
    }

    pub fn ping(&self) -> Rc<Request>
    {
        let r = self.create_request();
        r.ping();
        r
    }

    pub fn ping_msg(&self, payload: impl ToString) -> Rc<Request>
    {
        let r = self.create_request();
        r.ping().msg(payload.to_string());
        r
    }

    pub fn set_timer<F>(&self, timer_impl: F)
        where F: Fn(u32, Box<TimeoutCb>) + 'static
    {
        self.inner.borrow_mut().dispatcher.borrow_mut().set_timer_impl(timer_impl);
    }

    pub fn set_ready(&self, ready: bool) {
        self.inner.borrow_mut().is_ready = ready;
    }

    pub fn get_connection(&self) -> Rc<RefCell<dyn Connection>> {
        self.inner.borrow().connection.clone()
    }
}

impl RpcProto for Rpc {
    fn make_seq(&self) -> SeqType {
        let mut inner = self.inner.borrow_mut();
        let seq = inner.seq;
        inner.seq += 1;
        seq
    }

    fn send_request(&self, request: &Request) {
        let msg;
        let payload;
        let connection;
        {
            let inner = self.inner.borrow_mut();
            let request = request.inner.borrow_mut();
            if request.need_rsp {
                inner.dispatcher.borrow_mut().subscribe_rsp(request.seq, request.rsp_handle.as_ref().unwrap().clone(), request.timeout_cb.clone(), request.timeout_ms);
            }
            msg = MsgWrapper {
                seq: request.seq,
                type_: (|| {
                    let mut type_val = MsgType::Command;
                    if request.is_ping {
                        type_val |= MsgType::Ping;
                    }
                    if request.need_rsp {
                        type_val |= MsgType::NeedRsp;
                    }
                    type_val
                })(),
                cmd: request.cmd.clone(),
                data: request.payload.clone().unwrap_or(vec![]),
                request_payload: None,
            };

            payload = coder::serialize(&msg);
            connection = inner.connection.clone();
        }
        debug!("=> seq:{} type:{} {}", msg.seq, if msg.type_.contains( MsgType::Ping) { "ping" } else {"cmd"}, msg.cmd);
        connection.borrow().send_package(payload);
    }

    fn is_ready(&self) -> bool {
        self.inner.borrow().is_ready
    }
}
