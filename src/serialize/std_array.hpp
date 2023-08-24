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
std::string serialize(const T& t) {
  return {(char*)&t, sizeof(t)};
}

template <typename T, typename std::enable_if<detail::is_std_array<T>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t, size_t* cost_len = nullptr) {
  if (cost_len) {
    *cost_len = sizeof(t);
  }
  memcpy((void*)&t, data.data(), sizeof(t));
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
