use std::rc::Rc;

use crate::request::{DisposeProto, Request};

struct Dispose {}

impl DisposeProto for Dispose {
    fn add(&mut self, _: Rc<Request>) {
        todo!()
    }

    fn remove(&mut self, _: Rc<Request>) {
        todo!()
    }
}
