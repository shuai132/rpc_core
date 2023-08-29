#pragma once

#include <bitset>
#include <string>
#include <type_traits>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_bitset : std::false_type {};

template <std::size_t N>
struct is_std_bitset<std::bitset<N>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_bitset<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  oa.data.append(t.to_string());
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_bitset<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  std::string tmp;
  tmp << ia;
  t = T(std::move(tmp));
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
