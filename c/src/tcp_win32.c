#include "rpc_core_c/tcp.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "rpc_core_c/stream.h"

typedef struct rpc_core_tcp_recv_ctx {
  rpc_core_t* rpc;
  int count;
} rpc_core_tcp_recv_ctx_t;

static int rpc_core_tcp_init_(void) {
  static int initialized = 0;
  WSADATA data;
  if (initialized) {
    return 0;
  }
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    return -1;
  }
  initialized = 1;
  return 0;
}

static SOCKET rpc_core_tcp_socket_(rpc_core_socket_t fd) {
  return (SOCKET)(uintptr_t)fd;
}

static rpc_core_socket_t rpc_core_tcp_fd_(SOCKET socket_value) {
  if (socket_value == INVALID_SOCKET) {
    return (rpc_core_socket_t)-1;
  }
  return (rpc_core_socket_t)(uintptr_t)socket_value;
}

static void rpc_core_tcp_on_package_(const uint8_t* package, size_t package_len, void* user) {
  rpc_core_tcp_recv_ctx_t* ctx = (rpc_core_tcp_recv_ctx_t*)user;
  if (ctx == NULL) {
    return;
  }
  (void)rpc_core_on_package(ctx->rpc, package, package_len);
  ctx->count++;
}

static int rpc_core_tcp_send_all_(rpc_core_socket_t fd, const uint8_t* data, size_t len) {
  SOCKET socket_value = rpc_core_tcp_socket_(fd);
  size_t sent = 0;
  while (sent < len) {
    int chunk = (int)((len - sent) > INT_MAX ? INT_MAX : (len - sent));
    int n = send(socket_value, (const char*)data + sent, chunk, 0);
    if (n == SOCKET_ERROR || n == 0) {
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

rpc_core_socket_t rpc_core_tcp_connect(const char* host, uint16_t port) {
  char port_text[16];
  struct addrinfo hints;
  struct addrinfo* result = NULL;
  struct addrinfo* it;
  SOCKET socket_value = INVALID_SOCKET;

  if (rpc_core_tcp_init_() != 0) {
    return (rpc_core_socket_t)-1;
  }
  if (host == NULL) {
    host = "127.0.0.1";
  }
  snprintf(port_text, sizeof(port_text), "%u", (unsigned)port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  if (getaddrinfo(host, port_text, &hints, &result) != 0) {
    return (rpc_core_socket_t)-1;
  }
  for (it = result; it != NULL; it = it->ai_next) {
    socket_value = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (socket_value == INVALID_SOCKET) {
      continue;
    }
    if (connect(socket_value, it->ai_addr, (int)it->ai_addrlen) == 0) {
      break;
    }
    closesocket(socket_value);
    socket_value = INVALID_SOCKET;
  }
  freeaddrinfo(result);
  return rpc_core_tcp_fd_(socket_value);
}

rpc_core_socket_t rpc_core_tcp_listen(const char* host, uint16_t port) {
  char port_text[16];
  struct addrinfo hints;
  struct addrinfo* result = NULL;
  struct addrinfo* it;
  SOCKET socket_value = INVALID_SOCKET;
  BOOL yes = TRUE;

  if (rpc_core_tcp_init_() != 0) {
    return (rpc_core_socket_t)-1;
  }
  if (host == NULL) {
    host = "127.0.0.1";
  }
  snprintf(port_text, sizeof(port_text), "%u", (unsigned)port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(host, port_text, &hints, &result) != 0) {
    return (rpc_core_socket_t)-1;
  }
  for (it = result; it != NULL; it = it->ai_next) {
    socket_value = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (socket_value == INVALID_SOCKET) {
      continue;
    }
    (void)setsockopt(socket_value, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    if (bind(socket_value, it->ai_addr, (int)it->ai_addrlen) == 0 && listen(socket_value, 16) == 0) {
      break;
    }
    closesocket(socket_value);
    socket_value = INVALID_SOCKET;
  }
  freeaddrinfo(result);
  return rpc_core_tcp_fd_(socket_value);
}

rpc_core_socket_t rpc_core_tcp_accept(rpc_core_socket_t listen_fd) {
  SOCKET socket_value = accept(rpc_core_tcp_socket_(listen_fd), NULL, NULL);
  return rpc_core_tcp_fd_(socket_value);
}

int rpc_core_tcp_local_port(rpc_core_socket_t fd, uint16_t* port) {
  struct sockaddr_storage addr;
  int len = sizeof(addr);
  if (port == NULL || getsockname(rpc_core_tcp_socket_(fd), (struct sockaddr*)&addr, &len) != 0) {
    return -1;
  }
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* in = (struct sockaddr_in*)&addr;
    *port = (uint16_t)ntohs(in->sin_port);
    return 0;
  }
  if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* in6 = (struct sockaddr_in6*)&addr;
    *port = (uint16_t)ntohs(in6->sin6_port);
    return 0;
  }
  return -1;
}

int rpc_core_tcp_send_package(rpc_core_socket_t fd, const uint8_t* package, size_t package_len) {
  uint8_t* frame = NULL;
  size_t frame_len = 0;
  int result;
  if (rpc_core_stream_pack(package, package_len, &frame, &frame_len) != 0) {
    return -1;
  }
  result = rpc_core_tcp_send_all_(fd, frame, frame_len);
  rpc_core_free(frame);
  return result;
}

int rpc_core_tcp_recv_once(rpc_core_socket_t fd, rpc_core_t* rpc, uint32_t max_body_size) {
  uint8_t buffer[65536];
  rpc_core_tcp_recv_ctx_t ctx;
  rpc_core_stream_parser_t parser;
  ctx.rpc = rpc;
  ctx.count = 0;
  rpc_core_stream_parser_init(&parser, max_body_size, rpc_core_tcp_on_package_, &ctx);
  while (ctx.count == 0) {
    int n = recv(rpc_core_tcp_socket_(fd), (char*)buffer, (int)sizeof(buffer), 0);
    if (n == SOCKET_ERROR) {
      rpc_core_stream_parser_deinit(&parser);
      return -1;
    }
    if (n == 0) {
      rpc_core_stream_parser_deinit(&parser);
      return 0;
    }
    if (rpc_core_stream_parser_feed(&parser, buffer, (size_t)n) != 0) {
      rpc_core_stream_parser_deinit(&parser);
      return -1;
    }
  }
  rpc_core_stream_parser_deinit(&parser);
  return ctx.count;
}

int rpc_core_tcp_recv_loop(rpc_core_socket_t fd, rpc_core_t* rpc, uint32_t max_body_size) {
  uint8_t buffer[65536];
  rpc_core_tcp_recv_ctx_t ctx;
  rpc_core_stream_parser_t parser;
  ctx.rpc = rpc;
  ctx.count = 0;
  rpc_core_stream_parser_init(&parser, max_body_size, rpc_core_tcp_on_package_, &ctx);
  for (;;) {
    int n = recv(rpc_core_tcp_socket_(fd), (char*)buffer, (int)sizeof(buffer), 0);
    if (n == SOCKET_ERROR) {
      rpc_core_stream_parser_deinit(&parser);
      return -1;
    }
    if (n == 0) {
      rpc_core_stream_parser_deinit(&parser);
      return 0;
    }
    if (rpc_core_stream_parser_feed(&parser, buffer, (size_t)n) != 0) {
      rpc_core_stream_parser_deinit(&parser);
      return -1;
    }
  }
}

void rpc_core_tcp_close(rpc_core_socket_t fd) {
  if (fd != (rpc_core_socket_t)-1) {
    closesocket(rpc_core_tcp_socket_(fd));
  }
}
