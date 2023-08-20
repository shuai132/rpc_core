#pragma once

#include <cstring>

namespace RpcCore {

template <typename T, typename std::enable_if<std::is_trivial<T>::value && !std::is_same<T, Void>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  return {(char*)&t, sizeof(t)};
}

template <typename T, typename std::enable_if<std::is_trivial<T>::value && !std::is_same<T, Void>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  memcpy((void*)&t, data.data(), sizeof(t));
  return true;
}

}  // namespace RpcCore
