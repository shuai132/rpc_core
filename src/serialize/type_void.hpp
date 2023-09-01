#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
