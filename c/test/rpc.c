#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "rpc_core_c/rpc_core.h"
#include "rpc_core_c/tcp.h"

#define CHECK(expr)                              \
  do {                                           \
    if (!(expr)) {                               \
      fprintf(stderr, "CHECK failed: %s\n", #expr); \
      abort();                                   \
    }                                            \
  } while (0)

typedef struct loopback_endpoint {
  rpc_core_t* peer;
} loopback_endpoint_t;

typedef struct tcp_send_user {
  rpc_core_socket_t fd;
} tcp_send_user_t;

typedef struct tcp_server_ctx {
  rpc_core_socket_t listen_fd;
} tcp_server_ctx_t;

#ifdef _WIN32
typedef HANDLE test_thread_t;
#define TEST_THREAD_RETURN DWORD WINAPI
static int test_thread_create(test_thread_t* thread, LPTHREAD_START_ROUTINE fn, void* arg) {
  *thread = CreateThread(NULL, 0, fn, arg, 0, NULL);
  return *thread != NULL ? 0 : -1;
}
static int test_thread_join(test_thread_t thread) {
  DWORD result = WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
  return result == WAIT_OBJECT_0 ? 0 : -1;
}
#else
typedef pthread_t test_thread_t;
#define TEST_THREAD_RETURN void*
static int test_thread_create(test_thread_t* thread, void* (*fn)(void*), void* arg) {
  return pthread_create(thread, NULL, fn, arg);
}
static int test_thread_join(test_thread_t thread) {
  return pthread_join(thread, NULL);
}
#endif

static int loopback_send(const uint8_t* package, size_t package_len, void* user) {
  loopback_endpoint_t* endpoint = (loopback_endpoint_t*)user;
  CHECK(endpoint != NULL);
  CHECK(endpoint->peer != NULL);
  return rpc_core_on_package(endpoint->peer, package, package_len);
}

static int tcp_send(const uint8_t* package, size_t package_len, void* user) {
  tcp_send_user_t* send_user = (tcp_send_user_t*)user;
  CHECK(send_user != NULL);
  return rpc_core_tcp_send_package(send_user->fd, package, package_len);
}

static void on_cmd(rpc_core_call_t* call, void* user) {
  (void)user;
  CHECK(strcmp(rpc_core_call_cmd(call), "cmd") == 0);
  CHECK(strcmp(rpc_core_call_payload_text(call), "hello") == 0);
  CHECK(rpc_core_reply_text(call, "world") == 0);
}

static void on_rsp(const rpc_core_message_t* rsp, rpc_core_finally_t type, void* user) {
  int* pass = (int*)user;
  CHECK(type == RPC_CORE_FINALLY_NORMAL);
  CHECK(rsp != NULL);
  CHECK(strcmp(rpc_core_message_text(rsp), "world") == 0);
  *pass = 1;
}

static void on_ping_rsp(const rpc_core_message_t* rsp, rpc_core_finally_t type, void* user) {
  int* pass = (int*)user;
  CHECK(type == RPC_CORE_FINALLY_NORMAL);
  CHECK(rsp != NULL);
  CHECK(strcmp(rpc_core_message_text(rsp), "hello") == 0);
  CHECK(rpc_core_message_has_type(rsp, RPC_CORE_MSG_PONG));
  *pass = 1;
}

static void on_missing_rsp(const rpc_core_message_t* rsp, rpc_core_finally_t type, void* user) {
  int* pass = (int*)user;
  CHECK(rsp != NULL);
  CHECK(type == RPC_CORE_FINALLY_NO_SUCH_CMD);
  CHECK(rpc_core_message_has_type(rsp, RPC_CORE_MSG_NO_SUCH_CMD));
  *pass = 1;
}

static void run_loopback(void) {
  loopback_endpoint_t server_endpoint;
  loopback_endpoint_t client_endpoint;
  rpc_core_t* server;
  rpc_core_t* client;
  int pass = 0;

  server_endpoint.peer = NULL;
  client_endpoint.peer = NULL;
  server = rpc_core_create(loopback_send, &server_endpoint);
  client = rpc_core_create(loopback_send, &client_endpoint);
  CHECK(server != NULL);
  CHECK(client != NULL);
  server_endpoint.peer = client;
  client_endpoint.peer = server;
  rpc_core_set_ready(server, 1);
  rpc_core_set_ready(client, 1);

  CHECK(rpc_core_subscribe(server, "cmd", on_cmd, NULL) == 0);

  {
    rpc_core_request_t req;
    rpc_core_request_init(&req, client);
    rpc_core_request_cmd(&req, "cmd");
    rpc_core_request_text(&req, "hello");
    rpc_core_request_rsp(&req, on_rsp, &pass);
    CHECK(rpc_core_request_call(&req) == 0);
    rpc_core_request_deinit(&req);
  }
  CHECK(pass);

  pass = 0;
  CHECK(rpc_core_ping_text(client, "hello", on_ping_rsp, &pass) == 0);
  CHECK(pass);

  pass = 0;
  CHECK(rpc_core_call_text(client, "missing", "hello", on_missing_rsp, &pass) == 0);
  CHECK(pass);

  rpc_core_destroy(client);
  rpc_core_destroy(server);
}

static TEST_THREAD_RETURN tcp_server_thread(void* arg) {
  tcp_server_ctx_t* ctx = (tcp_server_ctx_t*)arg;
  rpc_core_socket_t fd = rpc_core_tcp_accept(ctx->listen_fd);
  tcp_send_user_t send_user;
  rpc_core_t* server;
  CHECK(fd >= 0);
  send_user.fd = fd;
  server = rpc_core_create(tcp_send, &send_user);
  CHECK(server != NULL);
  rpc_core_set_ready(server, 1);
  CHECK(rpc_core_subscribe(server, "cmd", on_cmd, NULL) == 0);
  (void)rpc_core_tcp_recv_loop(fd, server, 0);
  rpc_core_destroy(server);
  rpc_core_tcp_close(fd);
  return 0;
}

static void run_tcp(void) {
  tcp_server_ctx_t server_ctx;
  test_thread_t thread;
  uint16_t port = 0;
  rpc_core_socket_t fd;
  tcp_send_user_t send_user;
  rpc_core_t* client;
  int pass = 0;

  server_ctx.listen_fd = rpc_core_tcp_listen("127.0.0.1", 0);
  CHECK(server_ctx.listen_fd >= 0);
  CHECK(rpc_core_tcp_local_port(server_ctx.listen_fd, &port) == 0);
  CHECK(port != 0);
  CHECK(test_thread_create(&thread, tcp_server_thread, &server_ctx) == 0);

  fd = rpc_core_tcp_connect("127.0.0.1", port);
  CHECK(fd >= 0);
  send_user.fd = fd;
  client = rpc_core_create(tcp_send, &send_user);
  CHECK(client != NULL);
  rpc_core_set_ready(client, 1);

  CHECK(rpc_core_call_text(client, "cmd", "hello", on_rsp, &pass) == 0);
  while (!pass) {
    CHECK(rpc_core_tcp_recv_once(fd, client, 0) > 0);
  }

  rpc_core_destroy(client);
  rpc_core_tcp_close(fd);
  CHECK(test_thread_join(thread) == 0);
  rpc_core_tcp_close(server_ctx.listen_fd);
}

int main(void) {
  run_loopback();
  run_tcp();
  printf("c rpc ok\n");
  return 0;
}
