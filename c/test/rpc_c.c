#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpc_core_c/rpc_core.h"
#include "rpc_core_c/tcp.h"

#define CHECK(expr)                              \
  do {                                           \
    if (!(expr)) {                               \
      fprintf(stderr, "CHECK failed: %s\n", #expr); \
      abort();                                   \
    }                                            \
  } while (0)

typedef struct tcp_send_user {
  int fd;
} tcp_send_user_t;

static int tcp_send(const uint8_t* package, size_t package_len, void* user) {
  tcp_send_user_t* send_user = (tcp_send_user_t*)user;
  return rpc_core_tcp_send_package(send_user->fd, package, package_len);
}

static void on_rsp(const rpc_core_message_t* rsp, rpc_core_finally_t type, void* user) {
  int* done = (int*)user;
  if (type != RPC_CORE_FINALLY_NORMAL) {
    fprintf(stderr, "cmd failed: %s\n", rpc_core_finally_str(type));
    CHECK(0);
  }
  CHECK(rsp != NULL);
  CHECK(rpc_core_message_is_json(rsp));
  printf("cmd rsp: %s\n", rpc_core_message_text(rsp));
  CHECK(strcmp(rpc_core_message_text(rsp), "\"world\"") == 0);
  *done = 1;
}

int main(void) {
  const char* host = getenv("RPC_HOST");
  const char* port_text = getenv("RPC_PORT");
  uint16_t port = (uint16_t)(port_text != NULL ? (unsigned)atoi(port_text) : 6666u);
  int fd;
  tcp_send_user_t send_user;
  rpc_core_t* rpc;
  int done = 0;

  if (host == NULL) {
    host = "127.0.0.1";
  }

  fd = rpc_core_tcp_connect(host, port);
  CHECK(fd >= 0);
  printf("client on_open\n");

  send_user.fd = fd;
  rpc = rpc_core_create(tcp_send, &send_user);
  CHECK(rpc != NULL);
  rpc_core_set_ready(rpc, 1);
  CHECK(rpc_core_call_json(rpc, "cmd", "\"hello\"", on_rsp, &done) == 0);

  while (!done) {
    CHECK(rpc_core_tcp_recv_once(fd, rpc, 0) > 0);
  }

  rpc_core_destroy(rpc);
  rpc_core_tcp_close(fd);
  printf("client on_close\n");
  return 0;
}
