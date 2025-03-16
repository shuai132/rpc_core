use std::cell::RefCell;
use std::rc::Rc;
use std::time::Duration;

use crate::request::{FinallyType, Request};
use crate::rpc::Rpc;

// Each transport owns its heartbeat, so old ping failures cannot close a new connection.
pub(crate) struct Heartbeat {
    task: tokio::task::JoinHandle<()>,
    pending: Rc<RefCell<Option<Rc<Request>>>>,
}

impl Heartbeat {
    pub(crate) fn start(
        rpc: &Rc<Rpc>,
        interval_ms: u32,
        timeout_ms: u32,
        close: impl Fn() + 'static,
    ) -> Option<Self> {
        if interval_ms == 0 {
            return None;
        }
        let weak = Rc::downgrade(rpc);
        let pending = Rc::new(RefCell::new(None));
        let current = pending.clone();
        let task = tokio::task::spawn_local(async move {
            loop {
                tokio::time::sleep(Duration::from_millis(interval_ms.into())).await;
                let request = {
                    let Some(rpc) = weak.upgrade() else {
                        return;
                    };
                    if !rpc.is_ready() {
                        return;
                    }
                    rpc.ping()
                };
                request.msg(()).timeout_ms(timeout_ms);
                *current.borrow_mut() = Some(request.clone());
                let result = request.future::<()>().await;
                current.borrow_mut().take();
                match result.type_ {
                    FinallyType::Normal | FinallyType::SessionReset => {}
                    _ => {
                        close();
                        return;
                    }
                }
            }
        });
        Some(Self { task, pending })
    }
}

impl Drop for Heartbeat {
    fn drop(&mut self) {
        self.task.abort();
        let request = self.pending.borrow_mut().take();
        if let Some(request) = request {
            request.cancel();
        }
    }
}
