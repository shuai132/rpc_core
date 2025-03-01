#pragma once

#include <cstring>
#include <climits>
#include <limits>
#include <type_traits>

#include "detail/auto_size.hpp"

#define RPC_CORE_DETAIL_DEFINE_RAW_TYPE(type_raw, type_size)                         \
  static_assert(sizeof(type_raw) <= type_size, "");                                  \
  inline serialize_oarchive& operator>>(const type_raw& t, serialize_oarchive& oa) { \
    oa.data.append(reinterpret_cast<const char*>(&t), sizeof(t));                    \
    oa.data.append(type_size - sizeof(t), '\0');                                     \
    return oa;                                                                       \
  }                                                                                  \
  inline serialize_iarchive& operator<<(type_raw& t, serialize_iarchive& ia) {       \
    t = {};                                                                          \
    if (!ia.require(type_size)) return ia;                                           \
    memcpy(&t, ia.data, detail::min<size_t>(sizeof(t), type_size));                  \
    ia.data += type_size;                                                            \
    ia.size -= type_size;                                                            \
    return ia;                                                                       \
  }

#define RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(type_raw, type_auto)               \
  inline serialize_oarchive& operator>>(const type_raw& t, serialize_oarchive& oa) { \
    type_auto impl(t);                                                               \
    impl >> oa;                                                                      \
    return oa;                                                                       \
  }                                                                                  \
  inline serialize_iarchive& operator<<(type_raw& t, serialize_iarchive& ia) {       \
    t = {};                                                                          \
    type_auto impl;                                                                  \
    impl << ia;                                                                      \
    if (ia.error) return ia;                                                          \
    if (impl.value < (std::numeric_limits<type_raw>::min)() ||                         \
        impl.value > (std::numeric_limits<type_raw>::max)()) {                        \
      ia.error = true;                                                               \
      return ia;                                                                     \
    }                                                                                \
    t = (type_raw)impl.value;                                                        \
    return ia;                                                                       \
  }

namespace rpc_core {

// Use a fixed one-byte wire value instead of the platform's bool object representation.
inline serialize_oarchive& operator>>(bool t, serialize_oarchive& oa) {
  oa.data.push_back(t ? 1 : 0);
  return oa;
}

inline serialize_iarchive& operator<<(bool& t, serialize_iarchive& ia) {
  t = false;
  if (!ia.require(1)) return ia;
  t = static_cast<unsigned char>(*ia.data) != 0;
  ++ia.data;
  --ia.size;
  return ia;
}

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(signed char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(short, 2);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned short, 2);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(int, detail::auto_intmax);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(unsigned int, detail::auto_uintmax);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(long, detail::auto_intmax);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(unsigned long, detail::auto_uintmax);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(long long, detail::auto_intmax);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE_AUTO_SIZE(unsigned long long, detail::auto_uintmax);

namespace detail {
template <typename T>
struct binary_float_format {
  static constexpr bool is_float = std::is_same<T, float>::value;
  static constexpr size_t bytes = is_float ? 4 : 8;
  static constexpr bool supported = CHAR_BIT == 8 && sizeof(T) == bytes &&
      std::numeric_limits<T>::is_iec559 && std::numeric_limits<T>::radix == 2 &&
      std::numeric_limits<T>::digits == (is_float ? 24 : 53) &&
      std::numeric_limits<T>::min_exponent == (is_float ? -125 : -1021) &&
      std::numeric_limits<T>::max_exponent == (is_float ? 128 : 1024);
};
}  // namespace detail

template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  using format = detail::binary_float_format<T>;
  static_assert(format::supported,
                "unsupported floating-point format: float requires IEEE 754 binary32; double and long double require binary64");
  oa.data.append(reinterpret_cast<const char*>(&t), format::bytes);
  return oa;
}

template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using format = detail::binary_float_format<T>;
  static_assert(format::supported,
                "unsupported floating-point format: float requires IEEE 754 binary32; double and long double require binary64");
  t = {};
  if (!ia.require(format::bytes)) return ia;
  memcpy(&t, ia.data, format::bytes);
  ia.data += format::bytes;
  ia.size -= format::bytes;
  return ia;
}

}  // namespace rpc_core
