#include "rpc_core_c/rpc_core.h"

#include <stdlib.h>
#include <string.h>

typedef struct rpc_core_command_entry {
  char* cmd;
  rpc_core_command_fn fn;
  void* user;
  struct rpc_core_command_entry* next;
} rpc_core_command_entry_t;

typedef struct rpc_core_pending_entry {
  uint32_t seq;
  rpc_core_response_fn rsp_fn;
  void* rsp_user;
  rpc_core_finally_fn finally_fn;
  void* finally_user;
  struct rpc_core_pending_entry* next;
} rpc_core_pending_entry_t;

struct rpc_core {
  rpc_core_send_fn send_fn;
  void* send_user;
  rpc_core_command_entry_t* commands;
  rpc_core_pending_entry_t* pending;
  uint32_t seq;
  int ready;
  rpc_core_error_fn error_fn;
  void* error_user;
};

struct rpc_core_call {
  rpc_core_t* rpc;
  const rpc_core_message_t* req;
  int need_rsp;
  int replied;
};

static char* rpc_core_strdup_(const char* src) {
  size_t len;
  char* out;
  if (src == NULL) {
    src = "";
  }
  len = strlen(src);
  out = (char*)rpc_core_malloc(len + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, src, len + 1);
  return out;
}

static uint8_t* rpc_core_memdup0_(const void* data, size_t len) {
  uint8_t* out = (uint8_t*)rpc_core_malloc(len + 1);
  if (out == NULL) {
    return NULL;
  }
  if (len > 0 && data != NULL) {
    memcpy(out, data, len);
  }
  out[len] = 0;
  return out;
}

static uint16_t rpc_core_read_u16_le_(const uint8_t* data) {
  return (uint16_t)(((uint16_t)data[0]) | ((uint16_t)data[1] << 8));
}

static uint32_t rpc_core_read_u32_le_(const uint8_t* data) {
  return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void rpc_core_write_u16_le_(uint8_t* data, uint16_t value) {
  data[0] = (uint8_t)(value & 0xffu);
  data[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void rpc_core_write_u32_le_(uint8_t* data, uint32_t value) {
  data[0] = (uint8_t)(value & 0xffu);
  data[1] = (uint8_t)((value >> 8) & 0xffu);
  data[2] = (uint8_t)((value >> 16) & 0xffu);
  data[3] = (uint8_t)((value >> 24) & 0xffu);
}

static void rpc_core_emit_error_(rpc_core_t* rpc, const char* error, const rpc_core_message_t* msg) {
  if (rpc != NULL && rpc->error_fn != NULL) {
    rpc->error_fn(error, msg, rpc->error_user);
  }
}

static void rpc_core_message_deinit_(rpc_core_message_t* msg) {
  if (msg == NULL) {
    return;
  }
  rpc_core_free(msg->cmd);
  rpc_core_free(msg->data);
  memset(msg, 0, sizeof(*msg));
}

static int rpc_core_deserialize_message_(const uint8_t* package, size_t package_len, rpc_core_message_t* out) {
  uint16_t cmd_len;
  size_t offset = 0;
  if (package == NULL || out == NULL || package_len < 7) {
    return -1;
  }
  memset(out, 0, sizeof(*out));
  out->seq = rpc_core_read_u32_le_(package + offset);
  offset += 4;
  cmd_len = rpc_core_read_u16_le_(package + offset);
  offset += 2;
  if (offset + cmd_len + 1 > package_len) {
    return -1;
  }
  out->cmd = (char*)rpc_core_malloc((size_t)cmd_len + 1);
  if (out->cmd == NULL) {
    return -1;
  }
  if (cmd_len > 0) {
    memcpy(out->cmd, package + offset, cmd_len);
  }
  out->cmd[cmd_len] = '\0';
  offset += cmd_len;
  out->type = package[offset++];
  out->data_len = package_len - offset;
  out->data = rpc_core_memdup0_(package + offset, out->data_len);
  if (out->data == NULL) {
    rpc_core_message_deinit_(out);
    return -1;
  }
  return 0;
}

static int rpc_core_serialize_message_(uint32_t seq, uint8_t type, const char* cmd, const void* data, size_t data_len, uint8_t** out,
                                       size_t* out_len) {
  size_t cmd_len;
  size_t offset = 0;
  uint8_t* package;
  if (cmd == NULL) {
    cmd = "";
  }
  cmd_len = strlen(cmd);
  if (cmd_len > 0xffffu || out == NULL || out_len == NULL) {
    return -1;
  }
  *out_len = 4 + 2 + cmd_len + 1 + data_len;
  package = (uint8_t*)rpc_core_malloc(*out_len);
  if (package == NULL) {
    return -1;
  }
  rpc_core_write_u32_le_(package + offset, seq);
  offset += 4;
  rpc_core_write_u16_le_(package + offset, (uint16_t)cmd_len);
  offset += 2;
  if (cmd_len > 0) {
    memcpy(package + offset, cmd, cmd_len);
  }
  offset += cmd_len;
  package[offset++] = type;
  if (data_len > 0 && data != NULL) {
    memcpy(package + offset, data, data_len);
  }
  *out = package;
  return 0;
}

static int rpc_core_send_message_(rpc_core_t* rpc, uint32_t seq, uint8_t type, const char* cmd, const void* data, size_t data_len) {
  uint8_t* package = NULL;
  size_t package_len = 0;
  int result;
  if (rpc == NULL || rpc->send_fn == NULL) {
    return -1;
  }
  if (rpc_core_serialize_message_(seq, type, cmd, data, data_len, &package, &package_len) != 0) {
    return -1;
  }
  result = rpc->send_fn(package, package_len, rpc->send_user);
  rpc_core_free(package);
  return result;
}

static rpc_core_command_entry_t* rpc_core_find_command_(rpc_core_t* rpc, const char* cmd) {
  rpc_core_command_entry_t* it;
  for (it = rpc != NULL ? rpc->commands : NULL; it != NULL; it = it->next) {
    if (strcmp(it->cmd, cmd) == 0) {
      return it;
    }
  }
  return NULL;
}

static rpc_core_pending_entry_t* rpc_core_take_pending_(rpc_core_t* rpc, uint32_t seq) {
  rpc_core_pending_entry_t** it;
  if (rpc == NULL) {
    return NULL;
  }
  for (it = &rpc->pending; *it != NULL; it = &(*it)->next) {
    if ((*it)->seq == seq) {
      rpc_core_pending_entry_t* found = *it;
      *it = found->next;
      found->next = NULL;
      return found;
    }
  }
  return NULL;
}

static int rpc_core_add_pending_(rpc_core_t* rpc, uint32_t seq, rpc_core_response_fn rsp_fn, void* rsp_user, rpc_core_finally_fn finally_fn,
                                 void* finally_user) {
  rpc_core_pending_entry_t* entry;
  if (rpc == NULL) {
    return -1;
  }
  entry = (rpc_core_pending_entry_t*)rpc_core_malloc(sizeof(*entry));
  if (entry == NULL) {
    return -1;
  }
  entry->seq = seq;
  entry->rsp_fn = rsp_fn;
  entry->rsp_user = rsp_user;
  entry->finally_fn = finally_fn;
  entry->finally_user = finally_user;
  entry->next = rpc->pending;
  rpc->pending = entry;
  return 0;
}

static void rpc_core_finish_(rpc_core_response_fn rsp_fn, void* rsp_user, rpc_core_finally_fn finally_fn, void* finally_user,
                             const rpc_core_message_t* msg, rpc_core_finally_t type) {
  if (rsp_fn != NULL) {
    rsp_fn(msg, type, rsp_user);
  }
  if (finally_fn != NULL) {
    finally_fn(type, finally_user);
  }
}

void* rpc_core_malloc(size_t size) {
  return malloc(size == 0 ? 1 : size);
}

void rpc_core_free(void* ptr) {
  free(ptr);
}

rpc_core_t* rpc_core_create(rpc_core_send_fn send_fn, void* user) {
  rpc_core_t* rpc = (rpc_core_t*)rpc_core_malloc(sizeof(*rpc));
  if (rpc == NULL) {
    return NULL;
  }
  memset(rpc, 0, sizeof(*rpc));
  rpc->send_fn = send_fn;
  rpc->send_user = user;
  return rpc;
}

void rpc_core_destroy(rpc_core_t* rpc) {
  rpc_core_command_entry_t* cmd;
  rpc_core_pending_entry_t* pending;
  if (rpc == NULL) {
    return;
  }
  cmd = rpc->commands;
  while (cmd != NULL) {
    rpc_core_command_entry_t* next = cmd->next;
    rpc_core_free(cmd->cmd);
    rpc_core_free(cmd);
    cmd = next;
  }
  pending = rpc->pending;
  while (pending != NULL) {
    rpc_core_pending_entry_t* next = pending->next;
    rpc_core_finish_(pending->rsp_fn, pending->rsp_user, pending->finally_fn, pending->finally_user, NULL, RPC_CORE_FINALLY_RPC_EXPIRED);
    rpc_core_free(pending);
    pending = next;
  }
  rpc_core_free(rpc);
}

void rpc_core_set_ready(rpc_core_t* rpc, int ready) {
  if (rpc != NULL) {
    rpc->ready = ready != 0;
  }
}

int rpc_core_is_ready(const rpc_core_t* rpc) {
  return rpc != NULL && rpc->ready;
}

void rpc_core_set_error_handler(rpc_core_t* rpc, rpc_core_error_fn fn, void* user) {
  if (rpc == NULL) {
    return;
  }
  rpc->error_fn = fn;
  rpc->error_user = user;
}

int rpc_core_on_package(rpc_core_t* rpc, const uint8_t* package, size_t package_len) {
  rpc_core_message_t msg;
  if (rpc == NULL || rpc_core_deserialize_message_(package, package_len, &msg) != 0) {
    rpc_core_emit_error_(rpc, "payload deserialize error", NULL);
    return -1;
  }

  if ((msg.type & RPC_CORE_MSG_COMMAND) != 0) {
    int need_rsp = (msg.type & RPC_CORE_MSG_NEED_RSP) != 0;
    if ((msg.type & RPC_CORE_MSG_PING) != 0) {
      uint8_t type = (uint8_t)(RPC_CORE_MSG_RESPONSE | RPC_CORE_MSG_PONG | (msg.type & RPC_CORE_MSG_PAYLOAD_JSON));
      int result = rpc_core_send_message_(rpc, msg.seq, type, msg.cmd, msg.data, msg.data_len);
      rpc_core_message_deinit_(&msg);
      return result;
    }

    {
      rpc_core_command_entry_t* entry = rpc_core_find_command_(rpc, msg.cmd);
      if (entry == NULL) {
        int result = 0;
        if (need_rsp) {
          result = rpc_core_send_message_(rpc, msg.seq, (uint8_t)(RPC_CORE_MSG_RESPONSE | RPC_CORE_MSG_NO_SUCH_CMD), "", NULL, 0);
        }
        rpc_core_message_deinit_(&msg);
        return result;
      }

      {
        rpc_core_call_t call;
        call.rpc = rpc;
        call.req = &msg;
        call.need_rsp = need_rsp;
        call.replied = 0;
        entry->fn(&call, entry->user);
        if (need_rsp && !call.replied) {
          (void)rpc_core_reply_empty(&call);
        }
      }
      rpc_core_message_deinit_(&msg);
      return 0;
    }
  }

  if ((msg.type & RPC_CORE_MSG_RESPONSE) != 0) {
    rpc_core_pending_entry_t* pending = rpc_core_take_pending_(rpc, msg.seq);
    if (pending != NULL) {
      rpc_core_finally_t type = (msg.type & RPC_CORE_MSG_NO_SUCH_CMD) != 0 ? RPC_CORE_FINALLY_NO_SUCH_CMD : RPC_CORE_FINALLY_NORMAL;
      rpc_core_finish_(pending->rsp_fn, pending->rsp_user, pending->finally_fn, pending->finally_user, &msg, type);
      rpc_core_free(pending);
    }
    rpc_core_message_deinit_(&msg);
    return 0;
  }

  rpc_core_emit_error_(rpc, "unknown message type", &msg);
  rpc_core_message_deinit_(&msg);
  return -1;
}

int rpc_core_subscribe(rpc_core_t* rpc, const char* cmd, rpc_core_command_fn fn, void* user) {
  rpc_core_command_entry_t* entry;
  if (rpc == NULL || cmd == NULL || fn == NULL) {
    return -1;
  }
  entry = rpc_core_find_command_(rpc, cmd);
  if (entry != NULL) {
    entry->fn = fn;
    entry->user = user;
    return 0;
  }
  entry = (rpc_core_command_entry_t*)rpc_core_malloc(sizeof(*entry));
  if (entry == NULL) {
    return -1;
  }
  entry->cmd = rpc_core_strdup_(cmd);
  if (entry->cmd == NULL) {
    rpc_core_free(entry);
    return -1;
  }
  entry->fn = fn;
  entry->user = user;
  entry->next = rpc->commands;
  rpc->commands = entry;
  return 0;
}

void rpc_core_unsubscribe(rpc_core_t* rpc, const char* cmd) {
  rpc_core_command_entry_t** it;
  if (rpc == NULL || cmd == NULL) {
    return;
  }
  for (it = &rpc->commands; *it != NULL; it = &(*it)->next) {
    if (strcmp((*it)->cmd, cmd) == 0) {
      rpc_core_command_entry_t* found = *it;
      *it = found->next;
      rpc_core_free(found->cmd);
      rpc_core_free(found);
      return;
    }
  }
}

void rpc_core_request_init(rpc_core_request_t* req, rpc_core_t* rpc) {
  if (req == NULL) {
    return;
  }
  memset(req, 0, sizeof(*req));
  req->rpc = rpc;
  req->timeout_ms = 3000;
}

void rpc_core_request_deinit(rpc_core_request_t* req) {
  if (req == NULL) {
    return;
  }
  rpc_core_free(req->cmd);
  rpc_core_free(req->data);
  memset(req, 0, sizeof(*req));
}

rpc_core_request_t* rpc_core_request_cmd(rpc_core_request_t* req, const char* cmd) {
  if (req == NULL) {
    return NULL;
  }
  rpc_core_free(req->cmd);
  req->cmd = rpc_core_strdup_(cmd);
  return req;
}

rpc_core_request_t* rpc_core_request_raw(rpc_core_request_t* req, const void* data, size_t len) {
  if (req == NULL) {
    return NULL;
  }
  rpc_core_free(req->data);
  req->data = rpc_core_memdup0_(data, len);
  req->data_len = req->data != NULL ? len : 0;
  req->payload_json = 0;
  return req;
}

rpc_core_request_t* rpc_core_request_text(rpc_core_request_t* req, const char* text) {
  return rpc_core_request_raw(req, text, text != NULL ? strlen(text) : 0);
}

rpc_core_request_t* rpc_core_request_json(rpc_core_request_t* req, const char* json) {
  req = rpc_core_request_raw(req, json, json != NULL ? strlen(json) : 0);
  if (req != NULL) {
    req->payload_json = 1;
  }
  return req;
}

rpc_core_request_t* rpc_core_request_rsp(rpc_core_request_t* req, rpc_core_response_fn fn, void* user) {
  if (req == NULL) {
    return NULL;
  }
  req->need_rsp = 1;
  req->rsp_fn = fn;
  req->rsp_user = user;
  return req;
}

rpc_core_request_t* rpc_core_request_finally(rpc_core_request_t* req, rpc_core_finally_fn fn, void* user) {
  if (req == NULL) {
    return NULL;
  }
  req->finally_fn = fn;
  req->finally_user = user;
  return req;
}

rpc_core_request_t* rpc_core_request_timeout_ms(rpc_core_request_t* req, uint32_t timeout_ms) {
  if (req != NULL) {
    req->timeout_ms = timeout_ms;
  }
  return req;
}

rpc_core_request_t* rpc_core_request_ping(rpc_core_request_t* req) {
  if (req != NULL) {
    req->is_ping = 1;
  }
  return req;
}

void rpc_core_request_cancel(rpc_core_request_t* req) {
  if (req == NULL) {
    return;
  }
  req->canceled = 1;
  if (req->rpc != NULL) {
    rpc_core_pending_entry_t* pending = rpc_core_take_pending_(req->rpc, req->seq);
    if (pending != NULL) {
      rpc_core_finish_(pending->rsp_fn, pending->rsp_user, pending->finally_fn, pending->finally_user, NULL, RPC_CORE_FINALLY_CANCELED);
      rpc_core_free(pending);
      return;
    }
  }
  rpc_core_finish_(req->rsp_fn, req->rsp_user, req->finally_fn, req->finally_user, NULL, RPC_CORE_FINALLY_CANCELED);
}

int rpc_core_request_call(rpc_core_request_t* req) {
  uint8_t type = RPC_CORE_MSG_COMMAND;
  int result;
  if (req == NULL || req->rpc == NULL) {
    rpc_core_finish_(req != NULL ? req->rsp_fn : NULL, req != NULL ? req->rsp_user : NULL, req != NULL ? req->finally_fn : NULL,
                     req != NULL ? req->finally_user : NULL, NULL, RPC_CORE_FINALLY_RPC_EXPIRED);
    return -1;
  }
  if (req->canceled) {
    rpc_core_finish_(req->rsp_fn, req->rsp_user, req->finally_fn, req->finally_user, NULL, RPC_CORE_FINALLY_CANCELED);
    return -1;
  }
  if (!req->rpc->ready) {
    rpc_core_finish_(req->rsp_fn, req->rsp_user, req->finally_fn, req->finally_user, NULL, RPC_CORE_FINALLY_RPC_NOT_READY);
    return -1;
  }
  req->seq = req->rpc->seq++;
  if (req->is_ping) {
    type = (uint8_t)(type | RPC_CORE_MSG_PING);
  }
  if (req->need_rsp) {
    type = (uint8_t)(type | RPC_CORE_MSG_NEED_RSP);
    if (rpc_core_add_pending_(req->rpc, req->seq, req->rsp_fn, req->rsp_user, req->finally_fn, req->finally_user) != 0) {
      return -1;
    }
  }
  if (req->payload_json) {
    type = (uint8_t)(type | RPC_CORE_MSG_PAYLOAD_JSON);
  }
  result = rpc_core_send_message_(req->rpc, req->seq, type, req->cmd != NULL ? req->cmd : "", req->data, req->data_len);
  if (result != 0 && req->need_rsp) {
    rpc_core_pending_entry_t* pending = rpc_core_take_pending_(req->rpc, req->seq);
    if (pending != NULL) {
      rpc_core_finish_(pending->rsp_fn, pending->rsp_user, pending->finally_fn, pending->finally_user, NULL, RPC_CORE_FINALLY_RPC_EXPIRED);
      rpc_core_free(pending);
    }
    return result;
  }
  if (!req->need_rsp) {
    rpc_core_finish_(req->rsp_fn, req->rsp_user, req->finally_fn, req->finally_user, NULL, RPC_CORE_FINALLY_NO_NEED_RSP);
  }
  return result;
}

int rpc_core_call_raw(rpc_core_t* rpc, const char* cmd, const void* data, size_t len, rpc_core_response_fn fn, void* user) {
  int result;
  rpc_core_request_t req;
  rpc_core_request_init(&req, rpc);
  rpc_core_request_cmd(&req, cmd);
  rpc_core_request_raw(&req, data, len);
  rpc_core_request_rsp(&req, fn, user);
  result = rpc_core_request_call(&req);
  rpc_core_request_deinit(&req);
  return result;
}

int rpc_core_call_text(rpc_core_t* rpc, const char* cmd, const char* text, rpc_core_response_fn fn, void* user) {
  return rpc_core_call_raw(rpc, cmd, text, text != NULL ? strlen(text) : 0, fn, user);
}

int rpc_core_call_json(rpc_core_t* rpc, const char* cmd, const char* json, rpc_core_response_fn fn, void* user) {
  int result;
  rpc_core_request_t req;
  rpc_core_request_init(&req, rpc);
  rpc_core_request_cmd(&req, cmd);
  rpc_core_request_json(&req, json);
  rpc_core_request_rsp(&req, fn, user);
  result = rpc_core_request_call(&req);
  rpc_core_request_deinit(&req);
  return result;
}

int rpc_core_notify_raw(rpc_core_t* rpc, const char* cmd, const void* data, size_t len) {
  int result;
  rpc_core_request_t req;
  rpc_core_request_init(&req, rpc);
  rpc_core_request_cmd(&req, cmd);
  rpc_core_request_raw(&req, data, len);
  result = rpc_core_request_call(&req);
  rpc_core_request_deinit(&req);
  return result;
}

int rpc_core_notify_text(rpc_core_t* rpc, const char* cmd, const char* text) {
  return rpc_core_notify_raw(rpc, cmd, text, text != NULL ? strlen(text) : 0);
}

int rpc_core_notify_json(rpc_core_t* rpc, const char* cmd, const char* json) {
  int result;
  rpc_core_request_t req;
  rpc_core_request_init(&req, rpc);
  rpc_core_request_cmd(&req, cmd);
  rpc_core_request_json(&req, json);
  result = rpc_core_request_call(&req);
  rpc_core_request_deinit(&req);
  return result;
}

int rpc_core_ping_raw(rpc_core_t* rpc, const void* data, size_t len, int payload_json, rpc_core_response_fn fn, void* user) {
  int result;
  rpc_core_request_t req;
  rpc_core_request_init(&req, rpc);
  rpc_core_request_ping(&req);
  rpc_core_request_raw(&req, data, len);
  req.payload_json = payload_json != 0;
  rpc_core_request_rsp(&req, fn, user);
  result = rpc_core_request_call(&req);
  rpc_core_request_deinit(&req);
  return result;
}

int rpc_core_ping_text(rpc_core_t* rpc, const char* text, rpc_core_response_fn fn, void* user) {
  return rpc_core_ping_raw(rpc, text, text != NULL ? strlen(text) : 0, 0, fn, user);
}

int rpc_core_ping_json(rpc_core_t* rpc, const char* json, rpc_core_response_fn fn, void* user) {
  return rpc_core_ping_raw(rpc, json, json != NULL ? strlen(json) : 0, 1, fn, user);
}

uint32_t rpc_core_message_seq(const rpc_core_message_t* msg) {
  return msg != NULL ? msg->seq : 0;
}

uint8_t rpc_core_message_type(const rpc_core_message_t* msg) {
  return msg != NULL ? msg->type : 0;
}

const char* rpc_core_message_cmd(const rpc_core_message_t* msg) {
  return msg != NULL && msg->cmd != NULL ? msg->cmd : "";
}

rpc_core_bytes_t rpc_core_message_data(const rpc_core_message_t* msg) {
  rpc_core_bytes_t bytes;
  bytes.data = msg != NULL ? msg->data : NULL;
  bytes.len = msg != NULL ? msg->data_len : 0;
  return bytes;
}

const char* rpc_core_message_text(const rpc_core_message_t* msg) {
  return msg != NULL && msg->data != NULL ? (const char*)msg->data : "";
}

int rpc_core_message_is_json(const rpc_core_message_t* msg) {
  return msg != NULL && (msg->type & RPC_CORE_MSG_PAYLOAD_JSON) != 0;
}

int rpc_core_message_has_type(const rpc_core_message_t* msg, uint8_t type) {
  return msg != NULL && (msg->type & type) != 0;
}

rpc_core_bytes_t rpc_core_call_payload(const rpc_core_call_t* call) {
  return rpc_core_message_data(call != NULL ? call->req : NULL);
}

const char* rpc_core_call_payload_text(const rpc_core_call_t* call) {
  return rpc_core_message_text(call != NULL ? call->req : NULL);
}

const char* rpc_core_call_cmd(const rpc_core_call_t* call) {
  return rpc_core_message_cmd(call != NULL ? call->req : NULL);
}

int rpc_core_call_is_json(const rpc_core_call_t* call) {
  return rpc_core_message_is_json(call != NULL ? call->req : NULL);
}

const rpc_core_message_t* rpc_core_call_message(const rpc_core_call_t* call) {
  return call != NULL ? call->req : NULL;
}

int rpc_core_reply_raw(rpc_core_call_t* call, const void* data, size_t len, int payload_json) {
  uint8_t type = RPC_CORE_MSG_RESPONSE;
  int result = 0;
  if (call == NULL || call->req == NULL) {
    return -1;
  }
  call->replied = 1;
  if (!call->need_rsp) {
    return 0;
  }
  if (payload_json) {
    type = (uint8_t)(type | RPC_CORE_MSG_PAYLOAD_JSON);
  }
  result = rpc_core_send_message_(call->rpc, call->req->seq, type, "", data, len);
  return result;
}

int rpc_core_reply_text(rpc_core_call_t* call, const char* text) {
  return rpc_core_reply_raw(call, text, text != NULL ? strlen(text) : 0, 0);
}

int rpc_core_reply_json(rpc_core_call_t* call, const char* json) {
  return rpc_core_reply_raw(call, json, json != NULL ? strlen(json) : 0, 1);
}

int rpc_core_reply_empty(rpc_core_call_t* call) {
  return rpc_core_reply_raw(call, NULL, 0, 0);
}

const char* rpc_core_finally_str(rpc_core_finally_t type) {
  switch (type) {
    case RPC_CORE_FINALLY_NORMAL:
      return "normal";
    case RPC_CORE_FINALLY_NO_NEED_RSP:
      return "no_need_rsp";
    case RPC_CORE_FINALLY_TIMEOUT:
      return "timeout";
    case RPC_CORE_FINALLY_CANCELED:
      return "canceled";
    case RPC_CORE_FINALLY_RPC_EXPIRED:
      return "rpc_expired";
    case RPC_CORE_FINALLY_RPC_NOT_READY:
      return "rpc_not_ready";
    case RPC_CORE_FINALLY_RSP_SERIALIZE_ERROR:
      return "rsp_serialize_error";
    case RPC_CORE_FINALLY_NO_SUCH_CMD:
      return "no_such_cmd";
    default:
      return "unknown";
  }
}
