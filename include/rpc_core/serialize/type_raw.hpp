#pragma once

#include <cstring>
#include <type_traits>

#include "detail/auto_size.hpp"

#define RPC_CORE_DETAIL_DEFINE_RAW_TYPE(type_raw, type_size)                         \
  static_assert(sizeof(type_raw) <= type_size, "");                                  \
  inline serialize_oarchive& operator>>(const type_raw& t, serialize_oarchive& oa) { \
    oa.data.append((char*)&t, type_size);                                            \
    return oa;                                                                       \
  }                                                                                  \
  inline serialize_iarchive& operator<<(type_raw& t, serialize_iarchive& ia) {       \
    t = {};                                                                          \
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
    type_auto impl;                                                                  \
    impl << ia;                                                                      \
    t = impl.value;                                                                  \
    return ia;                                                                       \
  }

namespace rpc_core {

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(bool, 1);
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

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(float, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(double, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(long double, 16);

}  // namespace rpc_core
