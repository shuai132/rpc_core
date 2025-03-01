#pragma once

#include <algorithm>
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

// Forming a pointer to the protected member through a derived type is legal;
// applying it to the base object does not assume an object layout.
template <typename T>
struct adaptor_access : T {
  static auto container(T& t) -> typename T::container_type& { return t.*&adaptor_access::c; }
  static auto container(const T& t) -> const typename T::container_type& { return t.*&adaptor_access::c; }
  static void restore_heap(T&) {}
};

template <typename V, typename C, typename Compare>
struct adaptor_access<std::priority_queue<V, C, Compare>> : std::priority_queue<V, C, Compare> {
  using T = std::priority_queue<V, C, Compare>;
  static C& container(T& t) { return t.*&adaptor_access::c; }
  static const C& container(const T& t) { return t.*&adaptor_access::c; }
  static void restore_heap(T& t) {
    auto& items = container(t);
    std::make_heap(items.begin(), items.end(), t.*&adaptor_access::comp);
  }
};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_stack<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::adaptor_access<T>::container(t) >> oa;
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_stack<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::adaptor_access<T>::container(t) << ia;
  detail::adaptor_access<T>::restore_heap(t);
  return ia;
}

}  // namespace rpc_core
