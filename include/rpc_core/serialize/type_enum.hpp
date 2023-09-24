#pragma once

namespace rpc_core {

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::auto_uintmax impl((uintmax_t)t);
  impl >> oa;
  return oa;
}

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::auto_uintmax impl;
  impl << ia;
  t = (T)impl.value;
  return ia;
}

}  // namespace rpc_core
