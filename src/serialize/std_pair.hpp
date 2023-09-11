#pragma once

#include <tuple>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_pair : std::false_type {};

template <typename... Args>
struct is_std_pair<std::pair<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  std::tie(t.first, t.second) >> oa;
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using first_type = detail::remove_cvref_t<decltype(t.first)>;
  using second_type = detail::remove_cvref_t<decltype(t.second)>;
  auto& tt = (std::pair<first_type, second_type>&)t;
  std::tuple<first_type, second_type> tup;
  tup << ia;
  std::tie(tt.first, tt.second) = std::move(tup);
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
