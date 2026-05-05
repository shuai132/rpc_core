#include "rpc_core_c/stream.h"

#include <stdlib.h>
#include <string.h>

#include "rpc_core_c/rpc_core.h"

static uint32_t rpc_core_stream_read_u32_le_(const uint8_t* data) {
  return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void rpc_core_stream_write_u32_le_(uint8_t* data, uint32_t value) {
  data[0] = (uint8_t)(value & 0xffu);
  data[1] = (uint8_t)((value >> 8) & 0xffu);
  data[2] = (uint8_t)((value >> 16) & 0xffu);
  data[3] = (uint8_t)((value >> 24) & 0xffu);
}

static int rpc_core_stream_reserve_(rpc_core_stream_parser_t* parser, size_t needed) {
  uint8_t* next;
  size_t cap;
  if (needed <= parser->buffer_cap) {
    return 0;
  }
  cap = parser->buffer_cap == 0 ? 1024 : parser->buffer_cap;
  while (cap < needed) {
    cap *= 2;
  }
  next = (uint8_t*)realloc(parser->buffer, cap);
  if (next == NULL) {
    return -1;
  }
  parser->buffer = next;
  parser->buffer_cap = cap;
  return 0;
}

int rpc_core_stream_pack(const uint8_t* package, size_t package_len, uint8_t** out, size_t* out_len) {
  uint8_t* data;
  if (out == NULL || out_len == NULL || package_len > 0xffffffffu) {
    return -1;
  }
  *out_len = package_len + 4;
  data = (uint8_t*)rpc_core_malloc(*out_len);
  if (data == NULL) {
    return -1;
  }
  rpc_core_stream_write_u32_le_(data, (uint32_t)package_len);
  if (package_len > 0 && package != NULL) {
    memcpy(data + 4, package, package_len);
  }
  *out = data;
  return 0;
}

void rpc_core_stream_parser_init(rpc_core_stream_parser_t* parser, uint32_t max_body_size, rpc_core_stream_package_fn fn, void* user) {
  if (parser == NULL) {
    return;
  }
  memset(parser, 0, sizeof(*parser));
  parser->max_body_size = max_body_size == 0 ? 0xffffffffu : max_body_size;
  parser->on_package = fn;
  parser->user = user;
}

void rpc_core_stream_parser_deinit(rpc_core_stream_parser_t* parser) {
  if (parser == NULL) {
    return;
  }
  free(parser->buffer);
  memset(parser, 0, sizeof(*parser));
}

void rpc_core_stream_parser_reset(rpc_core_stream_parser_t* parser) {
  if (parser == NULL) {
    return;
  }
  parser->buffer_len = 0;
  parser->body_size = 0;
  parser->has_body_size = 0;
}

int rpc_core_stream_parser_feed(rpc_core_stream_parser_t* parser, const uint8_t* data, size_t len) {
  if (parser == NULL || (data == NULL && len > 0)) {
    return -1;
  }
  if (len == 0) {
    return 0;
  }
  if (rpc_core_stream_reserve_(parser, parser->buffer_len + len) != 0) {
    return -1;
  }
  memcpy(parser->buffer + parser->buffer_len, data, len);
  parser->buffer_len += len;

  for (;;) {
    if (!parser->has_body_size) {
      if (parser->buffer_len < 4) {
        return 0;
      }
      parser->body_size = rpc_core_stream_read_u32_le_(parser->buffer);
      if (parser->body_size > parser->max_body_size) {
        rpc_core_stream_parser_reset(parser);
        return -1;
      }
      memmove(parser->buffer, parser->buffer + 4, parser->buffer_len - 4);
      parser->buffer_len -= 4;
      parser->has_body_size = 1;
    }

    if (parser->buffer_len < parser->body_size) {
      return 0;
    }

    if (parser->on_package != NULL) {
      parser->on_package(parser->buffer, parser->body_size, parser->user);
    }
    memmove(parser->buffer, parser->buffer + parser->body_size, parser->buffer_len - parser->body_size);
    parser->buffer_len -= parser->body_size;
    parser->body_size = 0;
    parser->has_body_size = 0;
  }
}
