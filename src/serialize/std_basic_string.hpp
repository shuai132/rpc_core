#pragma once

#include <string>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_basic_string : std::false_type {};

template <typename... Args>
struct is_std_basic_string<std::basic_string<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  oa.data.append(t);
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
serialize_oarchive& operator<<(rpc_core::serialize_oarchive& oa, T&& t) {
  if (oa.data.empty()) {
    oa.data = std::forward<T>(t);
    return oa;
  }
  oa.data.append(t);
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  t = std::string(ia.data, ia.size);
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
