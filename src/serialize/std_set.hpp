#pragma once

#include <set>
#include <unordered_set>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_set_like : std::false_type {};

template <typename... Args>
struct is_set_like<std::set<Args...>> : std::true_type {};

template <typename... Args>
struct is_set_like<std::unordered_set<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_set_like<T>::value, int>::type = 0>
std::string serialize(const T& t);

template <typename T, typename std::enable_if<detail::is_set_like<T>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t);

}  // namespace RPC_CORE_NAMESPACE
