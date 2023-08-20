#pragma once

#include <tuple>

namespace RpcCore {

namespace detail {

template <std::size_t I, class T>
using tuple_element_t = typename std::tuple_element<I, typename std::remove_cv<typename std::remove_reference<T>::type>::type>::type;

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
inline std::string serialize(const T& t);

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t);

}  // namespace RpcCore
