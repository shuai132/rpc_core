use std::cell::{Cell, RefCell};
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::sync::Arc;
use std::task::{Context, Poll, Wake, Waker};

use rpc_core::connection::{Connection, DefaultConnection, LoopbackConnection};
use rpc_core::request::FinallyType;
use rpc_core::rpc::Rpc;

struct NoopWake;
impl Wake for NoopWake {
    fn wake(self: Arc<Self>) {}
}
fn poll_once<F: Future>(future: Pin<&mut F>) -> Poll<F::Output> {
    future.poll(&mut Context::from_waker(&Waker::from(Arc::new(NoopWake))))
}

#[test]
fn reset_completes_future_preserves_readiness_and_releases_request() {
    for ready in [false, true] {
        let rpc = Rpc::new(None);
        rpc.get_connection()
            .borrow_mut()
            .set_send_package_impl(Box::new(|_| true));
        rpc.set_ready(true);
        let request = rpc.cmd("pending");
        let mut future = Box::pin(request.future::<String>());
        assert!(poll_once(future.as_mut()).is_pending());
        rpc.set_ready(ready);
        rpc.reset_session();
        let Poll::Ready(result) = poll_once(future.as_mut()) else {
            panic!("reset request stayed pending");
        };
        assert_eq!(result.type_, FinallyType::SessionReset);
        assert!(result.result.is_none());
        drop(future);
        let weak = Rc::downgrade(&request);
        drop(request);
        assert!(weak.upgrade().is_none());
        assert_eq!(rpc.is_ready(), ready);
    }
}

#[test]
fn reconnect_keeps_pending_requests_but_reset_ends_them() {
    let rpc = Rpc::new(None);
    rpc.get_connection()
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    rpc.set_ready(true);
    let timers = Rc::new(RefCell::new(Vec::<Box<dyn Fn()>>::new()));
    let copy = timers.clone();
    rpc.set_timer(move |_, cb| copy.borrow_mut().push(cb));
    let completed = Rc::new(RefCell::new(Vec::new()));
    let copy = completed.clone();
    let request = rpc.cmd("pending");
    request
        .rsp(|_: String| panic!("unexpected response"))
        .finally(move |status| copy.borrow_mut().push(status));
    assert_eq!(request.call(), Ok(()));
    rpc.set_ready(false);
    rpc.set_ready(true);
    assert!(completed.borrow().is_empty());
    rpc.reset_session();
    assert_eq!(*completed.borrow(), vec![FinallyType::SessionReset]);
    (timers.borrow()[0])();
    assert_eq!(completed.borrow().len(), 1);
}

#[test]
fn reset_completion_can_start_a_new_call_and_old_timer_cannot_finish_it() {
    let rpc = Rpc::new(None);
    rpc.get_connection()
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    rpc.set_ready(true);
    let timers = Rc::new(RefCell::new(Vec::<Box<dyn Fn()>>::new()));
    let copy = timers.clone();
    rpc.set_timer(move |_, cb| copy.borrow_mut().push(cb));
    let request = rpc.cmd("pending");
    let weak = Rc::downgrade(&request);
    let completed = Rc::new(RefCell::new(Vec::new()));
    let copy = completed.clone();
    request
        .rsp(|_: String| panic!("unexpected response"))
        .finally(move |status| {
            copy.borrow_mut().push(status.clone());
            if status == FinallyType::SessionReset {
                assert_eq!(weak.upgrade().unwrap().call(), Ok(()));
            }
        });
    assert_eq!(request.call(), Ok(()));
    rpc.reset_session();
    assert_eq!(timers.borrow().len(), 2);
    (timers.borrow()[0])();
    assert_eq!(*completed.borrow(), vec![FinallyType::SessionReset]);
    (timers.borrow()[1])();
    assert_eq!(
        *completed.borrow(),
        vec![FinallyType::SessionReset, FinallyType::Timeout]
    );
}

#[test]
fn reset_inside_handler_suppresses_old_response_and_retains_subscription() {
    let (server_conn, client_conn) = LoopbackConnection::new();
    let server = Rpc::new(Some(server_conn));
    let client = Rpc::new(Some(client_conn));
    server.set_ready(true);
    client.set_ready(true);
    let weak_server = Rc::downgrade(&server);
    let calls = Rc::new(Cell::new(0));
    let copy = calls.clone();
    server.subscribe("echo", move |value: String| {
        copy.set(copy.get() + 1);
        if copy.get() == 1 {
            weak_server.upgrade().unwrap().reset_session();
        }
        value
    });
    let responses = Rc::new(Cell::new(0));
    let copy = responses.clone();
    let request = client.cmd("echo");
    request.msg("ok").rsp(move |value: String| {
        assert_eq!(value, "ok");
        copy.set(copy.get() + 1);
    });
    assert_eq!(request.call(), Ok(()));
    assert_eq!(calls.get(), 1);
    assert_eq!(responses.get(), 0);
    request.cancel().reset_cancel();
    assert_eq!(request.call(), Ok(()));
    assert_eq!(calls.get(), 2);
    assert_eq!(responses.get(), 1);
}

#[test]
fn reconnect_during_handler_allows_response_in_same_session() {
    let (server_conn, client_conn) = LoopbackConnection::new();
    let server = Rpc::new(Some(server_conn));
    let client = Rpc::new(Some(client_conn));
    server.set_ready(true);
    client.set_ready(true);
    let weak_server = Rc::downgrade(&server);
    server.subscribe("echo", move |value: String| {
        let server = weak_server.upgrade().unwrap();
        server.set_ready(false);
        server.set_ready(true);
        value
    });
    let request = client.cmd("echo");
    request.msg("ok");
    let mut future = Box::pin(request.future::<String>());
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("same-session response was lost");
    };
    assert_eq!(result.unwrap(), "ok");
}

struct RejectConnection(DefaultConnection);
impl Connection for RejectConnection {
    fn set_send_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>) -> bool>) {
        self.0.set_send_package_impl(handle);
    }
    fn send_package(&self, _: Vec<u8>) -> bool {
        false
    }
    fn set_recv_package_impl(&mut self, handle: Box<dyn Fn(Vec<u8>)>) {
        self.0.set_recv_package_impl(handle);
    }
    fn on_recv_package(&self, data: Vec<u8>) {
        self.0.on_recv_package(data);
    }
}

#[test]
fn missing_sender_rejects_request() {
    let rpc = Rpc::new(None);
    rpc.set_ready(true);
    let request = rpc.cmd("pending");
    assert_eq!(request.call(), Err(FinallyType::RpcNotReady));
}

#[test]
fn transport_rejection_completes_request_and_removes_timeout() {
    let conn = Rc::new(RefCell::new(RejectConnection(DefaultConnection::default())));
    let rpc = Rpc::new(Some(conn));
    rpc.set_ready(true);
    let timer = Rc::new(RefCell::new(None::<Box<dyn Fn()>>));
    let copy = timer.clone();
    rpc.set_timer(move |_, cb| *copy.borrow_mut() = Some(cb));
    let request = rpc.cmd("pending");
    let mut future = Box::pin(request.future::<String>());
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("rejected send stayed pending");
    };
    assert_eq!(result.type_, FinallyType::RpcNotReady);
    timer.borrow().as_ref().unwrap()();
    drop(future);
    let weak = Rc::downgrade(&request);
    drop(request);
    assert!(weak.upgrade().is_none());
}

#[test]
fn reset_during_timeout_can_reuse_request() {
    for retries in [0, 1, -1] {
        let rpc = Rpc::new(None);
        let sent = Rc::new(Cell::new(0));
        let copy = sent.clone();
        rpc.get_connection()
            .borrow_mut()
            .set_send_package_impl(Box::new(move |_| {
                copy.set(copy.get() + 1);
                true
            }));
        rpc.set_ready(true);
        let timers = Rc::new(RefCell::new(Vec::<Rc<dyn Fn()>>::new()));
        let copy = timers.clone();
        rpc.set_timer(move |_, cb| copy.borrow_mut().push(Rc::from(cb)));
        let finished = Rc::new(RefCell::new(Vec::new()));
        let copy = finished.clone();
        let request = rpc.cmd("x");
        request
            .rsp(|_: String| panic!("late response"))
            .retry(retries)
            .finally(move |status| copy.borrow_mut().push(status));
        let weak_rpc = Rc::downgrade(&rpc);
        let weak_request = Rc::downgrade(&request);
        let copy = timers.clone();
        let completed = finished.clone();
        request.timeout(move || {
            (copy.borrow()[0])(); // Reentering the expired timer must do nothing.
            let rpc = weak_rpc.upgrade().unwrap();
            rpc.get_connection()
                .borrow()
                .on_recv_package(b"\0\0\0\0\0\0\x02\"late\"".to_vec());
            rpc.reset_session();
            assert_eq!(*completed.borrow(), vec![FinallyType::SessionReset]);
            let request = weak_request.upgrade().unwrap();
            request.timeout(|| {}).retry(0).call().unwrap();
        });
        request.call().unwrap();
        let first = timers.borrow()[0].clone();
        first();
        assert_eq!(sent.get(), 2);
        assert_eq!(timers.borrow().len(), 2);
        assert_eq!(*finished.borrow(), vec![FinallyType::SessionReset]);
        first();
        let observer = Rc::downgrade(&request);
        drop(request);
        (timers.borrow()[1])();
        assert_eq!(
            *finished.borrow(),
            vec![FinallyType::SessionReset, FinallyType::Timeout]
        );
        assert!(observer.upgrade().is_none());
    }
}
