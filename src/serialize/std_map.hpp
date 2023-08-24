#pragma once

#include <map>
#include <unordered_map>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_map_like : std::false_type {};

template <typename... Args>
struct is_map_like<std::map<Args...>> : std::true_type {};

template <typename... Args>
struct is_map_like<std::unordered_map<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type = 0>
std::string serialize(const T& t);

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t);

}  // namespace RPC_CORE_NAMESPACE
