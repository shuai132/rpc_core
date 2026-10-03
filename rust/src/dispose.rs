use std::collections::HashSet;
use std::rc::{Rc, Weak};

use crate::request::Request;

#[derive(Default)]
pub struct Dispose {
    requests: Vec<Weak<Request>>,
}

impl Dispose {
    pub fn new() -> Dispose {
        Dispose::default()
    }

    pub fn dismiss(&mut self) {
        let pending = std::mem::take(&mut self.requests);
        let mut canceled = HashSet::new();
        let mut failure = None;
        for item in pending {
            if let Some(request) = item.upgrade() {
                if canceled.insert(Rc::as_ptr(&request)) {
                    if let Err(error) =
                        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                            request.cancel();
                        }))
                    {
                        // Finish the detached batch before propagating a user callback failure.
                        if failure.is_none() {
                            failure = Some(error);
                        }
                    }
                }
            }
        }
        if let Some(error) = failure {
            std::panic::resume_unwind(error);
        }
    }
}

impl Drop for Dispose {
    fn drop(&mut self) {
        self.dismiss();
    }
}

impl Dispose {
    pub fn add(&mut self, request: &Rc<Request>) {
        self.requests.push(Rc::downgrade(request));
    }

    pub fn remove(&mut self, request: &Rc<Request>) {
        self.requests.retain(|r| {
            !if let Some(r) = r.upgrade() {
                Rc::ptr_eq(&r, request)
            } else {
                true
            }
        });
    }
}
