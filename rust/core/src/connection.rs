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
    send_package_impl: Option<RefCell<Box<dyn Fn(Vec<u8>)>>>,
    recv_package_impl: Option<RefCell<Box<dyn Fn(Vec<u8>)>>>,
}

impl DefaultConnection {
    pub fn create() -> Rc<RefCell<Self>> {
        Rc::new(RefCell::from(DefaultConnection::default()))
    }
}

impl Connection for DefaultConnection {
    fn set_send_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.send_package_impl = Some(RefCell::new(handle));
    }

    fn send_package(&self, package: Vec<u8>) {
        if let Some(handle) = self.send_package_impl.as_ref() {
            handle.borrow()(package);
        }
    }

    fn set_recv_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.recv_package_impl = Some(RefCell::new(handle));
    }

    fn on_recv_package(&self, package: Vec<u8>) {
        if let Some(handle) = self.recv_package_impl.as_ref() {
            handle.borrow()(package);
        }
    }
}

pub struct LoopbackConnection(DefaultConnection);

impl LoopbackConnection {
    pub fn create() -> Rc<RefCell<Self>> {
        let c = Rc::new(RefCell::new(LoopbackConnection(DefaultConnection::default())));
        let c2 = c.clone();
        c.borrow_mut().0.send_package_impl = Some(RefCell::new(
            Box::new(move |package: Vec<u8>| {
                c2.borrow().0.on_recv_package(package);
            }) as Box<dyn Fn(Vec<u8>) + 'static>
        ));
        c
    }
}

impl Connection for LoopbackConnection {
    fn set_send_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.0.set_send_package_impl(handle);
    }

    fn send_package(&self, package: Vec<u8>) {
        self.0.send_package(package);
    }

    fn set_recv_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.0.set_recv_package_impl(handle);
    }

    fn on_recv_package(&self, package: Vec<u8>) {
        self.0.on_recv_package(package);
    }
}
