#pragma once

#include <set>
#include <unordered_set>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_set_like : std::false_type {};

template <typename... Args>
struct is_std_set_like<std::set<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_set_like<std::unordered_set<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_set_like<std::multiset<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_set_like<std::unordered_multiset<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_set_like<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa);

template <typename T, typename std::enable_if<detail::is_std_set_like<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia);

}  // namespace RPC_CORE_NAMESPACE
