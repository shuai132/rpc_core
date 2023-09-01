#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace RPC_CORE_NAMESPACE {
namespace detail {

struct size_type {
  explicit size_type(size_t size = 0) : size(size) {}

  std::string serialize() const {
    if (size == 0) {
      return {(char*)&size, 1};
    }

    uint8_t effective_bytes = sizeof(size_t);
    for (int i = sizeof(size_t) - 1; i >= 0; --i) {
      if ((size >> (i * 8)) & 0xff) {
        break;
      } else {
        --effective_bytes;
      }
    }

    std::string ret;
    ret.resize(1 + effective_bytes);
    auto data = (uint8_t*)ret.data();
    data[0] = effective_bytes;
    memcpy(data + 1, &size, effective_bytes);
    return ret;
  }

  int deserialize(const void* data) {
    auto p = (uint8_t*)data;
    uint8_t size_bytes = p[0];
    memcpy(&size, p + 1, size_bytes);
    return size_bytes + 1;
  }

  size_t size;
};

}  // namespace detail
}  // namespace RPC_CORE_NAMESPACE
