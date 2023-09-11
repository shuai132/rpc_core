#pragma once

#include <map>
#include <unordered_map>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_map_like : std::false_type {};

template <typename... Args>
struct is_std_map_like<std::map<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_map_like<std::unordered_map<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_map_like<std::multimap<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_map_like<std::unordered_multimap<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_map_like<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::size_type size(t.size());
  size >> oa;
  for (auto& item : t) {
    serialize_oarchive tmp;
    item >> tmp;
    tmp >> oa;
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_map_like<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::size_type size;
  size << ia;
  for (size_t i = 0; i < size.size; ++i) {
    typename T::value_type item;
    serialize_iarchive tmp;
    tmp << ia;
    item << tmp;
    if (tmp.error) {
      ia.error = true;
      break;
    }
    t.emplace(std::move(item));
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
