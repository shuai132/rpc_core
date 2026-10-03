use std::sync::Mutex;

use rpc_core::connection::LoopbackConnection;
use rpc_core::rpc::Rpc;
use serde::{Serialize, Serializer};

struct CaptureLogger(Mutex<Vec<String>>);

impl log::Log for CaptureLogger {
    fn enabled(&self, _: &log::Metadata<'_>) -> bool {
        true
    }

    fn log(&self, record: &log::Record<'_>) {
        self.0.lock().unwrap().push(record.args().to_string());
    }

    fn flush(&self) {}
}

static LOGGER: CaptureLogger = CaptureLogger(Mutex::new(Vec::new()));
const SECRET: &str = "test-only-secret-token";

struct RejectedResponse;

impl Serialize for RejectedResponse {
    fn serialize<S: Serializer>(&self, _: S) -> Result<S::Ok, S::Error> {
        Err(serde::ser::Error::custom(SECRET))
    }
}

#[test]
fn response_serialization_errors_do_not_log_payloads() {
    log::set_logger(&LOGGER).unwrap();
    log::set_max_level(log::LevelFilter::Trace);
    let (server_connection, client_connection) = LoopbackConnection::new();
    let server = Rpc::new(Some(server_connection));
    let client = Rpc::new(Some(client_connection));
    server.set_ready(true);
    client.set_ready(true);
    server.subscribe("reject", |_: ()| RejectedResponse);
    client.cmd("reject").msg(()).call().unwrap();

    let errors = LOGGER.0.lock().unwrap();
    assert!(errors
        .iter()
        .any(|e| e.contains("response serialization failed")));
    assert!(errors.iter().all(|e| !e.contains(SECRET)));
}
