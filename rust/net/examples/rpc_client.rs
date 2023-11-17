use std::rc::Rc;
use std::time::Duration;

use rpc_core_net::rpc_client;

fn main() {
    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();

    runtime.block_on(async {
        let mut client = rpc_client::create(&runtime);
        client.on_open(|rpc: Rc<rpc_core::rpc::Rpc>| {
            rpc.ping().call();
        });
        client.open("localhost", 6666);
        println!("fff");
        tokio::time::sleep(Duration::from_secs(1000)).await;
    });
}


