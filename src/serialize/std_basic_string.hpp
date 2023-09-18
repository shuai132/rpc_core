#pragma once

#include <string>

namespace rpc_core {

namespace detail {

template <typename T>
struct is_std_basic_string : std::false_type {};

template <typename... Args>
struct is_std_basic_string<std::basic_string<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  using VT = typename T::value_type;
  oa.data.append((char*)t.data(), t.size() * sizeof(VT));
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
serialize_oarchive& operator<<(rpc_core::serialize_oarchive& oa, T&& t) {
  using VT = typename T::value_type;
  if (oa.data.empty()) {
    oa.data = std::forward<T>(t);
    return oa;
  }
  oa.data.append((char*)t.data(), t.size() * sizeof(VT));
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using VT = typename T::value_type;
  t = T((VT*)(ia.data), ia.size / sizeof(VT));
  return ia;
}

}  // namespace rpc_core
