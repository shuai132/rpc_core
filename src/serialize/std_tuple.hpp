#pragma once

#include <tuple>

#include "../detail/callable/helpers.hpp"

namespace RpcCore {

namespace detail {

template <std::size_t I, class T>
using tuple_element_t = detail::remove_cvref_t<typename std::tuple_element<I, detail::remove_cvref_t<T>>::type>;

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
