use std::cell::UnsafeCell;
use std::rc::{Rc, Weak};

pub struct UnsafeThis<T> {
    value: Option<Rc<*mut T>>,
}

impl<T> UnsafeThis<T> {
    pub fn new() -> UnsafeThis<T> {
        Self {
            value: None,
        }
    }

    pub fn from_box(value: &Box<T>) -> UnsafeThis<T> {
        let mut this = Self::new();
        this.init_from_box(value);
        this
    }
}

impl<T> UnsafeThis<T> {
    pub fn init_from_box(&mut self, value: &Box<T>) {
        let raw_ptr = unsafe { &*(value.as_ref() as *const T as *const UnsafeCell<T>) }.get();
        self.value = Some(Rc::new(raw_ptr));
    }

    pub fn downgrade(&self) -> WeakThis<T> {
        match &self.value {
            Some(ptr) => {
                WeakThis::<T> {
                    value: Rc::downgrade(ptr),
                }
            }
            None => panic!("UnsafeThis not init"),
        }
    }
}

pub struct WeakThis<T> {
    value: Weak<*mut T>,
}

impl<T> WeakThis<T> {
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
            None => panic!("WeakThis expired"),
        }
    }

    pub fn clone(&self) -> WeakThis<T> {
        Self {
            value: self.value.clone()
        }
    }
}
