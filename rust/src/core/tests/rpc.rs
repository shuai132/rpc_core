#[cfg(test)]
mod test_rpc {
    use std::cell::RefCell;
    use std::rc::Rc;

    use log::info;

    use rpc_core::request::FinallyType;

    #[test]
    fn simple() {
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
        rpc_c.cmd("cmd")
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
        rpc_c.ping_msg("hello")
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
            request.cmd("cmd")
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
            rpc_s.subscribe("cmd", |_: String| -> String {
                assert!(false);
                "".to_string()
            });

            let pass = Rc::new(RefCell::new(false));
            let pass_clone = pass.clone();
            let request = rpc_c.cmd("cmd");
            request.msg("hello")
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
    }
}
