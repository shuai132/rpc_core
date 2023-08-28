#pragma once

#include <array>
#include <cstring>
#include <type_traits>

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  auto data = (uint64_t)t;
  oa.data.append((char*)&data, sizeof(uint64_t));
  return oa;
}

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  uint64_t d = *(uint64_t*)(ia.data);
  t = (T)d;
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
