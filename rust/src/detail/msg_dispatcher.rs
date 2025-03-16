use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::{Rc, Weak};

use log::{debug, error, trace, warn};

use crate::connection::Connection;
use crate::detail::coder;
use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::request::FinallyType;
use crate::type_def::{CmdType, SeqType};

pub type TimeoutCb = dyn Fn();
pub type TimerImpl = dyn Fn(u32, Box<TimeoutCb>);

type CmdHandle = Rc<dyn Fn(MsgWrapper) -> Option<MsgWrapper>>;
pub type RspHandle = dyn Fn(MsgWrapper) -> bool;

struct PendingResponse {
    handle: Option<Rc<RspHandle>>,
    expired: Option<Rc<TimeoutCb>>,
    reset: Option<Rc<TimeoutCb>>,
    registration: u64,
}

pub struct MsgDispatcher {
    conn: Weak<RefCell<dyn Connection>>,
    cmd_handle_map: HashMap<CmdType, CmdHandle>,
    rsp_handle_map: HashMap<SeqType, PendingResponse>,
    timer_impl: Option<Rc<TimerImpl>>,
    session_generation: u64,
    registration_id: u64,
    responses_paused: bool,
}

impl MsgDispatcher {
    pub fn new(conn: Rc<RefCell<dyn Connection>>) -> Rc<RefCell<Self>> {
        Rc::<RefCell<Self>>::new_cyclic(|this_weak| {
            let dispatcher = RefCell::new(Self {
                conn: Rc::downgrade(&conn),
                cmd_handle_map: HashMap::new(),
                rsp_handle_map: HashMap::new(),
                timer_impl: None,
                session_generation: 0,
                registration_id: 0,
                responses_paused: false,
            });

            let this_weak = this_weak.clone();
            conn.borrow_mut()
                .set_recv_package_impl(Box::new(move |payload| {
                    let Some(this) = this_weak.upgrade() else {
                        return;
                    };
                    if let Some(msg) = coder::deserialize(&payload) {
                        Self::dispatch(&this, msg);
                    } else {
                        error!("deserialize error");
                    }
                }));
            dispatcher
        })
    }
}

impl MsgDispatcher {
    pub fn make_seq(&self, next: &mut SeqType) -> SeqType {
        while self.rsp_handle_map.contains_key(next) {
            *next = next.wrapping_add(1);
        }
        let seq = *next;
        *next = next.wrapping_add(1);
        seq
    }

    pub fn set_ready(&mut self, ready: bool) {
        self.responses_paused = !ready;
    }

    pub fn reset_session(this: &Rc<RefCell<Self>>) {
        let previous = {
            let mut dispatcher = this.borrow_mut();
            dispatcher.session_generation = dispatcher.session_generation.wrapping_add(1);
            std::mem::take(&mut dispatcher.rsp_handle_map)
        };
        // User completions may immediately register requests for the new session.
        for (_, pending) in previous {
            if let Some(reset) = pending.reset {
                reset();
            }
        }
    }

    pub fn subscribe_cmd(&mut self, cmd: String, handle: CmdHandle) {
        self.cmd_handle_map.insert(cmd, handle);
    }

    pub fn unsubscribe_cmd(&mut self, cmd: String) {
        if self.cmd_handle_map.remove(&cmd).is_some() {
            debug!("erase cmd: {}", cmd);
        } else {
            debug!("not subscribe cmd for: {}", cmd);
        }
    }

    pub fn subscribe_rsp(
        this: &Rc<RefCell<Self>>,
        seq: SeqType,
        rsp_handle: Rc<RspHandle>,
        timeout_cb: Option<Rc<TimeoutCb>>,
        timeout_ms: u32,
        expired: Option<Rc<TimeoutCb>>,
        reset: Option<Rc<TimeoutCb>>,
    ) {
        let (registration, timer_impl) = {
            let mut dispatcher = this.borrow_mut();
            dispatcher.registration_id = dispatcher.registration_id.wrapping_add(1);
            let registration = dispatcher.registration_id;
            dispatcher.rsp_handle_map.insert(
                seq,
                PendingResponse {
                    handle: Some(rsp_handle),
                    expired,
                    reset,
                    registration,
                },
            );
            (registration, dispatcher.timer_impl.clone())
        };
        if let Some(timer_impl) = timer_impl {
            let this_weak = Rc::downgrade(this);
            timer_impl(
                timeout_ms,
                Box::new(move || {
                    let Some(this) = this_weak.upgrade() else {
                        debug!("seq:{} timeout after destroy", seq);
                        return;
                    };

                    let expired = {
                        let mut dispatcher = this.borrow_mut();
                        // Stop accepting responses, retaining the call for reset_session().
                        dispatcher
                            .rsp_handle_map
                            .get_mut(&seq)
                            .filter(|pending| pending.registration == registration)
                            .and_then(|pending| pending.handle.take())
                            .is_some()
                    };
                    if expired {
                        if let Some(timeout_cb) = &timeout_cb {
                            timeout_cb();
                        }
                        let mut dispatcher = this.borrow_mut();
                        if dispatcher
                            .rsp_handle_map
                            .get(&seq)
                            .is_some_and(|pending| pending.registration == registration)
                        {
                            dispatcher.rsp_handle_map.remove(&seq);
                        }
                        trace!(
                            "Timeout seq={}, rsp_handle_map.size={}",
                            seq,
                            dispatcher.rsp_handle_map.len()
                        );
                    }
                }),
            );
        } else {
            warn!("no timeout will cause memory leak!");
        }
    }

    pub fn unsubscribe_rsp(&mut self, seq: SeqType) {
        self.rsp_handle_map.remove(&seq);
    }

    fn send_response(
        this: &Rc<RefCell<Self>>,
        msg: &MsgWrapper,
        generation: u64,
    ) -> Result<(), FinallyType> {
        let conn = {
            let dispatcher = this.borrow();
            if dispatcher.session_generation != generation {
                return Err(FinallyType::SessionReset);
            }
            if dispatcher.responses_paused {
                return Err(FinallyType::RpcNotReady);
            }
            dispatcher.conn.upgrade().ok_or(FinallyType::RpcExpired)?
        };
        let payload = coder::serialize(msg).map_err(|_| FinallyType::RspSerializeError)?;
        let sent = conn.borrow().send_package(payload);
        if sent {
            Ok(())
        } else {
            Err(FinallyType::RpcNotReady)
        }
    }

    pub fn dispatch(this: &Rc<RefCell<Self>>, mut msg: MsgWrapper) {
        let generation = this.borrow().session_generation;
        if msg.type_.contains(MsgType::Command) {
            // ping
            let is_ping = msg.type_.contains(MsgType::Ping);
            if is_ping {
                debug!("<= seq:{} type:ping", msg.seq);
                msg.type_ = MsgType::Response | MsgType::Pong;
                debug!("=> seq:{} type:pong", msg.seq);
                let _ = Self::send_response(this, &msg, generation);
                return;
            }

            // command
            debug!("<= seq:{} cmd:{}", msg.seq, msg.cmd);
            let cmd = &msg.cmd;
            let handle = this.borrow().cmd_handle_map.get(cmd).cloned();
            if let Some(handle) = handle {
                let need_rsp = msg.type_.contains(MsgType::NeedRsp);
                let resp = handle(msg);
                if need_rsp {
                    if let Some(rsp) = resp {
                        debug!("=> seq:{} type:rsp", rsp.seq);
                        let _ = Self::send_response(this, &rsp, generation);
                    }
                }
            } else {
                debug!("not subscribe cmd for: {}", cmd);
                let need_rsp = msg.type_.contains(MsgType::NeedRsp);
                if need_rsp {
                    debug!("=> seq:{} type:rsp", msg.seq);
                    let mut rsp = MsgWrapper::new();
                    rsp.seq = msg.seq;
                    rsp.type_ = MsgType::Response | MsgType::NoSuchCmd;
                    let _ = Self::send_response(this, &rsp, generation);
                }
            }
        } else if msg.type_.contains(MsgType::Response) {
            // pong or response
            debug!(
                "<= seq:{} type:{}",
                msg.seq,
                if msg.type_.contains(MsgType::Pong) {
                    "pong"
                } else {
                    "rsp"
                }
            );
            let handle = {
                let mut dispatcher = this.borrow_mut();
                let handle = dispatcher
                    .rsp_handle_map
                    .get_mut(&msg.seq)
                    .and_then(|pending| pending.handle.take());
                if handle.is_some() {
                    dispatcher.rsp_handle_map.remove(&msg.seq);
                }
                handle
            };
            if let Some(handle) = handle {
                if handle(msg) {
                    trace!("rsp_handle_map.size={}", this.borrow().rsp_handle_map.len());
                } else {
                    error!("may deserialize error");
                }
            } else {
                debug!("no rsp for seq:{}", msg.seq);
            }
        } else {
            error!("unknown type");
        }
    }

    pub fn set_timer_impl<F>(&mut self, timer_impl: F)
    where
        F: Fn(u32, Box<TimeoutCb>) + 'static,
    {
        self.timer_impl = Some(Rc::new(timer_impl));
    }
}

impl Drop for MsgDispatcher {
    fn drop(&mut self) {
        for (_, pending) in self.rsp_handle_map.drain() {
            if let Some(expired) = pending.expired {
                expired();
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::connection::DefaultConnection;
    use std::cell::Cell;

    #[test]
    fn old_timer_cannot_remove_registration_with_reused_sequence() {
        let conn = DefaultConnection::new();
        let dispatcher = MsgDispatcher::new(conn.clone());
        let timers = Rc::new(RefCell::new(Vec::<Box<TimeoutCb>>::new()));
        let copy = timers.clone();
        dispatcher
            .borrow_mut()
            .set_timer_impl(move |_, cb| copy.borrow_mut().push(cb));
        let resets = Rc::new(Cell::new(0));
        let reset_copy = resets.clone();
        let old_timeouts = Rc::new(Cell::new(0));
        let old_copy = old_timeouts.clone();
        MsgDispatcher::subscribe_rsp(
            &dispatcher,
            7,
            Rc::new(|_| true),
            Some(Rc::new(move || old_copy.set(old_copy.get() + 1))),
            10,
            None,
            Some(Rc::new(move || reset_copy.set(reset_copy.get() + 1))),
        );
        MsgDispatcher::reset_session(&dispatcher);
        let new_timeouts = Rc::new(Cell::new(0));
        let new_copy = new_timeouts.clone();
        MsgDispatcher::subscribe_rsp(
            &dispatcher,
            7,
            Rc::new(|_| true),
            Some(Rc::new(move || new_copy.set(new_copy.get() + 1))),
            10,
            None,
            None,
        );
        (timers.borrow()[0])();
        assert_eq!(resets.get(), 1);
        assert_eq!(old_timeouts.get(), 0);
        assert_eq!(new_timeouts.get(), 0);
        (timers.borrow()[1])();
        assert_eq!(new_timeouts.get(), 1);
    }
}
