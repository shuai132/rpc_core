#pragma once

#include <deque>
#include <list>
#include <queue>
#include <vector>

namespace RpcCore {

namespace detail {

template <typename T>
struct is_list_like : std::false_type {};

template <typename... Args>
struct is_list_like<std::vector<Args...>> : std::true_type {};

template <typename... Args>
struct is_list_like<std::list<Args...>> : std::true_type {};

template <typename... Args>
struct is_list_like<std::deque<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type = 0>
inline std::string serialize(const T& t);

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t);

}  // namespace RpcCore
