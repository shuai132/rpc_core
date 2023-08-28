#pragma once

#include <array>
#include <cstring>
#include <type_traits>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_array<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  oa.data.append((char*)&t, sizeof(t));
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_array<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  memcpy((void*)&t, ia.data, sizeof(t));
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
