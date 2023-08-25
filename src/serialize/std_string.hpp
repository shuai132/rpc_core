#pragma once

#include <string>

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
inline serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {
  oa.data.append(t);
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
serialize_oarchive& operator<<(rpc_core::serialize_oarchive& oa, T&& t) {
  if (oa.data.empty()) {
    oa.data = std::forward<T>(t);
    return oa;
  }
  oa.data.append(t);
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
inline serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {
  t = std::string(ia.data_, ia.size_);
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
