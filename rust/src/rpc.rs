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
        let cmd = cmd.to_string();
        let previous = self.inner.borrow().dispatcher.borrow_mut().subscribe_cmd(
            cmd,
            Rc::new(move |msg: MsgWrapper| -> Option<MsgWrapper> {
                if let Ok(value) = msg.unpack_as::<P>() {
                    let rsp: R = handle(value);
                    match MsgWrapper::make_rsp(msg.seq, rsp) {
                        Ok(rsp) => Some(rsp),
                        Err(_) => {
                            log::error!("response serialization failed");
                            None
                        }
                    }
                } else {
                    None
                }
            }),
        );
        drop(previous);
    }

    pub fn unsubscribe<C>(&self, cmd: C)
    where
        C: ToString,
    {
        let cmd = cmd.to_string();
        let previous = self
            .inner
            .borrow()
            .dispatcher
            .borrow_mut()
            .unsubscribe_cmd(cmd);
        drop(previous);
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
        let previous = self
            .inner
            .borrow()
            .dispatcher
            .borrow_mut()
            .set_timer_impl(timer_impl);
        drop(previous);
    }

    pub fn set_ready(&self, ready: bool) {
        let dispatcher = {
            let mut inner = self.inner.borrow_mut();
            inner.is_ready = ready;
            inner.dispatcher.clone()
        };
        dispatcher.borrow_mut().set_ready(ready);
    }

    /// Start a new logical session, retaining subscriptions, readiness and sequence.
    /// Ordinary reconnects only change set_ready().
    pub fn reset_session(&self) {
        let dispatcher = self.inner.borrow().dispatcher.clone();
        MsgDispatcher::reset_session(&dispatcher);
    }

    pub fn get_connection(&self) -> Rc<RefCell<dyn Connection>> {
        self.inner.borrow().connection.clone()
    }
}

impl Rpc {
    pub fn make_seq(&self) -> SeqType {
        let mut inner = self.inner.borrow_mut();
        let mut next = inner.seq;
        let seq = inner.dispatcher.borrow().make_seq(&mut next);
        inner.seq = next;
        seq
    }

    pub fn send_request(&self, request: &Request) -> Result<(), FinallyType> {
        let msg;
        let payload;
        let connection;
        let attempt;
        let dispatcher;
        let options;
        let weak;
        {
            let inner = self.inner.borrow();
            let request = request.inner.borrow();
            let Some(active) = &request.active else {
                return Err(FinallyType::Canceled);
            };
            options = active.clone();
            weak = request.self_weak.clone();
            dispatcher = inner.dispatcher.clone();
            attempt = (request.call_id, request.seq);
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
                // Omitted messages use JSON's representation of serde unit.
                data: options.payload.clone().unwrap_or_else(|| b"null".to_vec()),
                request_payload: None,
            };

            payload = coder::serialize(&msg).map_err(|_| FinallyType::ReqSerializeError)?;
            connection = inner.connection.clone();
        }
        if options.need_rsp {
            let weak = weak.clone();
            let call_id = attempt.0;
            let seq = attempt.1;
            let handle = options.rsp_handle.as_ref().unwrap().clone();
            let response_weak = weak.clone();
            let timeout_weak = weak.clone();
            let reset_weak = weak.clone();
            MsgDispatcher::subscribe_rsp(
                &dispatcher,
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
                Some(Rc::new(move || {
                    if let Some(request) = reset_weak.upgrade() {
                        if request.matches_attempt(call_id, seq) {
                            request.on_finish(FinallyType::SessionReset);
                        }
                    }
                })),
            );
        }
        if !request.matches_attempt(attempt.0, attempt.1) {
            return match options.completion.borrow().clone() {
                None | Some(FinallyType::Normal) => Ok(()),
                Some(error) => Err(error),
            };
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
        // A reentrant timer implementation may also disconnect this RPC.
        let sent = self.is_ready() && connection.borrow().send_package(payload);
        if !sent && request.matches_attempt(attempt.0, attempt.1) {
            self.unsubscribe_rsp(attempt.1);
        }
        if sent {
            Ok(())
        } else {
            Err(FinallyType::RpcNotReady)
        }
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
    use std::cell::Cell;

    #[test]
    fn sequence_wrap_keeps_pending_requests_and_timeouts() {
        let rpc = Rpc::new(None);
        rpc.set_ready(true);
        let sent = Rc::new(RefCell::new(Vec::new()));
        let output = sent.clone();
        rpc.get_connection()
            .borrow_mut()
            .set_send_package_impl(Box::new(move |packet| {
                output
                    .borrow_mut()
                    .push(coder::deserialize(&packet).unwrap().seq);
                true
            }));
        let timers = Rc::new(RefCell::new(Vec::<Box<TimeoutCb>>::new()));
        let pending_timers = timers.clone();
        rpc.set_timer(move |_, cb| pending_timers.borrow_mut().push(cb));
        let completions = Rc::new(Cell::new(0));
        let mut requests = Vec::new();
        for index in 0..3 {
            if index == 1 {
                rpc.inner.borrow_mut().seq = u32::MAX;
            }
            let request = rpc.cmd("pending");
            let finished = completions.clone();
            request.rsp(|_: ()| {}).finally(move |status| {
                assert_eq!(status, FinallyType::Timeout);
                finished.set(finished.get() + 1);
            });
            request.call().unwrap();
            requests.push(Rc::downgrade(&request));
        }
        assert_eq!(*sent.borrow(), vec![0, u32::MAX, 1]);
        for timer in timers.borrow().iter() {
            timer();
        }
        assert_eq!(completions.get(), 3);
        assert!(requests.iter().all(|request| request.upgrade().is_none()));
    }

    #[test]
    fn sequence_wraps_without_panicking() {
        let rpc = Rpc::new(None);
        rpc.inner.borrow_mut().seq = u32::MAX;
        assert_eq!(rpc.make_seq(), u32::MAX);
        assert_eq!(rpc.make_seq(), 0);
    }
}
