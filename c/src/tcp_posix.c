#include "rpc_core_c/tcp.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "rpc_core_c/stream.h"

typedef struct rpc_core_tcp_recv_ctx {
  rpc_core_t* rpc;
  int count;
} rpc_core_tcp_recv_ctx_t;

static void rpc_core_tcp_on_package_(const uint8_t* package, size_t package_len, void* user) {
  rpc_core_tcp_recv_ctx_t* ctx = (rpc_core_tcp_recv_ctx_t*)user;
  if (ctx == NULL) {
    return;
  }
  (void)rpc_core_on_package(ctx->rpc, package, package_len);
  ctx->count++;
}

static int rpc_core_tcp_send_all_(int fd, const uint8_t* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
#ifdef MSG_NOSIGNAL
    ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
#else
    ssize_t n = send(fd, data + sent, len - sent, 0);
#endif
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (n == 0) {
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

int rpc_core_tcp_connect(const char* host, uint16_t port) {
  char port_text[16];
  struct addrinfo hints;
  struct addrinfo* result = NULL;
  struct addrinfo* it;
  int fd = -1;

  if (host == NULL) {
    host = "127.0.0.1";
  }
  snprintf(port_text, sizeof(port_text), "%u", (unsigned)port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  if (getaddrinfo(host, port_text, &hints, &result) != 0) {
    return -1;
  }
  for (it = result; it != NULL; it = it->ai_next) {
    fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0) {
      continue;
    }
    if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(result);
  return fd;
}

int rpc_core_tcp_listen(const char* host, uint16_t port) {
  char port_text[16];
  struct addrinfo hints;
  struct addrinfo* result = NULL;
  struct addrinfo* it;
  int fd = -1;
  int yes = 1;

  if (host == NULL) {
    host = "127.0.0.1";
  }
  snprintf(port_text, sizeof(port_text), "%u", (unsigned)port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(host, port_text, &hints, &result) != 0) {
    return -1;
  }
  for (it = result; it != NULL; it = it->ai_next) {
    fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0) {
      continue;
    }
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind(fd, it->ai_addr, it->ai_addrlen) == 0 && listen(fd, 16) == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(result);
  return fd;
}

int rpc_core_tcp_accept(int listen_fd) {
  for (;;) {
    int fd = accept(listen_fd, NULL, NULL);
    if (fd >= 0) {
      return fd;
    }
    if (errno != EINTR) {
      return -1;
    }
  }
}

int rpc_core_tcp_local_port(int fd, uint16_t* port) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (port == NULL || getsockname(fd, (struct sockaddr*)&addr, &len) != 0) {
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

int rpc_core_tcp_send_package(int fd, const uint8_t* package, size_t package_len) {
  uint8_t* frame = NULL;
  size_t frame_len = 0;
  int result;
  (void)signal(SIGPIPE, SIG_IGN);
  if (rpc_core_stream_pack(package, package_len, &frame, &frame_len) != 0) {
    return -1;
  }
  result = rpc_core_tcp_send_all_(fd, frame, frame_len);
  rpc_core_free(frame);
  return result;
}

int rpc_core_tcp_recv_once(int fd, rpc_core_t* rpc, uint32_t max_body_size) {
  uint8_t buffer[65536];
  rpc_core_tcp_recv_ctx_t ctx;
  rpc_core_stream_parser_t parser;
  ctx.rpc = rpc;
  ctx.count = 0;
  rpc_core_stream_parser_init(&parser, max_body_size, rpc_core_tcp_on_package_, &ctx);
  while (ctx.count == 0) {
    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
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

int rpc_core_tcp_recv_loop(int fd, rpc_core_t* rpc, uint32_t max_body_size) {
  uint8_t buffer[65536];
  rpc_core_tcp_recv_ctx_t ctx;
  rpc_core_stream_parser_t parser;
  ctx.rpc = rpc;
  ctx.count = 0;
  rpc_core_stream_parser_init(&parser, max_body_size, rpc_core_tcp_on_package_, &ctx);
  for (;;) {
    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
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

void rpc_core_tcp_close(int fd) {
  if (fd >= 0) {
    close(fd);
  }
}
