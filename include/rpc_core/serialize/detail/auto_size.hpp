#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace rpc_core {
namespace detail {

template <typename uint_impl_t>
struct auto_size_type {
  explicit auto_size_type(uint_impl_t value = 0) : value(value) {}

  std::string serialize() const {
    if (value == 0) {
      return {(char*)&value, 1};
    }

    uint8_t effective_bytes = sizeof(uint_impl_t);
    for (int i = sizeof(uint_impl_t) - 1; i >= 0; --i) {
      if ((value >> (i * 8)) & 0xff) {
        break;
      } else {
        --effective_bytes;
      }
    }

    std::string ret;
    ret.resize(1 + effective_bytes);
    auto data = (uint8_t*)ret.data();
    data[0] = effective_bytes;
    memcpy(data + 1, &value, effective_bytes);
    return ret;
  }

  int deserialize(const void* data) {
    auto p = (uint8_t*)data;
    uint8_t size_bytes = p[0];
    memcpy(&value, p + 1, size_bytes);
    return size_bytes + 1;
  }

  uint_impl_t value;
};

using auto_size = auto_size_type<size_t>;
using auto_intmax = auto_size_type<intmax_t>;
using auto_uintmax = auto_size_type<uintmax_t>;

template <typename T>
struct is_auto_size_type : std::false_type {};

template <typename T>
struct is_auto_size_type<auto_size_type<T>> : std::true_type {};

}  // namespace detail
}  // namespace rpc_core
