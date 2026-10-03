use std::cell::RefCell;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::sync::Arc;
use std::task::{Context, Poll, Wake, Waker};

use log::info;

use rpc_core::connection::{Connection, DefaultConnection};
use rpc_core::request::FinallyType;
use rpc_core::rpc::Rpc;

struct NoopWake;

#[test]
fn removing_dispatcher_callbacks_can_cancel_pending_requests() {
    for operation in ["replace command", "unsubscribe", "replace timer"] {
        let f = PendingFixture::new();
        let request = f.rpc.cmd("pending");
        let mut group = rpc_core::dispose::Dispose::new();
        request.add_to(&mut group);
        let finished = Rc::new(RefCell::new(Vec::new()));
        let copy = finished.clone();
        let weak_rpc = Rc::downgrade(&f.rpc);
        request.rsp(|_: String| {}).finally(move |status| {
            copy.borrow_mut().push(status);
            if let Some(rpc) = weak_rpc.upgrade() {
                rpc.set_ready(false);
                rpc.subscribe("after cancel", |_: ()| {});
            }
        });
        if operation == "replace timer" {
            let timers = f.timers.clone();
            f.rpc.set_timer(move |_, cb| {
                let _keep_group = &group;
                timers.borrow_mut().push(Rc::from(cb));
            });
        } else {
            f.rpc.subscribe("owned", move |_: ()| {
                let _keep_group = &group;
            });
        }
        request.call().unwrap();
        match operation {
            "replace command" => f.rpc.subscribe("owned", |_: ()| {}),
            "unsubscribe" => f.rpc.unsubscribe("owned"),
            _ => f.rpc.set_timer(|_, _| {}),
        }
        assert_eq!(*finished.borrow(), vec![FinallyType::Canceled]);
        assert!(!f.rpc.is_ready());
        f.expire(0);
        f.reply(0, "late response");
        assert_eq!(*finished.borrow(), vec![FinallyType::Canceled]);
    }
}

#[test]
fn replacing_callbacks_can_drop_a_dispose_group_for_the_same_request() {
    for callback in ["response", "timeout", "finally"] {
        let request = rpc_core::request::Request::new();
        let mut group = rpc_core::dispose::Dispose::new();
        request.add_to(&mut group);
        match callback {
            "response" => {
                request.rsp(move |_: ()| {
                    let _keep_group = &group;
                });
            }
            "timeout" => {
                request.timeout(move || {
                    let _keep_group = &group;
                });
            }
            _ => {
                request.finally(move |_| {
                    let _keep_group = &group;
                });
            }
        }
        assert!(!request.is_canceled());
        // Releasing the previous callback drops its group, which cancels this request.
        match callback {
            "response" => {
                request.rsp(|_: ()| {});
            }
            "timeout" => {
                request.timeout(|| {});
            }
            _ => {
                request.finally(|_| {});
            }
        }
        assert!(request.is_canceled());
        request.reset_cancel();
        assert!(!request.is_canceled());
    }
}

impl Wake for NoopWake {
    fn wake(self: Arc<Self>) {}
}

fn poll_once<F: Future>(future: Pin<&mut F>) -> Poll<F::Output> {
    let waker = Waker::from(Arc::new(NoopWake));
    future.poll(&mut Context::from_waker(&waker))
}

#[test]
fn rpc() {
    std::env::set_var("RUST_LOG", "trace");
    env_logger::init();

    // loopback connection
    let (connection_s, connection_c) = rpc_core::connection::LoopbackConnection::new();

    // rpc server
    let rpc_s = rpc_core::rpc::Rpc::new(Some(connection_s));
    rpc_s.set_timer(|ms: u32, _: Box<dyn Fn()>| {
        info!("set_timer: {ms}");
    });
    rpc_s.set_ready(true);

    rpc_s.subscribe("cmd", |msg: String| -> String {
        assert_eq!(msg, "hello");
        "world".to_string()
    });

    // rpc client
    let rpc_c = rpc_core::rpc::Rpc::new(Some(connection_c));
    rpc_c.set_timer(|ms: u32, _: Box<dyn Fn()>| {
        info!("set_timer: {ms}");
    });
    rpc_c.set_ready(true);

    // test code
    let pass = Rc::new(RefCell::new(false));
    let pass_clone = pass.clone();
    rpc_c
        .cmd("cmd")
        .msg("hello")
        .rsp(move |msg: String| {
            assert_eq!(msg, "world");
            *pass_clone.borrow_mut() = true;
        })
        .call()
        .unwrap();
    assert!(*pass.borrow());

    info!("--- test unsubscribe ---");
    rpc_s.subscribe("x", |_: ()| {});
    rpc_s.unsubscribe("x");

    info!("--- test ping ---");
    *pass.borrow_mut() = false;
    let pass_clone = pass.clone();
    rpc_c.ping();
    rpc_c
        .ping_msg("hello")
        .rsp(move |msg: String| {
            info!("rsp: {}", msg);
            *pass_clone.borrow_mut() = true;
        })
        .call()
        .unwrap();
    assert!(*pass.borrow());

    info!("--- test request ---");
    {
        let request = rpc_core::request::Request::new();
        let pass = Rc::new(RefCell::new(false));
        let pass_clone = pass.clone();
        request
            .cmd("cmd")
            .msg("hello")
            .rsp(move |msg: String| {
                assert_eq!(msg, "world");
                *pass_clone.borrow_mut() = true;
            })
            .call_with_rpc(rpc_c.clone())
            .unwrap();
        assert!(*pass.borrow());
    }

    info!("--- test dispose ---");
    {
        info!("--- dispose test RAII ---");
        rpc_s.subscribe("cmd", |_: String| -> String {
            assert!(false);
            "".to_string()
        });

        let pass = Rc::new(RefCell::new(false));
        let pass_clone = pass.clone();
        let request = rpc_c.cmd("cmd");
        request
            .msg("hello")
            .rsp(|_: String| {
                assert!(false);
            })
            .finally(move |t| {
                assert_eq!(t, FinallyType::Canceled);
                *pass_clone.borrow_mut() = true;
            });
        {
            let mut dispose = rpc_core::dispose::Dispose::new();
            request.add_to(&mut dispose);
        }
        assert_eq!(request.call(), Err(FinallyType::Canceled));
        assert!(*pass.borrow());
    }
    {
        info!("--- dispose test remove ---");
        rpc_s.subscribe("cmd", |_: String| -> String { "".to_string() });

        let pass = Rc::new(RefCell::new(false));
        let pass_clone = pass.clone();
        let request = rpc_c.cmd("cmd");
        request
            .msg("hello")
            .rsp(|_: String| {
                assert!(true);
            })
            .finally(move |t| {
                assert_eq!(t, FinallyType::Normal);
                *pass_clone.borrow_mut() = true;
            });
        {
            let mut dispose = rpc_core::dispose::Dispose::new();
            request.add_to(&mut dispose);
            dispose.remove(&request);
        }
        request.call().unwrap();
        assert!(*pass.borrow());
    }
}

#[test]
fn oversized_command_reports_error_without_sending() {
    let connection = DefaultConnection::new();
    let sent = Rc::new(RefCell::new(false));
    let sent_copy = sent.clone();
    connection
        .borrow_mut()
        .set_send_package_impl(Box::new(move |_| {
            *sent_copy.borrow_mut() = true;
            true
        }));
    let rpc = Rpc::new(Some(connection));
    rpc.set_ready(true);

    let result = Rc::new(RefCell::new(None));
    let result_copy = result.clone();
    rpc.cmd("a".repeat(u16::MAX as usize + 1))
        .rsp(|_: String| {})
        .finally(move |type_| *result_copy.borrow_mut() = Some(type_))
        .call()
        .unwrap_err();

    assert_eq!(*result.borrow(), Some(FinallyType::ReqSerializeError));
    assert!(!*sent.borrow());
}

#[test]
fn cancel_removes_pending_response_before_timeout() {
    let connection = DefaultConnection::new();
    let timer = Rc::new(RefCell::new(None::<Box<dyn Fn()>>));
    let timer_copy = timer.clone();
    let rpc = Rpc::new(Some(connection));
    rpc.get_connection()
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    rpc.set_ready(true);
    rpc.set_timer(move |_, cb| *timer_copy.borrow_mut() = Some(cb));

    let timeout_count = Rc::new(RefCell::new(0));
    let timeout_copy = timeout_count.clone();
    let request = rpc.cmd("pending");
    request
        .rsp(|_: String| panic!("response after cancellation"))
        .timeout(move || *timeout_copy.borrow_mut() += 1)
        .call()
        .unwrap();
    request.cancel();
    timer.borrow().as_ref().unwrap()();

    assert_eq!(*timeout_count.borrow(), 0);
}

#[test]
fn command_can_unsubscribe_itself_during_callback() {
    let (server_connection, client_connection) = rpc_core::connection::LoopbackConnection::new();
    let server = Rpc::new(Some(server_connection));
    let client = Rpc::new(Some(client_connection));
    server.set_ready(true);
    client.set_ready(true);

    let server_copy = server.clone();
    server.subscribe("once", move |_: String| -> String {
        server_copy.unsubscribe("once");
        "done".to_owned()
    });

    let result = Rc::new(RefCell::new(None));
    let result_copy = result.clone();
    client
        .cmd("once")
        .msg("hello")
        .rsp(move |value: String| {
            *result_copy.borrow_mut() = Some(value);
        })
        .call()
        .unwrap();
    assert_eq!(*result.borrow(), Some("done".to_owned()));
}

#[test]
fn typed_future_completes_on_request_errors() {
    let not_ready = Rpc::new(None);
    let request = not_ready.cmd("cmd");
    let mut future = Box::pin(request.future::<String>());
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("not-ready request stayed pending");
    };
    assert_eq!(result.type_, FinallyType::RpcNotReady);
    assert!(result.result.is_none());

    let (server_connection, client_connection) = rpc_core::connection::LoopbackConnection::new();
    let _server = Rpc::new(Some(server_connection));
    let client = Rpc::new(Some(client_connection));
    client.set_ready(true);
    let request = client.cmd("missing");
    let mut future = Box::pin(request.future::<String>());
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("missing-command request stayed pending");
    };
    assert_eq!(result.type_, FinallyType::NoSuchCmd);
    assert!(result.result.is_none());

    let connection = DefaultConnection::new();
    let rpc = Rpc::new(Some(connection));
    rpc.get_connection()
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    rpc.set_ready(true);
    let timer = Rc::new(RefCell::new(None::<Box<dyn Fn()>>));
    let timer_copy = timer.clone();
    rpc.set_timer(move |_, cb| *timer_copy.borrow_mut() = Some(cb));

    let request = rpc.cmd("pending");
    let mut future = Box::pin(request.future::<String>());
    assert!(poll_once(future.as_mut()).is_pending());
    timer.borrow_mut().take().unwrap()();
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("timed-out request stayed pending");
    };
    assert_eq!(result.type_, FinallyType::Timeout);

    let request = rpc.cmd("pending");
    let mut future = Box::pin(request.future::<String>());
    assert!(poll_once(future.as_mut()).is_pending());
    request.cancel();
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("canceled request stayed pending");
    };
    assert_eq!(result.type_, FinallyType::Canceled);
}

#[test]
fn timeout_callback_can_cancel_request() {
    let rpc = Rpc::new(Some(DefaultConnection::new()));
    rpc.get_connection()
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    rpc.set_ready(true);
    let timer = Rc::new(RefCell::new(None::<Box<dyn Fn()>>));
    let timer_copy = timer.clone();
    rpc.set_timer(move |_, cb| *timer_copy.borrow_mut() = Some(cb));

    let request = rpc.cmd("pending");
    let weak = Rc::downgrade(&request);
    let result = Rc::new(RefCell::new(Vec::new()));
    let result_copy = result.clone();
    request
        .rsp(|_: String| {})
        .timeout(move || {
            weak.upgrade().unwrap().cancel();
        })
        .finally(move |type_| result_copy.borrow_mut().push(type_))
        .call()
        .unwrap();

    timer.borrow_mut().take().unwrap()();
    assert_eq!(*result.borrow(), vec![FinallyType::Canceled]);
}

#[test]
fn dropping_rpc_finishes_and_releases_pending_requests() {
    for with_timer in [false, true] {
        let rpc = Rpc::new(None);
        rpc.get_connection()
            .borrow_mut()
            .set_send_package_impl(Box::new(|_| true));
        rpc.set_ready(true);
        let timer = Rc::new(RefCell::new(None::<Box<dyn Fn()>>));
        let timer_copy = timer.clone();
        if with_timer {
            rpc.set_timer(move |_, cb| *timer_copy.borrow_mut() = Some(cb));
        }
        let result = Rc::new(RefCell::new(Vec::new()));
        let result_copy = result.clone();
        let request = rpc.cmd("pending");
        let observer = Rc::downgrade(&request);
        request
            .rsp(|_: String| {})
            .finally(move |t| result_copy.borrow_mut().push(t))
            .call()
            .unwrap();
        drop(request);
        drop(rpc);
        assert!(observer.upgrade().is_none());
        assert_eq!(*result.borrow(), vec![FinallyType::RpcExpired]);
        if let Some(timer) = timer.borrow_mut().take() {
            timer();
        }
        assert_eq!(*result.borrow(), vec![FinallyType::RpcExpired]);
    }
}

#[test]
fn dropping_old_rpc_does_not_finish_new_call() {
    let old_conn = DefaultConnection::new();
    old_conn
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    let old_rpc = Rpc::new(Some(old_conn));
    let new_conn = DefaultConnection::new();
    let packets = Rc::new(RefCell::new(Vec::new()));
    let sent = packets.clone();
    new_conn
        .borrow_mut()
        .set_send_package_impl(Box::new(move |packet| {
            sent.borrow_mut().push(packet);
            true
        }));
    let new_rpc = Rpc::new(Some(new_conn.clone()));
    old_rpc.set_ready(true);
    new_rpc.set_ready(true);
    let request = old_rpc.cmd("pending");
    let observer = Rc::downgrade(&request);
    let responses = Rc::new(RefCell::new(0));
    let count = responses.clone();
    let finished = Rc::new(RefCell::new(Vec::new()));
    let result = finished.clone();
    request
        .rsp(move |data: String| {
            assert_eq!(data, "ok");
            *count.borrow_mut() += 1;
        })
        .finally(move |t| result.borrow_mut().push(t))
        .call()
        .unwrap();
    assert_eq!(
        request.call_with_rpc(new_rpc.clone()),
        Err(FinallyType::Busy)
    );
    request.cancel().reset_cancel();
    request.call_with_rpc(new_rpc.clone()).unwrap();
    drop(request);
    drop(old_rpc);
    assert_eq!(*finished.borrow(), vec![FinallyType::Canceled]);
    assert!(observer.upgrade().is_some());
    let mut response = packets.borrow()[0][..4].to_vec();
    response.extend_from_slice(&[0, 0, 2]);
    response.extend_from_slice(b"\"ok\"");
    new_conn.borrow().on_recv_package(response);
    assert_eq!(*responses.borrow(), 1);
    assert_eq!(
        *finished.borrow(),
        vec![FinallyType::Canceled, FinallyType::Normal]
    );
    assert!(observer.upgrade().is_none());
}

#[test]
fn cancel_during_timeout_does_not_retry_or_finish_twice() {
    for retries in [0, 1, -1] {
        let rpc = Rpc::new(None);
        rpc.get_connection()
            .borrow_mut()
            .set_send_package_impl(Box::new(|_| true));
        rpc.set_ready(true);
        let timer = Rc::new(RefCell::new(None::<Box<dyn Fn()>>));
        let timer_copy = timer.clone();
        rpc.set_timer(move |_, cb| *timer_copy.borrow_mut() = Some(cb));
        let request = rpc.cmd("pending");
        let weak = Rc::downgrade(&request);
        let result = Rc::new(RefCell::new(Vec::new()));
        let result_copy = result.clone();
        request
            .rsp(|_: String| {})
            .retry(retries)
            .timeout(move || {
                weak.upgrade().unwrap().canceled(true);
            })
            .finally(move |t| result_copy.borrow_mut().push(t))
            .call()
            .unwrap();
        let fire = timer.borrow_mut().take().unwrap();
        fire();
        assert_eq!(*result.borrow(), vec![FinallyType::Canceled]);
        assert!(timer.borrow().is_none());
        request.reset_cancel().call().unwrap();
        assert!(timer.borrow().is_some());
        request.cancel();
        assert_eq!(result.borrow().len(), 2);
    }
}

#[test]
fn future_notifies_the_most_recent_waker() {
    use std::sync::atomic::{AtomicUsize, Ordering};
    struct Counter(AtomicUsize);
    impl Wake for Counter {
        fn wake(self: Arc<Self>) {
            self.0.fetch_add(1, Ordering::SeqCst);
        }
    }
    let rpc = Rpc::new(None);
    rpc.get_connection()
        .borrow_mut()
        .set_send_package_impl(Box::new(|_| true));
    rpc.set_ready(true);
    let request = rpc.cmd("pending");
    let mut future = Box::pin(request.future::<String>());
    let first = Arc::new(Counter(AtomicUsize::new(0)));
    let second = Arc::new(Counter(AtomicUsize::new(0)));
    for counter in [&first, &second] {
        let waker = Waker::from(counter.clone());
        assert!(future
            .as_mut()
            .poll(&mut Context::from_waker(&waker))
            .is_pending());
    }
    request.cancel();
    assert_eq!(first.0.load(Ordering::SeqCst), 0);
    assert_eq!(second.0.load(Ordering::SeqCst), 1);
    assert!(poll_once(future.as_mut()).is_ready());
}

#[test]
fn loopback_send_after_peer_drop_is_safe() {
    let (first, second) = rpc_core::connection::LoopbackConnection::new();
    drop(second);
    assert!(!first.borrow().send_package(vec![1]));
}

#[test]
fn invalid_json_request_reports_error_without_panicking() {
    let rpc = Rpc::new(None);
    rpc.set_ready(true);
    let request = rpc.cmd("invalid");
    let invalid = std::collections::BTreeMap::from([(vec![1, 2], 3)]);
    request.msg(invalid);
    let mut future = Box::pin(request.future::<String>());
    let Poll::Ready(result) = poll_once(future.as_mut()) else {
        panic!("serialization failure stayed pending");
    };
    assert_eq!(result.type_, FinallyType::ReqSerializeError);
}

#[test]
fn response_callback_can_start_another_call() {
    let conn = DefaultConnection::new();
    let packets = Rc::new(RefCell::new(Vec::new()));
    let sent = packets.clone();
    conn.borrow_mut().set_send_package_impl(Box::new(move |p| {
        sent.borrow_mut().push(p);
        true
    }));
    let rpc = Rpc::new(Some(conn.clone()));
    rpc.set_ready(true);
    let request = rpc.cmd("x");
    let observer = Rc::downgrade(&request);
    let weak = observer.clone();
    let responses = Rc::new(RefCell::new(0));
    let count = responses.clone();
    let finished = Rc::new(RefCell::new(Vec::new()));
    let first_finished = finished.clone();
    let second_finished = finished.clone();
    request
        .rsp(move |_: String| {
            *count.borrow_mut() += 1;
            if *count.borrow() == 1 {
                let req = weak.upgrade().unwrap();
                let finished = second_finished.clone();
                req.finally(move |t| {
                    assert_eq!(t, FinallyType::Normal);
                    finished.borrow_mut().push(2);
                })
                .call()
                .unwrap();
            }
        })
        .finally(move |t| {
            assert_eq!(t, FinallyType::Normal);
            first_finished.borrow_mut().push(1);
        })
        .call()
        .unwrap();
    let mut response = packets.borrow()[0][..4].to_vec();
    response.extend_from_slice(&[0, 0, 2]);
    response.extend_from_slice(b"\"ok\"");
    conn.borrow().on_recv_package(response.clone());
    drop(request);
    assert_eq!(*finished.borrow(), vec![1]);
    assert!(observer.upgrade().is_some());
    response[..4].copy_from_slice(&packets.borrow()[1][..4]);
    conn.borrow().on_recv_package(response);
    assert_eq!(*responses.borrow(), 2);
    assert_eq!(*finished.borrow(), vec![1, 2]);
    assert!(observer.upgrade().is_none());
}

struct PendingFixture {
    rpc: Rc<Rpc>,
    conn: Rc<RefCell<DefaultConnection>>,
    sent: Rc<RefCell<Vec<Vec<u8>>>>,
    timers: Rc<RefCell<Vec<Rc<dyn Fn()>>>>,
}

impl PendingFixture {
    fn new() -> Self {
        let conn = DefaultConnection::new();
        let sent = Rc::new(RefCell::new(Vec::new()));
        let packets = sent.clone();
        conn.borrow_mut().set_send_package_impl(Box::new(move |p| {
            packets.borrow_mut().push(p);
            true
        }));
        let rpc = Rpc::new(Some(conn.clone()));
        rpc.set_ready(true);
        let timers = Rc::new(RefCell::new(Vec::<Rc<dyn Fn()>>::new()));
        let callbacks = timers.clone();
        rpc.set_timer(move |_, cb| callbacks.borrow_mut().push(Rc::from(cb)));
        Self {
            rpc,
            conn,
            sent,
            timers,
        }
    }

    fn reply(&self, index: usize, value: &str) {
        let mut response = self.sent.borrow()[index][..4].to_vec();
        response.extend_from_slice(&[0, 0, 2]);
        response.extend_from_slice(&serde_json::to_vec(value).unwrap());
        self.conn.borrow().on_recv_package(response);
    }

    fn expire(&self, index: usize) {
        let callback = self.timers.borrow()[index].clone();
        callback();
    }
}

#[test]
fn busy_keeps_original_call_and_rpc() {
    let f = PendingFixture::new();
    let other = PendingFixture::new();
    let request = f.rpc.cmd("original");
    let results = Rc::new(RefCell::new(Vec::new()));
    let original = results.clone();
    request
        .rsp(|data: String| assert_eq!(data, "ok"))
        .finally(move |t| original.borrow_mut().push(t))
        .call()
        .unwrap();
    let next_results = Rc::new(RefCell::new(Vec::new()));
    let next = next_results.clone();
    request.finally(move |t| next.borrow_mut().push(t));
    assert_eq!(request.call(), Err(FinallyType::Busy));
    assert_eq!(
        request.call_with_rpc(other.rpc.clone()),
        Err(FinallyType::Busy)
    );
    assert!(Rc::ptr_eq(
        &request.get_rpc().unwrap().upgrade().unwrap(),
        &f.rpc
    ));
    assert_eq!(f.sent.borrow().len(), 1);
    assert_eq!(f.timers.borrow().len(), 1);
    assert!(other.sent.borrow().is_empty() && results.borrow().is_empty());
    f.reply(0, "ok");
    assert_eq!(*results.borrow(), vec![FinallyType::Normal]);
    assert!(next_results.borrow().is_empty());
    request.call().unwrap();
    f.reply(1, "ok");
    assert_eq!(*next_results.borrow(), vec![FinallyType::Normal]);
}

#[test]
fn cancel_reuse_ignores_old_responses_and_timers() {
    let f = PendingFixture::new();
    let other = PendingFixture::new();
    let request = f.rpc.cmd("x");
    let results = Rc::new(RefCell::new(Vec::new()));
    let finished = results.clone();
    request
        .rsp(|data: String| assert_eq!(data, "new"))
        .finally(move |t| finished.borrow_mut().push(t))
        .call()
        .unwrap();
    request
        .disable_rsp()
        .rpc(Rc::downgrade(&other.rpc))
        .cancel();
    request
        .reset_cancel()
        .rsp(|data: String| assert_eq!(data, "new"));
    request.call_with_rpc(f.rpc.clone()).unwrap();
    f.expire(0);
    f.reply(0, "old");
    assert_eq!(*results.borrow(), vec![FinallyType::Canceled]);
    f.reply(1, "new");
    f.expire(1);
    assert_eq!(
        *results.borrow(),
        vec![FinallyType::Canceled, FinallyType::Normal]
    );
}

#[test]
fn retries_preserve_active_options_and_reset_budget_on_reuse() {
    let f = PendingFixture::new();
    let other = PendingFixture::new();
    let request = f.rpc.cmd("original");
    let results = Rc::new(RefCell::new(Vec::new()));
    let finished = results.clone();
    let weak = Rc::downgrade(&request);
    let timeouts = Rc::new(RefCell::new(0));
    let count = timeouts.clone();
    request
        .msg("payload")
        .rsp(|_: String| panic!("late response completed retry"))
        .retry(2)
        .timeout(move || {
            *count.borrow_mut() += 1;
            assert_eq!(weak.upgrade().unwrap().call(), Err(FinallyType::Busy));
        })
        .finally(move |t| finished.borrow_mut().push(t))
        .call()
        .unwrap();
    request
        .cmd("next")
        .msg("changed")
        .disable_rsp()
        .rpc(Rc::downgrade(&other.rpc))
        .finally(|_| panic!("next-call finally ran for active call"));
    f.expire(0);
    f.expire(0);
    f.reply(0, "late response");
    assert_eq!(f.sent.borrow().len(), 2);
    assert!(results.borrow().is_empty());
    f.expire(1);
    assert_eq!(f.sent.borrow().len(), 3);
    assert!(results.borrow().is_empty());
    f.expire(2);
    assert_eq!(*results.borrow(), vec![FinallyType::Timeout]);
    assert_eq!(*timeouts.borrow(), 3);
    assert!(other.sent.borrow().is_empty());
    for packet in f.sent.borrow().iter() {
        let cmd_len = u16::from_le_bytes(packet[4..6].try_into().unwrap()) as usize;
        assert_eq!(&packet[6..6 + cmd_len], b"original");
        assert_eq!(&packet[7 + cmd_len..], b"\"payload\"");
    }
    let finished = results.clone();
    request
        .cmd("original")
        .msg("payload")
        .rsp(|_: String| {})
        .finally(move |t| finished.borrow_mut().push(t));
    request.call_with_rpc(f.rpc.clone()).unwrap();
    f.expire(3);
    f.expire(4);
    assert_eq!(f.sent.borrow().len(), 6);
    assert_eq!(results.borrow().len(), 1);
    f.expire(5);
    assert_eq!(
        *results.borrow(),
        vec![FinallyType::Timeout, FinallyType::Timeout]
    );
}

#[test]
fn reentrant_retry_survives_old_send_failure() {
    let f = PendingFixture::new();
    let packets = f.sent.clone();
    let timers = f.timers.clone();
    f.conn
        .borrow_mut()
        .set_send_package_impl(Box::new(move |packet| {
            packets.borrow_mut().push(packet);
            let first = packets.borrow().len() == 1;
            if first {
                let expire = timers.borrow()[0].clone();
                expire();
                return false;
            }
            true
        }));
    let results = Rc::new(RefCell::new(Vec::new()));
    let finished = results.clone();
    let request = f.rpc.cmd("retry");
    request
        .retry(1)
        .rsp(|value: String| assert_eq!(value, "ok"))
        .finally(move |status| finished.borrow_mut().push(status));
    assert_eq!(request.call(), Err(FinallyType::RpcNotReady));
    assert_eq!(f.sent.borrow().len(), 2);
    assert!(results.borrow().is_empty());
    assert_eq!(request.call(), Err(FinallyType::Busy));
    f.reply(0, "stale");
    f.expire(0);
    assert!(results.borrow().is_empty());
    f.reply(1, "ok");
    assert_eq!(*results.borrow(), vec![FinallyType::Normal]);
    f.expire(1);
    assert_eq!(results.borrow().len(), 1);
}

#[test]
fn timer_registration_can_disconnect_rpc() {
    let f = PendingFixture::new();
    let timers = f.timers.clone();
    let weak = Rc::downgrade(&f.rpc);
    f.rpc.set_timer(move |_, callback| {
        timers.borrow_mut().push(Rc::from(callback));
        if timers.borrow().len() == 1 {
            weak.upgrade().unwrap().set_ready(false);
        }
    });
    let results = Rc::new(RefCell::new(Vec::new()));
    let finished = results.clone();
    let request = f.rpc.cmd("x");
    request
        .rsp(|value: String| assert_eq!(value, "ok"))
        .finally(move |status| finished.borrow_mut().push(status));
    assert_eq!(request.call(), Err(FinallyType::RpcNotReady));
    assert!(f.sent.borrow().is_empty());
    assert_eq!(*results.borrow(), vec![FinallyType::RpcNotReady]);
    f.expire(0);
    assert_eq!(results.borrow().len(), 1);
    f.rpc.set_ready(true);
    request.call().unwrap();
    f.expire(0);
    assert_eq!(f.sent.borrow().len(), 1);
    assert_eq!(results.borrow().len(), 1);
    f.reply(0, "ok");
    assert_eq!(
        *results.borrow(),
        vec![FinallyType::RpcNotReady, FinallyType::Normal]
    );
}

#[test]
fn timeout_can_cancel_and_start_a_new_logical_call() {
    let f = PendingFixture::new();
    let request = f.rpc.cmd("x");
    let results = Rc::new(RefCell::new(Vec::new()));
    let finished = results.clone();
    let weak = Rc::downgrade(&request);
    request
        .rsp(|data: String| assert_eq!(data, "new"))
        .retry(-1)
        .finally(move |t| finished.borrow_mut().push(t))
        .timeout(move || {
            let req = weak.upgrade().unwrap();
            req.cancel().reset_cancel().timeout(|| {});
            req.call().unwrap();
        })
        .call()
        .unwrap();
    f.expire(0);
    assert_eq!(f.sent.borrow().len(), 2);
    assert_eq!(*results.borrow(), vec![FinallyType::Canceled]);
    f.reply(0, "old");
    f.expire(0);
    assert_eq!(f.sent.borrow().len(), 2);
    f.reply(1, "new");
    assert_eq!(
        *results.borrow(),
        vec![FinallyType::Canceled, FinallyType::Normal]
    );
}

#[test]
fn overlapping_future_returns_busy_without_replacing_original() {
    let f = PendingFixture::new();
    let request = f.rpc.cmd("x");
    let mut first = Box::pin(request.future::<String>());
    assert!(poll_once(first.as_mut()).is_pending());
    let mut busy = Box::pin(request.future::<String>());
    let Poll::Ready(result) = poll_once(busy.as_mut()) else {
        panic!("busy future stayed pending");
    };
    assert_eq!(result.type_, FinallyType::Busy);
    assert!(result.result.is_none());
    assert_eq!(f.sent.borrow().len(), 1);
    f.reply(0, "first");
    let Poll::Ready(result) = poll_once(first.as_mut()) else {
        panic!("original future was replaced");
    };
    assert_eq!(result.unwrap(), "first");
    drop(first);
    let mut second = Box::pin(request.future::<String>());
    assert!(poll_once(second.as_mut()).is_pending());
    f.expire(0);
    f.reply(0, "old");
    assert!(poll_once(second.as_mut()).is_pending());
    f.reply(1, "second");
    let Poll::Ready(result) = poll_once(second.as_mut()) else {
        panic!("reused future stayed pending");
    };
    assert_eq!(result.unwrap(), "second");
}

#[test]
fn synchronous_timer_finishes_without_sending_and_can_reenter() {
    let connection = DefaultConnection::new();
    let sends = Rc::new(std::cell::Cell::new(0));
    let copy = sends.clone();
    connection
        .borrow_mut()
        .set_send_package_impl(Box::new(move |_| {
            copy.set(copy.get() + 1);
            true
        }));
    let rpc = Rpc::new(Some(connection));
    rpc.set_ready(true);
    rpc.set_timer(|_, cb| cb());
    let request = rpc.cmd("action");
    request.msg(1).rsp(|_: i32| {}).timeout_ms(0);
    assert_eq!(request.call(), Err(FinallyType::Timeout));
    assert_eq!(sends.get(), 0);
    let weak_rpc = Rc::downgrade(&rpc);
    let weak_request = Rc::downgrade(&request);
    request.finally(move |status| {
        assert_eq!(status, FinallyType::Timeout);
        let rpc = weak_rpc.upgrade().unwrap();
        let request = weak_request.upgrade().unwrap();
        rpc.set_timer(|_, _| {});
        request.finally(|_| {});
        assert_eq!(request.call(), Ok(()));
    });
    assert_eq!(request.call(), Err(FinallyType::Timeout));
    assert_eq!(sends.get(), 1);
    request.cancel();
}

#[test]
fn dropping_pending_future_cancels_retries_and_releases_request() {
    let f = PendingFixture::new();
    let request = f.rpc.cmd("pending");
    request.retry(-1);
    let weak = Rc::downgrade(&request);
    let mut future = Box::pin(request.future::<String>());
    assert!(poll_once(future.as_mut()).is_pending());
    f.expire(0);
    assert_eq!(f.sent.borrow().len(), 2);
    drop(future);
    assert!(request.is_canceled());
    f.expire(1);
    f.reply(1, "late");
    assert_eq!(f.sent.borrow().len(), 2);
    request.reset_cancel().call().unwrap();
    f.reply(2, "new");
    drop(request);
    assert!(weak.upgrade().is_none());
}

#[test]
fn dropping_unpolled_or_busy_future_leaves_active_call_alone() {
    let f = PendingFixture::new();
    let request = f.rpc.cmd("pending");
    let unpolled = request.future::<String>();
    request.rsp(|_: String| {}).call().unwrap();
    drop(unpolled);
    let mut busy = Box::pin(request.future::<String>());
    assert!(matches!(poll_once(busy.as_mut()), Poll::Ready(ret) if ret.type_ == FinallyType::Busy));
    drop(busy);
    assert!(!request.is_canceled());
    assert_eq!(request.call(), Err(FinallyType::Busy));
    f.reply(0, "ok");
    assert!(request.call().is_ok());
    request.cancel();
}

#[test]
fn old_future_drop_does_not_cancel_reused_request() {
    for complete_with_response in [false, true] {
        let f = PendingFixture::new();
        let request = f.rpc.cmd("pending");
        let mut old = Box::pin(request.future::<String>());
        assert!(poll_once(old.as_mut()).is_pending());
        if complete_with_response {
            f.reply(0, "old");
        } else {
            request.cancel().reset_cancel();
        }
        let mut current = Box::pin(request.future::<String>());
        assert!(poll_once(current.as_mut()).is_pending());
        drop(old);
        assert!(!request.is_canceled());
        f.reply(1, "new");
        let Poll::Ready(ret) = poll_once(current.as_mut()) else {
            panic!("new call did not complete");
        };
        assert_eq!(ret.unwrap(), "new");
        drop(current);
        assert!(!request.is_canceled());
    }
}

#[cfg(feature = "net")]
#[test]
fn aborting_task_cancels_its_pending_rpc() {
    let runtime = tokio::runtime::Builder::new_current_thread()
        .build()
        .unwrap();
    runtime.block_on(tokio::task::LocalSet::new().run_until(async {
        let f = PendingFixture::new();
        let request = f.rpc.cmd("pending");
        request.retry(-1);
        let weak = Rc::downgrade(&request);
        let task = tokio::task::spawn_local(async move {
            request.future::<String>().await;
        });
        tokio::task::yield_now().await;
        assert_eq!(f.sent.borrow().len(), 1);
        task.abort();
        assert!(task.await.unwrap_err().is_cancelled());
        assert!(weak.upgrade().is_none());
        f.expire(0);
        assert_eq!(f.sent.borrow().len(), 1);
    }));
}
