#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
