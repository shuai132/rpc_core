#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>

namespace rpc_core {
namespace detail {

template <typename int_impl_t>
struct auto_size_type {
  explicit auto_size_type(int_impl_t value = 0) : value(value) {}

  std::string serialize() const {
    if (value == 0) {
      return {(char*)&value, 1};
    }

    uint8_t effective_bytes = sizeof(int_impl_t);
    using unsigned_type = typename std::make_unsigned<int_impl_t>::type;
    auto value_tmp = static_cast<unsigned_type>(value);
    if (value < 0) {
      value_tmp = unsigned_type(0) - value_tmp;
    }
    for (int i = sizeof(int_impl_t) - 1; i >= 0; --i) {
      if ((value_tmp >> (i * 8)) & 0xff) {
        break;
      } else {
        --effective_bytes;
      }
    }

    std::string ret;
    ret.resize(1 + effective_bytes);
    auto data = (uint8_t*)ret.data();
    data[0] = effective_bytes;
    memcpy(data + 1, &value_tmp, effective_bytes);
    if (value < 0) {
      data[0] |= 0x80;
    }
    return ret;
  }

  // Returns zero for truncated or unrepresentable input.
  int deserialize(const void* data, size_t size) {
    value = 0;
    if (!data || size == 0) return 0;
    auto p = static_cast<const uint8_t*>(data);
    const bool negative = (p[0] & 0x80) != 0;
    const size_t size_bytes = p[0] & 0x7f;
    if (size_bytes > sizeof(value) || size_bytes > size - 1) return 0;

    using unsigned_type = typename std::make_unsigned<int_impl_t>::type;
    unsigned_type magnitude = 0;
    memcpy(&magnitude, p + 1, size_bytes);
    const auto max = static_cast<unsigned_type>(std::numeric_limits<int_impl_t>::max());
    if (negative) {
      if (!std::is_signed<int_impl_t>::value || magnitude > max + unsigned_type(1)) return 0;
      // Avoid negating the minimum signed integer.
      if (magnitude != 0) value = -static_cast<int_impl_t>(magnitude - 1) - 1;
    } else {
      if (magnitude > max) return 0;
      value = static_cast<int_impl_t>(magnitude);
    }
    return static_cast<int>(size_bytes + 1);
  }

  int_impl_t value;
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
