#pragma once

#include <chrono>
#include <limits>
#include <type_traits>

namespace rpc_core {

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
  if (std::is_floating_point<typename T::rep>::value) {
    t.count() >> oa;
  } else {
    // Positive counts share the same wire format, including unsigned values
    // above INTMAX_MAX. Do not narrow those through a signed intermediate.
    using Integer = typename std::conditional<std::is_signed<typename T::rep>::value, intmax_t, uintmax_t>::type;
    detail::auto_size_type<Integer> count(t.count());
    count >> oa;
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_chrono_duration<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  if (std::is_floating_point<typename T::rep>::value) {
    typename T::rep rep{};
    rep << ia;
    if (!ia.error) t = T(rep);
  } else {
    using Rep = typename T::rep;
    using Integer = typename std::conditional<std::is_signed<Rep>::value, intmax_t, uintmax_t>::type;
    detail::auto_size_type<Integer> rep;
    rep << ia;
    if (ia.error) return ia;
    if (rep.value < (std::numeric_limits<Rep>::min)() || rep.value > (std::numeric_limits<Rep>::max)()) {
      ia.error = true;
      return ia;
    }
    t = T(static_cast<Rep>(rep.value));
  }
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
  if (!ia.error) t = T(duration);
  return ia;
}

}  // namespace rpc_core
