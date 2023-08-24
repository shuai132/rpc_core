#pragma once

#include <array>
#include <cstring>
#include <type_traits>

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
std::string serialize(const T& t) {
  auto data = (uint64_t)t;
  return {(char*)&data, sizeof(uint64_t)};
}

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t, std::size_t* cost_len = nullptr) {
  uint64_t d = *(uint64_t*)(data.data());
  if (cost_len) {
    *cost_len = sizeof(d);
  }
  t = (T)d;
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
