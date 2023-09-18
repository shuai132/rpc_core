#pragma once

#include <queue>
#include <stack>

namespace rpc_core {

namespace detail {

template <typename T>
struct is_std_stack : std::false_type {};

template <typename... Args>
struct is_std_stack<std::stack<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_stack<std::queue<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_stack<std::priority_queue<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_stack<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  (typename T::container_type&)t >> oa;
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_stack<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  (typename T::container_type&)t << ia;
  return ia;
}

}  // namespace rpc_core
