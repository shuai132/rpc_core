#pragma once

#include <tuple>

namespace RpcCore {

namespace detail {

template <typename T>
struct is_std_pair : std::false_type {};

template <typename... Args>
struct is_std_pair<std::pair<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type = 0>
inline std::string serialize(const T& t);

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t);

}  // namespace RpcCore
