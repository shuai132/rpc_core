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
  rpc_core_socket_t fd;
} tcp_send_user_t;

static int tcp_send(const uint8_t* package, size_t package_len, void* user) {
  tcp_send_user_t* send_user = (tcp_send_user_t*)user;
  return rpc_core_tcp_send_package(send_user->fd, package, package_len);
}

static void on_cmd(rpc_core_call_t* call, void* user) {
  (void)user;
  printf("session on cmd: %s\n", rpc_core_call_payload_text(call));
  if (rpc_core_call_is_json(call)) {
    CHECK(strcmp(rpc_core_call_payload_text(call), "\"hello\"") == 0);
    CHECK(rpc_core_reply_json(call, "\"world\"") == 0);
  } else {
    CHECK(strcmp(rpc_core_call_payload_text(call), "hello") == 0);
    CHECK(rpc_core_reply_text(call, "world") == 0);
  }
}

int main(void) {
  const char* host = getenv("RPC_HOST");
  const char* port_text = getenv("RPC_PORT");
  int run_once = getenv("RPC_ONCE") != NULL && strcmp(getenv("RPC_ONCE"), "1") == 0;
  uint16_t port = (uint16_t)(port_text != NULL ? (unsigned)atoi(port_text) : 6666u);
  rpc_core_socket_t listen_fd;

  if (host == NULL) {
    host = "127.0.0.1";
  }
  listen_fd = rpc_core_tcp_listen(host, port);
  CHECK(listen_fd >= 0);
  printf("rpc_s listening on %s:%u\n", host, (unsigned)port);

  for (;;) {
    rpc_core_socket_t fd = rpc_core_tcp_accept(listen_fd);
    tcp_send_user_t send_user;
    rpc_core_t* rpc;
    CHECK(fd >= 0);
    printf("on_session\n");
    send_user.fd = fd;
    rpc = rpc_core_create(tcp_send, &send_user);
    CHECK(rpc != NULL);
    rpc_core_set_ready(rpc, 1);
    CHECK(rpc_core_subscribe(rpc, "cmd", on_cmd, NULL) == 0);
    (void)rpc_core_tcp_recv_loop(fd, rpc, 0);
    printf("session on_close\n");
    rpc_core_destroy(rpc);
    rpc_core_tcp_close(fd);
    if (run_once) {
      break;
    }
  }

  rpc_core_tcp_close(listen_fd);
  return 0;
}
