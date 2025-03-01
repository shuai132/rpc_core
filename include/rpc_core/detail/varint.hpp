#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rpc_core {
namespace detail {

static const uint8_t MSB = 0x80;

inline uint8_t* varint_encode(uint32_t n, uint8_t* buf, uint8_t* bytes) {
  uint8_t* ptr = buf;
  while (n >= MSB) {
    *(ptr++) = static_cast<uint8_t>(n & 0x7f) | MSB;
    n = n >> 7;
  }
  *ptr = n;
  *bytes = ptr - buf + 1;
  return buf;
}

inline std::string to_varint(uint32_t var) {
  uint8_t buf[sizeof(uint32_t) + 1];  // enough
  uint8_t bytes;
  varint_encode(var, buf, &bytes);
  return {(char*)buf, bytes};
}

// Decode a uint32_t without reading past the supplied buffer.
inline bool from_varint(const char* data, size_t size, uint32_t& value, size_t& bytes) {
  value = 0;
  bytes = 0;
  if (data == nullptr) return false;
  for (size_t i = 0; i < 5 && i < size; ++i) {
    const uint8_t byte = static_cast<uint8_t>(data[i]);
    if (i == 4 && (byte & 0xf0) != 0) return false;
    value |= static_cast<uint32_t>(byte & 0x7f) << (7 * i);
    if ((byte & MSB) == 0) {
      bytes = i + 1;
      return true;
    }
  }
  return false;
}

}  // namespace detail
}  // namespace rpc_core
