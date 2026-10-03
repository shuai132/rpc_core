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

struct PendingScope<'a> {
    dispatcher: &'a RefCell<MsgDispatcher>,
    seq: SeqType,
    registration: u64,
}
impl Drop for PendingScope<'_> {
    fn drop(&mut self) {
        let removed = {
            let mut dispatcher = self.dispatcher.borrow_mut();
            if dispatcher
                .rsp_handle_map
                .get(&self.seq)
                .is_some_and(|pending| pending.registration == self.registration)
            {
                dispatcher.rsp_handle_map.remove(&self.seq)
            } else {
                None
            }
        };
        drop(removed);
    }
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
        let mut failure = None;
        for (_, pending) in previous {
            if let Some(reset) = pending.reset {
                if let Err(error) =
                    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| reset()))
                {
                    // Every detached call must complete even if a user completion panics.
                    if failure.is_none() {
                        failure = Some(error);
                    }
                }
            }
        }
        if let Some(error) = failure {
            std::panic::resume_unwind(error);
        }
    }

    // Return removed callables so their captures can be dropped after releasing borrows.
    pub fn subscribe_cmd(&mut self, cmd: String, handle: CmdHandle) -> Option<CmdHandle> {
        self.cmd_handle_map.insert(cmd, handle)
    }

    pub fn unsubscribe_cmd(&mut self, cmd: String) -> Option<CmdHandle> {
        let previous = self.cmd_handle_map.remove(&cmd);
        if previous.is_some() {
            debug!("erase cmd: {}", cmd);
        } else {
            debug!("not subscribe cmd for: {}", cmd);
        }
        previous
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
                        let scope = PendingScope {
                            dispatcher: &this,
                            seq,
                            registration,
                        };
                        if let Some(timeout_cb) = &timeout_cb {
                            timeout_cb();
                        }
                        drop(scope);
                        trace!(
                            "Timeout seq={}, rsp_handle_map.size={}",
                            seq,
                            this.borrow().rsp_handle_map.len()
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
        let direction = msg.type_.bits() & (MsgType::Command | MsgType::Response).bits();
        if direction == MsgType::Command.bits() {
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
        } else if direction == MsgType::Response.bits() {
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
            let pending = {
                let mut dispatcher = this.borrow_mut();
                dispatcher
                    .rsp_handle_map
                    .get_mut(&msg.seq)
                    .and_then(|pending| {
                        pending
                            .handle
                            .take()
                            .map(|handle| (handle, pending.registration))
                    })
            };
            if let Some((handle, registration)) = pending {
                // Keep the call visible to reset_session() during custom decoding.
                // Cleanup on unwind must not remove a newer registration.
                let scope = PendingScope {
                    dispatcher: this,
                    seq: msg.seq,
                    registration,
                };
                let handled = handle(msg);
                drop(scope);
                if handled {
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

    pub fn set_timer_impl<F>(&mut self, timer_impl: F) -> Option<Rc<TimerImpl>>
    where
        F: Fn(u32, Box<TimeoutCb>) + 'static,
    {
        self.timer_impl.replace(Rc::new(timer_impl))
    }
}

impl Drop for MsgDispatcher {
    fn drop(&mut self) {
        let mut failure = None;
        for (_, pending) in self.rsp_handle_map.drain() {
            if let Some(expired) = pending.expired {
                if let Err(error) =
                    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| expired()))
                {
                    // Other requests still need completion to release their self-keepers.
                    if failure.is_none() {
                        failure = Some(error);
                    }
                }
            }
        }
        if let Some(error) = failure {
            std::panic::resume_unwind(error);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::connection::DefaultConnection;
    use std::cell::Cell;

    #[test]
    fn callback_unwind_cleans_only_its_registration() {
        for timeout in [false, true] {
            for replace in [false, true] {
                let conn = DefaultConnection::new();
                let dispatcher = MsgDispatcher::new(conn);
                let timers = Rc::new(RefCell::new(Vec::<Rc<TimeoutCb>>::new()));
                let saved = timers.clone();
                dispatcher
                    .borrow_mut()
                    .set_timer_impl(move |_, cb| saved.borrow_mut().push(Rc::from(cb)));
                let resets = Rc::new(Cell::new(0));
                let old_resets = resets.clone();
                let new_resets = resets.clone();
                let weak = Rc::downgrade(&dispatcher);
                let callback = Rc::new(move || {
                    if replace {
                        let resets = new_resets.clone();
                        MsgDispatcher::subscribe_rsp(
                            &weak.upgrade().unwrap(),
                            7,
                            Rc::new(|_| true),
                            None,
                            1,
                            None,
                            Some(Rc::new(move || resets.set(resets.get() + 1))),
                        );
                    }
                    panic!("callback failed");
                });
                let response_callback = callback.clone();
                MsgDispatcher::subscribe_rsp(
                    &dispatcher,
                    7,
                    Rc::new(move |_| {
                        response_callback();
                        true
                    }),
                    Some(callback),
                    1,
                    None,
                    Some(Rc::new(move || old_resets.set(old_resets.get() + 10))),
                );
                let response = MsgWrapper::make_rsp(7, ()).unwrap();
                let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                    if timeout {
                        let fire = timers.borrow()[0].clone();
                        fire();
                    } else {
                        MsgDispatcher::dispatch(&dispatcher, response);
                    }
                }));
                assert!(result.is_err());
                MsgDispatcher::reset_session(&dispatcher);
                assert_eq!(resets.get(), if replace { 1 } else { 0 });
            }
        }
    }

    #[test]
    fn ambiguous_message_directions_do_not_dispatch_or_finish_requests() {
        let conn = DefaultConnection::new();
        let dispatcher = MsgDispatcher::new(conn.clone());
        let commands = Rc::new(Cell::new(0));
        let responses = Rc::new(Cell::new(0));
        let sent = Rc::new(Cell::new(0));
        let copy = sent.clone();
        conn.borrow_mut().set_send_package_impl(Box::new(move |_| {
            copy.set(copy.get() + 1);
            true
        }));
        let copy = commands.clone();
        dispatcher.borrow_mut().subscribe_cmd(
            "cmd".into(),
            Rc::new(move |msg| {
                copy.set(copy.get() + 1);
                Some(MsgWrapper::make_rsp(msg.seq, ()).unwrap())
            }),
        );
        let copy = responses.clone();
        MsgDispatcher::subscribe_rsp(
            &dispatcher,
            7,
            Rc::new(move |_| {
                copy.set(copy.get() + 1);
                true
            }),
            None,
            1000,
            None,
            None,
        );
        let deliver = |type_| {
            let mut message = MsgWrapper::new();
            message.seq = 7;
            message.cmd = "cmd".into();
            message.type_ = type_;
            conn.borrow()
                .on_recv_package(coder::serialize(&message).unwrap());
        };

        // Exercise every combination of known flags with both or neither direction.
        let directions = (MsgType::Command | MsgType::Response).bits();
        for bits in 0..=MsgType::all().bits() {
            let direction = bits & directions;
            if direction == 0 || direction == directions {
                deliver(MsgType::from_bits(bits).unwrap());
                assert_eq!(commands.get(), 0);
                assert_eq!(responses.get(), 0);
                assert_eq!(sent.get(), 0);
                assert!(dispatcher.borrow().rsp_handle_map.contains_key(&7));
            }
        }

        deliver(MsgType::Command | MsgType::NeedRsp);
        assert_eq!(commands.get(), 1);
        assert_eq!(sent.get(), 1);
        deliver(MsgType::Response);
        assert_eq!(responses.get(), 1);
        assert!(!dispatcher.borrow().rsp_handle_map.contains_key(&7));
    }

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
