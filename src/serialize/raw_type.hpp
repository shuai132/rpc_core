#pragma once

#include <cstring>
#include <type_traits>

#define RPC_CORE_DETAIL_DEFINE_RAW_TYPE(type_raw, type_size)                                      \
  static_assert(sizeof(type_raw) <= type_size, "");                                               \
  template <typename T, typename std::enable_if<std::is_same<T, type_raw>::value, int>::type = 0> \
  serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {                            \
    oa.data.append((char*)&t, type_size);                                                         \
    return oa;                                                                                    \
  }                                                                                               \
  template <typename T, typename std::enable_if<std::is_same<T, type_raw>::value, int>::type = 0> \
  serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {                                  \
    t = {};                                                                                       \
    memcpy(&t, ia.data, detail::min<size_t>(sizeof(t), type_size));                               \
    ia.data += type_size;                                                                         \
    ia.size -= type_size;                                                                         \
    return ia;                                                                                    \
  }

namespace RPC_CORE_NAMESPACE {

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(bool, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(signed char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(signed short, 2);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned short, 2);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(int, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned int, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(long, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned long, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(long long, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(unsigned long long, 8);

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(float, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(double, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(long double, 16);

}  // namespace RPC_CORE_NAMESPACE
