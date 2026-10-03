use std::collections::HashSet;
use std::rc::{Rc, Weak};

use crate::request::Request;

#[derive(Default)]
pub struct Dispose {
    requests: Vec<Weak<Request>>,
    adds_until_prune: usize,
}

impl Dispose {
    pub fn new() -> Dispose {
        Dispose::default()
    }

    pub fn dismiss(&mut self) {
        let pending = std::mem::take(&mut self.requests);
        self.adds_until_prune = 0;
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn long_lived_group_reclaims_expired_and_duplicate_registrations() {
        let mut group = Dispose::new();
        let live = Request::new();
        let removed = Request::new();
        group.add(&removed);
        group.remove(&removed);
        for _ in 0..10_000 {
            group.add(&Request::new());
            group.add(&live);
        }
        // Registration storage must follow live requests, not total historical calls.
        assert!(group.requests.len() < 256);
        assert!(!live.is_canceled() && !removed.is_canceled());
        group.dismiss();
        assert!(live.is_canceled() && !removed.is_canceled());
        assert!(group.requests.is_empty());
    }
}

impl Drop for Dispose {
    fn drop(&mut self) {
        self.dismiss();
    }
}

impl Dispose {
    pub fn add(&mut self, request: &Rc<Request>) {
        if self.adds_until_prune == 0 {
            // Charge each scan to additions since the last scan, rather than
            // rescanning the entire group for every request.
            let mut seen = HashSet::new();
            self.requests
                .retain(|item| item.strong_count() != 0 && seen.insert(item.as_ptr()));
            self.adds_until_prune = self.requests.len().max(64);
        }
        self.adds_until_prune -= 1;
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
        self.adds_until_prune = self.requests.len().max(64);
    }
}
