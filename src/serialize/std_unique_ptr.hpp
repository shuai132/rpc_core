#pragma once

#include <memory>

namespace rpc_core {

namespace detail {

template <typename T>
struct is_std_unique_ptr : std::false_type {};

template <typename... Args>
struct is_std_unique_ptr<std::unique_ptr<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_unique_ptr<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  if (t != nullptr) {
    true >> oa;
    *t >> oa;
  } else {
    false >> oa;
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_unique_ptr<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  bool notnull;
  notnull << ia;
  if (notnull) {
    using Type = typename T::element_type;
    t = std::unique_ptr<Type>(new Type);
    *t << ia;
  }
  return ia;
}

}  // namespace rpc_core
