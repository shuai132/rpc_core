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

        let connection = rpc_core::connection::LoopbackConnection::new();
        let rpc = rpc_core::rpc::Rpc::new(Some(connection));
        rpc.set_timer(|ms: u32, _: Box<dyn Fn()>| {
            info!("set_timer: {ms}");
        });
        rpc.set_ready(true);

        rpc.subscribe("cmd", |msg: String| -> String {
            assert_eq!(msg, "hello");
            "world".to_string()
        });

        let pass = Rc::new(RefCell::new(false));
        let pass_clone = pass.clone();
        rpc.cmd("cmd")
            .msg("hello")
            .rsp(move |msg: String| {
                assert_eq!(msg, "world");
                *pass_clone.borrow_mut() = true;
            })
            .call();
        assert!(*pass.borrow());

        info!("--- test unsubscribe ---");
        rpc.subscribe("x", |_: ()| {});
        rpc.unsubscribe("x");

        info!("--- test ping ---");
        *pass.borrow_mut() = false;
        let pass_clone = pass.clone();
        rpc.ping();
        rpc.ping_msg("hello")
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
                .call_with_rpc(rpc.clone());
            assert!(*pass.borrow());
        }

        info!("--- test dispose ---");
        {
            rpc.subscribe("cmd", |_: String| -> String {
                assert!(false);
                "".to_string()
            });

            let pass = Rc::new(RefCell::new(false));
            let pass_clone = pass.clone();
            let request = rpc.cmd("cmd");
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
