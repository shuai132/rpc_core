use std::rc::{Rc, Weak};

#[derive(Clone)]
pub struct SharedPtr<T> {
    value: Option<Rc<*mut T>>,
}

impl<T> SharedPtr<T> {
    pub fn new() -> SharedPtr<T> {
        Self {
            value: None,
        }
    }

    pub fn from_box(value: &mut Box<T>) -> SharedPtr<T> {
        let mut this = Self::new();
        this.init_from_box(value);
        this
    }
}

impl<T> SharedPtr<T> {
    pub fn init_from_box(&mut self, value: &mut Box<T>) {
        self.value = Some(Rc::new(value.as_mut()));
    }

    pub fn downgrade(&self) -> WeakPtr<T> {
        WeakPtr::<T> {
            value: Rc::downgrade(self.value.as_ref().unwrap()),
        }
    }
}

#[derive(Clone)]
pub struct WeakPtr<T> {
    value: Weak<*mut T>,
}

unsafe impl<T> Send for WeakPtr<T> {}

unsafe impl<T> Sync for WeakPtr<T> {}

impl<T> WeakPtr<T> {
    pub fn expired(&self) -> bool {
        self.value.upgrade().is_none()
    }

    pub fn upgrade(&self) -> Option<&mut T> {
        match self.value.upgrade() {
            Some(ptr) => { Some(unsafe { &mut **ptr }) }
            None => None,
        }
    }

    pub fn unwrap(&self) -> &mut T {
        match self.value.upgrade() {
            Some(ptr) => { unsafe { &mut **ptr } }
            None => panic!("called `WeakPtr::unwrap()` on a `None` value"),
        }
    }

    pub fn clone(&self) -> WeakPtr<T> {
        Self {
            value: Rc::downgrade(&self.value.upgrade().unwrap()),
        }
    }
}

