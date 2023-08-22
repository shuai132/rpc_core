#pragma once

namespace RpcCore {

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  return {};
}

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  return true;
}

}  // namespace RpcCore
