use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::{Rc, Weak};

use log::{debug, error, trace, warn};

use crate::connection::Connection;
use crate::detail::coder;
use crate::detail::msg_wrapper::{MsgType, MsgWrapper};
use crate::type_def::{CmdType, SeqType};

pub(crate) type TimeoutCb = dyn Fn();
pub(crate) type TimerImpl = dyn Fn(u32, Box<TimeoutCb>);

type CmdHandle = Box<dyn Fn(MsgWrapper) -> Option<MsgWrapper>>;
pub(crate) type RspHandle = dyn Fn(MsgWrapper) -> bool;

pub(crate) struct MsgDispatcher {
    conn: Weak<RefCell<dyn Connection>>,
    cmd_handle_map: HashMap<CmdType, CmdHandle>,
    rsp_handle_map: HashMap<SeqType, Rc<RspHandle>>,
    timer_impl: Option<Rc<TimerImpl>>,
    is_alive: Rc<bool>,
}

impl MsgDispatcher {
    pub(crate) fn create(conn: Option<Rc<RefCell<dyn Connection>>>) -> Box<Self> {
        let dispatcher = Box::new(Self {
            conn: Rc::downgrade(&conn.unwrap()),
            cmd_handle_map: HashMap::new(),
            rsp_handle_map: HashMap::new(),
            timer_impl: None,
            is_alive: Rc::new(true),
        });

        let conn = dispatcher.conn.upgrade().unwrap();
        let alive = Rc::downgrade(&dispatcher.is_alive);
        let dispatcher_ptr = Box::into_raw(dispatcher);
        conn.borrow_mut().set_recv_package_impl(Box::new(
            move |payload| {
                if alive.upgrade().is_none() {
                    return;
                }
                if let Some(msg) = coder::deserialize(&payload) {
                    let dispatcher = unsafe { &mut *dispatcher_ptr };
                    dispatcher.dispatch(msg);
                } else {
                    eprintln!("deserialize error");
                }
            }
        ));
        unsafe { Box::from_raw(dispatcher_ptr) }
    }
}

impl MsgDispatcher {
    pub(crate) fn subscribe_cmd(&mut self, cmd: String, handle: CmdHandle) {
        self.cmd_handle_map.insert(cmd, handle);
    }

    pub(crate) fn unsubscribe_cmd(&mut self, cmd: String) {
        if let Some(_) = self.cmd_handle_map.remove(&cmd) {
            debug!("erase cmd: {}", cmd);
        } else {
            debug!("not subscribe cmd for: {}", cmd);
        }
    }

    pub(crate) fn subscribe_rsp(&mut self, seq: SeqType, rsp_handle: Rc<RspHandle>, timeout_cb: Option<Rc<TimeoutCb>>, timeout_ms: u32) {
        self.rsp_handle_map.insert(seq, rsp_handle);
        if let Some(timer_impl) = &self.timer_impl {
            let alive = Rc::downgrade(&self.is_alive);
            let self_ = self as *const _ as *mut Self;
            timer_impl(timeout_ms, Box::new(move || {
                if alive.upgrade().is_none() {
                    debug!("seq:{} timeout after destroy", seq);
                    return;
                }
                unsafe {
                    let self_ = &mut *self_;
                    if let Some(_) = (*self_).rsp_handle_map.remove(&seq) {
                        if let Some(timeout_cb) = &timeout_cb {
                            timeout_cb();
                        }
                        trace!("Timeout seq={}, rsp_handle_map.size={}", seq, (*self_).rsp_handle_map.len());
                    }
                }
            }));
        } else {
            warn!("no timeout will cause memory leak!");
        }
    }

    pub(crate) fn dispatch(&mut self, mut msg: MsgWrapper) {
        if msg.type_.contains(MsgType::Command) {
            // ping
            let is_ping = msg.type_.contains(MsgType::Ping);
            if is_ping {
                debug!("<= seq:{} type:ping", &msg.seq);
                msg.type_ = MsgType::Response | MsgType::Pong;
                debug!("=> seq:{} type:pong", &msg.seq);
                self.conn.upgrade().unwrap().borrow().send_package(coder::serialize(&msg));
                return;
            }

            // command
            debug!("<= seq:{} cmd:{}", &msg.seq, &msg.cmd);
            let cmd = &msg.cmd;
            if let Some(handle) = self.cmd_handle_map.get(cmd) {
                let need_rsp = msg.type_.contains(MsgType::NeedRsp);
                let resp = handle(msg);
                if need_rsp && resp.is_some() {
                    let rsp = resp.unwrap();
                    debug!("=> seq:{} type:rsp", &rsp.seq);
                    self.conn.upgrade().unwrap().borrow().send_package(coder::serialize(&rsp));
                }
            } else {
                debug!("not subscribe cmd for: {}", cmd);
                let need_rsp = msg.type_.contains(MsgType::NeedRsp);
                if need_rsp {
                    debug!("=> seq:{} type:rsp", msg.seq);
                    let mut rsp = MsgWrapper::new();
                    rsp.seq = msg.seq;
                    rsp.type_ = MsgType::Response | MsgType::NoSuchCmd;
                    self.conn.upgrade().unwrap().borrow().send_package(coder::serialize(&rsp));
                }
            }
        } else if msg.type_.contains(MsgType::Response) {
            // pong or response
            debug!("<= seq:{} type:{}", msg.seq, if msg.type_.contains(MsgType::Pong) { "pong" } else {"rsp"});
            if let Some(handle) = self.rsp_handle_map.remove(&msg.seq) {
                if handle(msg) {
                    trace!("rsp_handle_map.size={}", self.rsp_handle_map.len());
                } else {
                    error!("may deserialize error");
                }
            } else {
                debug!("no rsp for seq:{}", msg.seq);
                return;
            }
        } else {
            error!("unknown type");
        }
    }

    pub(crate) fn set_timer_impl<F>(&mut self, timer_impl: F)
        where F: Fn(u32, Box<TimeoutCb>) + 'static
    {
        self.timer_impl = Some(Rc::new(timer_impl));
    }
}
