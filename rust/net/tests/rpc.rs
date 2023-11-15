#[cfg(test)]
mod rpc {
    use log::info;

    #[test]
    fn simple() {
        std::env::set_var("RUST_LOG", "trace");
        env_logger::init();

        info!("test");
    }
}
