#ifndef RPC_CORE_C_STREAM_H
#define RPC_CORE_C_STREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rpc_core_stream_package_fn)(const uint8_t* package, size_t package_len, void* user);

typedef struct rpc_core_stream_parser {
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_cap;
  uint32_t body_size;
  int has_body_size;
  uint32_t max_body_size;
  rpc_core_stream_package_fn on_package;
  void* user;
} rpc_core_stream_parser_t;

int rpc_core_stream_pack(const uint8_t* package, size_t package_len, uint8_t** out, size_t* out_len);
void rpc_core_stream_parser_init(rpc_core_stream_parser_t* parser, uint32_t max_body_size, rpc_core_stream_package_fn fn, void* user);
void rpc_core_stream_parser_deinit(rpc_core_stream_parser_t* parser);
void rpc_core_stream_parser_reset(rpc_core_stream_parser_t* parser);
int rpc_core_stream_parser_feed(rpc_core_stream_parser_t* parser, const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
