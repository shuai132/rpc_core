#pragma once

#include <array>
#include <cstring>
#include <type_traits>

namespace RpcCore {

namespace detail {

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
struct is_basic_type {
  //  static constexpr bool value = std::is_fundamental<T>::value || std::is_array<T>::value || is_std_array<T>::value || std::is_same<T,
  //  decltype(std::ignore)>::value;
  static constexpr bool value = std::is_pod<T>::value;
};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_basic_type<T>::value && !std::is_same<T, Void>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  return {(char*)&t, sizeof(t)};
}

template <typename T, typename std::enable_if<detail::is_basic_type<T>::value && !std::is_same<T, Void>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  memcpy((void*)&t, data.data(), sizeof(t));
  return true;
}

}  // namespace RpcCore
