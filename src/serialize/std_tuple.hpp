#pragma once

#include <tuple>

#include "../detail/callable/helpers.hpp"

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <std::size_t I, class T>
using tuple_element_t = detail::remove_cvref_t<typename std::tuple_element<I, detail::remove_cvref_t<T>>::type>;

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t);

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t);

}  // namespace RPC_CORE_NAMESPACE
