#pragma once

#include <array>
#include <cstring>
#include <type_traits>

#define RpcCore_DETAIL_DEFINE_RAW_TYPE(type_raw, type_size)                                         \
  template <typename T, typename std::enable_if<std::is_same<T, type_raw>::value, int>::type = 0>   \
  inline std::string serialize(const T& t) {                                                        \
    return {(char*)&t, type_size};                                                                  \
  }                                                                                                 \
  template <typename T, typename std::enable_if<std::is_same<T, type_raw>::value, int>::type = 0>   \
  inline bool deserialize(const detail::string_view& data, T& t, std::size_t* cost_len = nullptr) { \
    memcpy(&t, data.data(), type_size);                                                             \
    if (cost_len) {                                                                                 \
      *cost_len = type_size;                                                                        \
    }                                                                                               \
    return true;                                                                                    \
  }

namespace RpcCore {

RpcCore_DETAIL_DEFINE_RAW_TYPE(bool, 1);
RpcCore_DETAIL_DEFINE_RAW_TYPE(char, 1);
RpcCore_DETAIL_DEFINE_RAW_TYPE(signed char, 1);
RpcCore_DETAIL_DEFINE_RAW_TYPE(unsigned char, 1);
RpcCore_DETAIL_DEFINE_RAW_TYPE(signed short, 2);
RpcCore_DETAIL_DEFINE_RAW_TYPE(unsigned short, 2);
RpcCore_DETAIL_DEFINE_RAW_TYPE(int, 4);
RpcCore_DETAIL_DEFINE_RAW_TYPE(unsigned int, 4);
RpcCore_DETAIL_DEFINE_RAW_TYPE(long, 8);
RpcCore_DETAIL_DEFINE_RAW_TYPE(unsigned long, 8);
RpcCore_DETAIL_DEFINE_RAW_TYPE(long long, 8);
RpcCore_DETAIL_DEFINE_RAW_TYPE(unsigned long long, 8);

RpcCore_DETAIL_DEFINE_RAW_TYPE(float, 4);
RpcCore_DETAIL_DEFINE_RAW_TYPE(double, 8);
RpcCore_DETAIL_DEFINE_RAW_TYPE(long double, 8);

}  // namespace RpcCore
