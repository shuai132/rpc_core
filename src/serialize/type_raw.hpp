#pragma once

#include <cstring>
#include <type_traits>

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

namespace RPC_CORE_NAMESPACE {

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(bool, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(char, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(int8_t, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(uint8_t, 1);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(int16_t, 2);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(uint16_t, 2);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(int32_t, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(uint32_t, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(int64_t, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(uint64_t, 8);

RPC_CORE_DETAIL_DEFINE_RAW_TYPE(float, 4);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(double, 8);
RPC_CORE_DETAIL_DEFINE_RAW_TYPE(long double, 16);

}  // namespace RPC_CORE_NAMESPACE
