#ifndef RPC_CORE_C_RPC_CORE_H
#define RPC_CORE_C_RPC_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpc_core rpc_core_t;
typedef struct rpc_core_call rpc_core_call_t;

typedef enum rpc_core_msg_type {
  RPC_CORE_MSG_COMMAND = 1u << 0,
  RPC_CORE_MSG_RESPONSE = 1u << 1,
  RPC_CORE_MSG_NEED_RSP = 1u << 2,
  RPC_CORE_MSG_PING = 1u << 3,
  RPC_CORE_MSG_PONG = 1u << 4,
  RPC_CORE_MSG_NO_SUCH_CMD = 1u << 5,
  RPC_CORE_MSG_PAYLOAD_JSON = 1u << 6
} rpc_core_msg_type_t;

typedef enum rpc_core_finally {
  RPC_CORE_FINALLY_NORMAL = 0,
  RPC_CORE_FINALLY_NO_NEED_RSP = 1,
  RPC_CORE_FINALLY_TIMEOUT = 2,
  RPC_CORE_FINALLY_CANCELED = 3,
  RPC_CORE_FINALLY_RPC_EXPIRED = 4,
  RPC_CORE_FINALLY_RPC_NOT_READY = 5,
  RPC_CORE_FINALLY_RSP_SERIALIZE_ERROR = 6,
  RPC_CORE_FINALLY_NO_SUCH_CMD = 7
} rpc_core_finally_t;

typedef struct rpc_core_bytes {
  const uint8_t* data;
  size_t len;
} rpc_core_bytes_t;

typedef struct rpc_core_message {
  uint32_t seq;
  uint8_t type;
  char* cmd;
  uint8_t* data;
  size_t data_len;
} rpc_core_message_t;

typedef int (*rpc_core_send_fn)(const uint8_t* package, size_t package_len, void* user);
typedef void (*rpc_core_command_fn)(rpc_core_call_t* call, void* user);
typedef void (*rpc_core_response_fn)(const rpc_core_message_t* rsp, rpc_core_finally_t type, void* user);
typedef void (*rpc_core_finally_fn)(rpc_core_finally_t type, void* user);
typedef void (*rpc_core_error_fn)(const char* error, const rpc_core_message_t* msg, void* user);

typedef struct rpc_core_request {
  rpc_core_t* rpc;
  char* cmd;
  uint8_t* data;
  size_t data_len;
  int payload_json;
  int need_rsp;
  int is_ping;
  int canceled;
  uint32_t timeout_ms;
  uint32_t seq;
  rpc_core_response_fn rsp_fn;
  void* rsp_user;
  rpc_core_finally_fn finally_fn;
  void* finally_user;
} rpc_core_request_t;

void* rpc_core_malloc(size_t size);
void rpc_core_free(void* ptr);

rpc_core_t* rpc_core_create(rpc_core_send_fn send_fn, void* user);
void rpc_core_destroy(rpc_core_t* rpc);

void rpc_core_set_ready(rpc_core_t* rpc, int ready);
int rpc_core_is_ready(const rpc_core_t* rpc);
void rpc_core_set_error_handler(rpc_core_t* rpc, rpc_core_error_fn fn, void* user);

int rpc_core_on_package(rpc_core_t* rpc, const uint8_t* package, size_t package_len);

int rpc_core_subscribe(rpc_core_t* rpc, const char* cmd, rpc_core_command_fn fn, void* user);
void rpc_core_unsubscribe(rpc_core_t* rpc, const char* cmd);

void rpc_core_request_init(rpc_core_request_t* req, rpc_core_t* rpc);
void rpc_core_request_deinit(rpc_core_request_t* req);
rpc_core_request_t* rpc_core_request_cmd(rpc_core_request_t* req, const char* cmd);
rpc_core_request_t* rpc_core_request_raw(rpc_core_request_t* req, const void* data, size_t len);
rpc_core_request_t* rpc_core_request_text(rpc_core_request_t* req, const char* text);
rpc_core_request_t* rpc_core_request_json(rpc_core_request_t* req, const char* json);
rpc_core_request_t* rpc_core_request_rsp(rpc_core_request_t* req, rpc_core_response_fn fn, void* user);
rpc_core_request_t* rpc_core_request_finally(rpc_core_request_t* req, rpc_core_finally_fn fn, void* user);
rpc_core_request_t* rpc_core_request_timeout_ms(rpc_core_request_t* req, uint32_t timeout_ms);
rpc_core_request_t* rpc_core_request_ping(rpc_core_request_t* req);
void rpc_core_request_cancel(rpc_core_request_t* req);
int rpc_core_request_call(rpc_core_request_t* req);

int rpc_core_call_raw(rpc_core_t* rpc, const char* cmd, const void* data, size_t len, rpc_core_response_fn fn, void* user);
int rpc_core_call_text(rpc_core_t* rpc, const char* cmd, const char* text, rpc_core_response_fn fn, void* user);
int rpc_core_call_json(rpc_core_t* rpc, const char* cmd, const char* json, rpc_core_response_fn fn, void* user);
int rpc_core_notify_raw(rpc_core_t* rpc, const char* cmd, const void* data, size_t len);
int rpc_core_notify_text(rpc_core_t* rpc, const char* cmd, const char* text);
int rpc_core_notify_json(rpc_core_t* rpc, const char* cmd, const char* json);
int rpc_core_ping_raw(rpc_core_t* rpc, const void* data, size_t len, int payload_json, rpc_core_response_fn fn, void* user);
int rpc_core_ping_text(rpc_core_t* rpc, const char* text, rpc_core_response_fn fn, void* user);
int rpc_core_ping_json(rpc_core_t* rpc, const char* json, rpc_core_response_fn fn, void* user);

uint32_t rpc_core_message_seq(const rpc_core_message_t* msg);
uint8_t rpc_core_message_type(const rpc_core_message_t* msg);
const char* rpc_core_message_cmd(const rpc_core_message_t* msg);
rpc_core_bytes_t rpc_core_message_data(const rpc_core_message_t* msg);
const char* rpc_core_message_text(const rpc_core_message_t* msg);
int rpc_core_message_is_json(const rpc_core_message_t* msg);
int rpc_core_message_has_type(const rpc_core_message_t* msg, uint8_t type);

rpc_core_bytes_t rpc_core_call_payload(const rpc_core_call_t* call);
const char* rpc_core_call_payload_text(const rpc_core_call_t* call);
const char* rpc_core_call_cmd(const rpc_core_call_t* call);
int rpc_core_call_is_json(const rpc_core_call_t* call);
const rpc_core_message_t* rpc_core_call_message(const rpc_core_call_t* call);
int rpc_core_reply_raw(rpc_core_call_t* call, const void* data, size_t len, int payload_json);
int rpc_core_reply_text(rpc_core_call_t* call, const char* text);
int rpc_core_reply_json(rpc_core_call_t* call, const char* json);
int rpc_core_reply_empty(rpc_core_call_t* call);

const char* rpc_core_finally_str(rpc_core_finally_t type);

#ifdef __cplusplus
}
#endif

#endif
