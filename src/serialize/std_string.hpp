#pragma once

#include <string>

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
std::string serialize(const T& t) {
  return t;
}

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
std::string serialize(T&& t) {
  return std::forward<T>(t);
}

template <typename T, typename std::enable_if<std::is_same<T, std::string>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t) {
  t = std::string(data.data(), data.size());
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
