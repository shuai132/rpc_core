use std::cell::RefCell;
use std::rc::Rc;

use log::info;

use rpc_core::connection::{Connection, DefaultConnection};
use rpc_core::request::FinallyType;
use rpc_core::rpc::Rpc;

#[test]
fn rpc() {
    std::env::set_var("RUST_LOG", "trace");
    let _ = env_logger::builder().is_test(true).try_init();

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
        .call();
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
        .call();
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
            .call_with_rpc(rpc_c.clone());
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
        request.call();
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
        request.call();
        assert!(*pass.borrow());
    }
}

#[test]
fn cancel_clears_pending_timeout_and_response_callbacks() {
    std::env::set_var("RUST_LOG", "trace");
    let _ = env_logger::builder().is_test(true).try_init();

    let conn_s = DefaultConnection::new();
    let conn_c = DefaultConnection::new();
    let to_server = Rc::new(RefCell::new(Vec::<Vec<u8>>::new()));
    let to_client = Rc::new(RefCell::new(Vec::<Vec<u8>>::new()));
    let client_timers = Rc::new(RefCell::new(Vec::<Box<dyn Fn()>>::new()));

    {
        let to_server = to_server.clone();
        conn_c
            .borrow_mut()
            .set_send_package_impl(Box::new(move |package| {
                to_server.borrow_mut().push(package);
            }));
    }
    {
        let to_client = to_client.clone();
        conn_s
            .borrow_mut()
            .set_send_package_impl(Box::new(move |package| {
                to_client.borrow_mut().push(package);
            }));
    }

    let rpc_s = Rpc::new(Some(conn_s.clone()));
    let rpc_c = Rpc::new(Some(conn_c.clone()));
    {
        let client_timers = client_timers.clone();
        rpc_c.set_timer(move |_ms, cb| {
            client_timers.borrow_mut().push(cb);
        });
    }
    rpc_s.set_ready(true);
    rpc_c.set_ready(true);

    rpc_s.subscribe("cmd_cancel_late", |msg: String| -> String {
        assert_eq!(msg, "hello");
        "world".to_string()
    });

    let pass_rsp = Rc::new(RefCell::new(false));
    let pass_timeout = Rc::new(RefCell::new(false));
    let finally_count = Rc::new(RefCell::new(0));
    let request = rpc_c.cmd("cmd_cancel_late");
    request
        .msg("hello")
        .rsp({
            let pass_rsp = pass_rsp.clone();
            move |rsp: String| {
                assert_eq!(rsp, "world");
                *pass_rsp.borrow_mut() = true;
            }
        })
        .timeout({
            let pass_timeout = pass_timeout.clone();
            move || {
                *pass_timeout.borrow_mut() = true;
            }
        })
        .finally({
            let finally_count = finally_count.clone();
            move |t| {
                assert_eq!(t, FinallyType::Canceled);
                *finally_count.borrow_mut() += 1;
            }
        });

    request.call();
    assert_eq!(to_server.borrow().len(), 1);
    assert_eq!(client_timers.borrow().len(), 1);

    request.cancel();
    assert_eq!(*finally_count.borrow(), 1);

    let timer = client_timers.borrow_mut().remove(0);
    timer();
    assert!(!*pass_timeout.borrow());
    assert!(!*pass_rsp.borrow());
    assert_eq!(*finally_count.borrow(), 1);

    drop(request);
    let payload = to_server.borrow_mut().remove(0);
    conn_s.borrow().on_recv_package(payload);
    assert_eq!(to_client.borrow().len(), 1);

    let payload = to_client.borrow_mut().remove(0);
    conn_c.borrow().on_recv_package(payload);
    assert!(!*pass_timeout.borrow());
    assert!(!*pass_rsp.borrow());
    assert_eq!(*finally_count.borrow(), 1);
}
