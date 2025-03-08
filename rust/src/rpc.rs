use std::cell::RefCell;
use std::rc::{Rc, Weak};

use log::debug;

use crate::connection::{Connection, DefaultConnection};
use crate::detail::coder;
use crate::detail::msg_dispatcher::{MsgDispatcher, TimeoutCb};
use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::request::{FinallyType, Request};
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
            Rc::new(move |msg: MsgWrapper| -> Option<MsgWrapper> {
                if let Ok(value) = msg.unpack_as::<P>() {
                    let rsp: R = handle(value);
                    match MsgWrapper::make_rsp(msg.seq, rsp) {
                        Ok(rsp) => Some(rsp),
                        Err(error) => {
                            log::error!("response serialization failed: {error}");
                            None
                        }
                    }
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
        inner.seq = inner.seq.wrapping_add(1);
        seq
    }

    pub fn send_request(&self, request: &Request) -> Result<(), FinallyType> {
        let msg;
        let payload;
        let connection;
        {
            let inner = self.inner.borrow();
            let request = request.inner.borrow();
            let Some(options) = &request.active else {
                return Err(FinallyType::Canceled);
            };
            if options.serialize_error {
                return Err(FinallyType::ReqSerializeError);
            }
            let mut type_ = MsgType::Command;
            if options.is_ping {
                type_ |= MsgType::Ping;
            }
            if options.need_rsp {
                type_ |= MsgType::NeedRsp;
            }
            msg = MsgWrapper {
                seq: request.seq,
                type_,
                cmd: options.cmd.clone(),
                data: options.payload.clone().unwrap_or_default(),
                request_payload: None,
            };

            payload = coder::serialize(&msg).map_err(|_| FinallyType::ReqSerializeError)?;
            if options.need_rsp {
                let weak = request.self_weak.clone();
                let call_id = request.call_id;
                let seq = request.seq;
                let handle = options.rsp_handle.as_ref().unwrap().clone();
                let response_weak = weak.clone();
                let timeout_weak = weak.clone();
                inner.dispatcher.borrow_mut().subscribe_rsp(
                    seq,
                    Rc::new(move |msg| {
                        let Some(request) = response_weak.upgrade() else {
                            return true;
                        };
                        if !request.matches_attempt(call_id, seq) {
                            return true;
                        }
                        handle(msg)
                    }),
                    Some(Rc::new(move || {
                        if let Some(request) = timeout_weak.upgrade() {
                            if request.matches_attempt(call_id, seq) {
                                request.on_timeout();
                            }
                        }
                    })),
                    options.timeout_ms,
                    Some(Rc::new(move || {
                        if let Some(request) = weak.upgrade() {
                            if request.matches_attempt(call_id, seq) {
                                request.on_finish(FinallyType::RpcExpired);
                            }
                        }
                    })),
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
        Ok(())
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

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn sequence_wraps_without_panicking() {
        let rpc = Rpc::new(None);
        rpc.inner.borrow_mut().seq = u32::MAX;
        assert_eq!(rpc.make_seq(), u32::MAX);
        assert_eq!(rpc.make_seq(), 0);
    }
}
