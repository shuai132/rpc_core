#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace rpc_core {
namespace detail {

static const uint8_t MSB = 0x80;
static const uint8_t MSB_ALL = ~0x7F;

inline uint8_t* varint_encode(unsigned long long n, uint8_t* buf, uint8_t* bytes) {
  uint8_t* ptr = buf;
  while (n & MSB_ALL) {
    *(ptr++) = (n & 0xFF) | MSB;
    n = n >> 7;
  }
  *ptr = n;
  *bytes = ptr - buf + 1;
  return buf;
}

inline unsigned long long varint_decode(uint8_t* buf, uint8_t* bytes) {
  unsigned long long result = 0;
  int bits = 0;
  uint8_t* ptr = buf;
  unsigned long long ll;
  while (*ptr & MSB) {
    ll = *ptr;
    result += ((ll & 0x7F) << bits);
    ptr++;
    bits += 7;
  }
  ll = *ptr;
  result += ((ll & 0x7F) << bits);
  *bytes = ptr - buf + 1;
  return result;
}

inline std::string to_varint(uint32_t var) {
  uint8_t buf[sizeof(uint32_t) + 1];  // enough
  std::string ret;
  uint8_t bytes;
  varint_encode(var, buf, &bytes);
  return {(char*)buf, bytes};
}

inline uint32_t from_varint(void* data, uint8_t* bytes) {
  return varint_decode((uint8_t*)data, bytes);
  ;
}

}  // namespace detail
}  // namespace rpc_core
