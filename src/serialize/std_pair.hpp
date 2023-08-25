#pragma once

#include <tuple>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_pair : std::false_type {};

template <typename... Args>
struct is_std_pair<std::pair<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type = 0>
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t);

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type = 0>
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t);

}  // namespace RPC_CORE_NAMESPACE
