use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::{Rc, Weak};

use log::{debug, error, trace, warn};

use crate::connection::Connection;
use crate::detail::coder;
use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::type_def::{CmdType, SeqType};

pub type TimeoutCb = dyn Fn();
pub type TimerImpl = dyn Fn(u32, Box<TimeoutCb>);

type CmdHandle = Rc<dyn Fn(MsgWrapper) -> Option<MsgWrapper>>;
pub type RspHandle = dyn Fn(MsgWrapper) -> bool;

pub struct MsgDispatcher {
    conn: Weak<RefCell<dyn Connection>>,
    cmd_handle_map: HashMap<CmdType, CmdHandle>,
    rsp_handle_map: HashMap<SeqType, (Rc<RspHandle>, Option<Rc<TimeoutCb>>)>,
    timer_impl: Option<Rc<TimerImpl>>,
    this: Weak<RefCell<Self>>,
}

impl MsgDispatcher {
    pub fn new(conn: Rc<RefCell<dyn Connection>>) -> Rc<RefCell<Self>> {
        Rc::<RefCell<Self>>::new_cyclic(|this_weak| {
            let dispatcher = RefCell::new(Self {
                conn: Rc::downgrade(&conn),
                cmd_handle_map: HashMap::new(),
                rsp_handle_map: HashMap::new(),
                timer_impl: None,
                this: this_weak.clone(),
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
        &mut self,
        seq: SeqType,
        rsp_handle: Rc<RspHandle>,
        timeout_cb: Option<Rc<TimeoutCb>>,
        timeout_ms: u32,
        expired: Option<Rc<TimeoutCb>>,
    ) {
        self.rsp_handle_map.insert(seq, (rsp_handle, expired));
        if let Some(timer_impl) = &self.timer_impl {
            let this_weak = self.this.clone();
            timer_impl(
                timeout_ms,
                Box::new(move || {
                    let Some(this) = this_weak.upgrade() else {
                        debug!("seq:{} timeout after destroy", seq);
                        return;
                    };

                    let removed = this.borrow_mut().rsp_handle_map.remove(&seq).is_some();
                    if removed {
                        if let Some(timeout_cb) = &timeout_cb {
                            timeout_cb();
                        }
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

    fn send_response(this: &Rc<RefCell<Self>>, msg: &MsgWrapper) {
        let Ok(payload) = coder::serialize(msg) else {
            error!("response serialization failed");
            return;
        };
        let conn = this.borrow().conn.upgrade();
        if let Some(conn) = conn {
            conn.borrow().send_package(payload);
        }
    }

    pub fn dispatch(this: &Rc<RefCell<Self>>, mut msg: MsgWrapper) {
        if msg.type_.contains(MsgType::Command) {
            // ping
            let is_ping = msg.type_.contains(MsgType::Ping);
            if is_ping {
                debug!("<= seq:{} type:ping", msg.seq);
                msg.type_ = MsgType::Response | MsgType::Pong;
                debug!("=> seq:{} type:pong", msg.seq);
                Self::send_response(this, &msg);
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
                        Self::send_response(this, &rsp);
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
                    Self::send_response(this, &rsp);
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
            let handle = this.borrow_mut().rsp_handle_map.remove(&msg.seq);
            if let Some((handle, _)) = handle {
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
        for (_, (_, expired)) in self.rsp_handle_map.drain() {
            if let Some(expired) = expired {
                expired();
            }
        }
    }
}
