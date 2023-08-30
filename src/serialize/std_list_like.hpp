#pragma once

#include <deque>
#include <list>
#include <vector>

namespace RPC_CORE_NAMESPACE {

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
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa);

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia);

}  // namespace RPC_CORE_NAMESPACE
