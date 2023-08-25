#pragma once

#include <deque>
#include <list>
#include <queue>
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
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t);

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type = 0>
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t);

}  // namespace RPC_CORE_NAMESPACE
