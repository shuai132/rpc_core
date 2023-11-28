use std::cell::UnsafeCell;
use std::rc::{Rc, Weak};
use std::sync::Arc;

pub struct SharedPtr<T> {
    value: Option<Rc<*mut T>>,
}

impl<T> SharedPtr<T> {
    pub fn new() -> SharedPtr<T> {
        Self {
            value: None,
        }
    }

    pub fn from_box(value: &Box<T>) -> SharedPtr<T> {
        let mut this = Self::new();
        this.init_from_box(value);
        this
    }
}

impl<T> SharedPtr<T> {
    pub fn init_from_box(&mut self, value: &Box<T>) {
        let raw_ptr = unsafe { &*(value.as_ref() as *const T as *const UnsafeCell<T>) }.get();
        self.value = Some(Rc::new(raw_ptr));
    }

    pub fn downgrade(&self) -> WeakPtr<T> {
        match &self.value {
            Some(ptr) => {
                WeakPtr::<T> {
                    value: Rc::downgrade(ptr),
                }
            }
            None => panic!("SharedPtr not init"),
        }
    }
}

pub struct WeakPtr<T> {
    value: Weak<*mut T>,
}

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
            None => panic!("WeakPtr expired"),
        }
    }

    pub fn clone(&self) -> WeakPtr<T> {
        Self {
            value: self.value.clone()
        }
    }
}


pub struct SharedPtrSync<T> {
    value: Option<Arc<*mut T>>,
}

impl<T> SharedPtrSync<T> {
    pub fn new() -> SharedPtrSync<T> {
        Self {
            value: None,
        }
    }

    pub fn from_box(value: &Box<T>) -> SharedPtrSync<T> {
        let mut this = Self::new();
        this.init_from_box(value);
        this
    }
}

impl<T> SharedPtrSync<T> {
    pub fn init_from_box(&mut self, value: &Box<T>) {
        let raw_ptr = unsafe { &*(value.as_ref() as *const T as *const UnsafeCell<T>) }.get();
        self.value = Some(Arc::new(raw_ptr));
    }

    pub fn downgrade(&self) -> WeakPtrSync<T> {
        match &self.value {
            Some(ptr) => {
                WeakPtrSync::<T> {
                    value: Arc::downgrade(ptr),
                }
            }
            None => panic!("SharedPtrSync not init"),
        }
    }
}

pub struct WeakPtrSync<T> {
    value: std::sync::Weak<*mut T>,
}

unsafe impl<T> Send for WeakPtrSync<T> {}

unsafe impl<T> Sync for WeakPtrSync<T> {}

impl<T> WeakPtrSync<T> {
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
            None => panic!("WeakPtrSync expired"),
        }
    }

    pub fn clone(&self) -> WeakPtrSync<T> {
        Self {
            value: self.value.clone()
        }
    }
}

