#pragma once

#include <chrono>
#include <type_traits>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_chrono_duration : std::false_type {};

template <typename... Args>
struct is_std_chrono_duration<std::chrono::duration<Args...>> : std::true_type {};

template <typename T>
struct is_std_chrono_time_point : std::false_type {};

template <typename... Args>
struct is_std_chrono_time_point<std::chrono::time_point<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_chrono_duration<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  t.count() >> oa;
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_chrono_duration<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  typename T::rep rep;
  rep << ia;
  t = T(rep);
  return ia;
}

template <typename T, typename std::enable_if<detail::is_std_chrono_time_point<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  t.time_since_epoch() >> oa;
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_chrono_time_point<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  typename T::duration duration;
  duration << ia;
  t = T(duration);
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
