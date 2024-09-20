use std::cell::RefCell;
use std::rc::Rc;

pub trait Connection {
    fn set_send_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>);
    fn send_package(&self, package: Vec<u8>);
    fn set_recv_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>);
    fn on_recv_package(&self, package: Vec<u8>);
}

#[derive(Default)]
pub struct DefaultConnection {
    send_package_impl: Option<Box<dyn Fn(Vec<u8>)>>,
    recv_package_impl: Option<Box<dyn Fn(Vec<u8>)>>,
}

impl DefaultConnection {
    pub fn new() -> Rc<RefCell<Self>> {
        Rc::new(RefCell::from(DefaultConnection::default()))
    }
}

impl Connection for DefaultConnection {
    fn set_send_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.send_package_impl = Some(handle);
    }

    fn send_package(&self, package: Vec<u8>) {
        if let Some(handle) = self.send_package_impl.as_ref() {
            handle(package);
        }
    }

    fn set_recv_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.recv_package_impl = Some(handle);
    }

    fn on_recv_package(&self, package: Vec<u8>) {
        if let Some(handle) = self.recv_package_impl.as_ref() {
            handle(package);
        }
    }
}

pub struct LoopbackConnection;

impl LoopbackConnection {
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> (
        Rc<RefCell<DefaultConnection>>,
        Rc<RefCell<DefaultConnection>>,
    ) {
        let c1 = Rc::new(RefCell::new(DefaultConnection::default()));
        let c1_weak = Rc::downgrade(&c1);
        let c2 = Rc::new(RefCell::new(DefaultConnection::default()));
        let c2_weak = Rc::downgrade(&c2);

        c1.borrow_mut().send_package_impl = Some(Box::new(move |package: Vec<u8>| {
            c2_weak.upgrade().unwrap().borrow().on_recv_package(package);
        }));
        c2.borrow_mut().send_package_impl = Some(Box::new(move |package: Vec<u8>| {
            c1_weak.upgrade().unwrap().borrow().on_recv_package(package);
        }));
        (c1, c2)
    }
}
