#pragma once

namespace rpc_core {

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  return ia;
}

}  // namespace rpc_core
