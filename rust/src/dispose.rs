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
        for item in &self.requests {
            if let Some(request) = item.upgrade() {
                request.cancel();
            }
        }
        self.requests.clear();
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
