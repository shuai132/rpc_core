#pragma once

#include <cstring>

#include "detail/string_view.hpp"

namespace RpcCore {

template <typename T, typename std::enable_if<std::is_trivial<T>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  return {(char*)&t, sizeof(t)};
}

template <typename T, typename std::enable_if<std::is_trivial<T>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  memcpy((void*)&t, data.data(), sizeof(t));
  return true;
}

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  return t;
}

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  t = std::string(data.data(), data.size());
  return true;
}

}  // namespace RpcCore
