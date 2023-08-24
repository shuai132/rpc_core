#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
std::string serialize(const T& t) {
  return {};
}

template <typename T, typename std::enable_if<std::is_same<T, void>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t) {
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
