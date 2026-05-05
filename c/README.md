# rpc_core C

Small C SDK for the `rpc_core` wire protocol.

The C API keeps the protocol layer binary-safe and dependency-free. Business code can use `text` or `json` helpers, while the core still sends raw payload bytes. JSON helpers only mark the payload with `RPC_CORE_MSG_PAYLOAD_JSON`; they do not parse JSON.

## Build

```shell
cmake -S c -B build-c
cmake --build build-c -j
./build-c/rpc_core_c_rpc
```

Standalone TCP checks:

```shell
RPC_ONCE=1 ./build-c/rpc_core_c_rpc_s
./build-c/rpc_core_c_rpc_c
```

## Hello World

```c
static void on_cmd(rpc_core_call_t* call, void* user) {
  assert(strcmp(rpc_core_call_payload_text(call), "hello") == 0);
  rpc_core_reply_text(call, "world");
}

rpc_core_subscribe(server, "cmd", on_cmd, NULL);
```

```c
static void on_rsp(const rpc_core_message_t* rsp, rpc_core_finally_t type, void* user) {
  assert(type == RPC_CORE_FINALLY_NORMAL);
  assert(strcmp(rpc_core_message_text(rsp), "world") == 0);
}

rpc_core_call_text(client, "cmd", "hello", on_rsp, NULL);
```

For cross-language calls with the JavaScript, Python, and Rust SDKs, use the JSON helpers:

```c
rpc_core_call_json(client, "cmd", "\"hello\"", on_rsp, NULL);
```
