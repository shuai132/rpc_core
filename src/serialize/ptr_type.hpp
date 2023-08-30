#pragma once

#include <cstdint>
#include <type_traits>

#include "raw_type.hpp"

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  auto ptr = (intptr_t)t;
  ptr >> oa;
  return oa;
}

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  intptr_t ptr;
  ptr << ia;
  t = (T)ptr;
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
