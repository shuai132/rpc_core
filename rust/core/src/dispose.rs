use std::rc::{Rc, Weak};

use crate::request::{DisposeProto, Request};

pub struct Dispose {
    requests: Vec<Weak<Request>>,
}

impl Dispose {
    pub fn new() -> Dispose {
        Dispose {
            requests: vec![],
        }
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

impl DisposeProto for Dispose {
    fn add(&mut self, request: &Rc<Request>) {
        self.requests.push(Rc::downgrade(&request));
    }

    fn remove(&mut self, request: &Rc<Request>) {
        if let Some(index) = self.requests.iter().position(|x| {
            if x.strong_count() == 0 {
                return true;
            }
            if Rc::ptr_eq(x.upgrade().as_ref().unwrap(), request) {
                return true;
            }
            return false;
        }) {
            self.requests.remove(index);
        }
    }
}
