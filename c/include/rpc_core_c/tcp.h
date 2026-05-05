#ifndef RPC_CORE_C_TCP_H
#define RPC_CORE_C_TCP_H

#include <stdint.h>
#include "rpc_core_c/rpc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t rpc_core_socket_t;

rpc_core_socket_t rpc_core_tcp_connect(const char* host, uint16_t port);
rpc_core_socket_t rpc_core_tcp_listen(const char* host, uint16_t port);
rpc_core_socket_t rpc_core_tcp_accept(rpc_core_socket_t listen_fd);
int rpc_core_tcp_local_port(rpc_core_socket_t fd, uint16_t* port);
int rpc_core_tcp_send_package(rpc_core_socket_t fd, const uint8_t* package, size_t package_len);
int rpc_core_tcp_recv_once(rpc_core_socket_t fd, rpc_core_t* rpc, uint32_t max_body_size);
int rpc_core_tcp_recv_loop(rpc_core_socket_t fd, rpc_core_t* rpc, uint32_t max_body_size);
void rpc_core_tcp_close(rpc_core_socket_t fd);

#ifdef __cplusplus
}
#endif

#endif
