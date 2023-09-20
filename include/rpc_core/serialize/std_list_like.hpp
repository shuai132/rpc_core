#pragma once

#include <deque>
#include <list>
#include <vector>

namespace rpc_core {

namespace detail {

template <typename T>
struct is_std_list_like : std::false_type {};

template <typename... Args>
struct is_std_list_like<std::vector<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_list_like<std::list<Args...>> : std::true_type {};

template <typename... Args>
struct is_std_list_like<std::deque<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_list_like<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::auto_size size(t.size());
  size >> oa;
  for (auto& item : t) {
    if (std::is_fundamental<detail::remove_cvref_t<decltype(item)>>::value) {
      item >> oa;
    } else {
      serialize_oarchive tmp;
      item >> tmp;
      tmp >> oa;
    }
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_list_like<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::auto_size size;
  size << ia;
  for (size_t i = 0; i < size.value; ++i) {
    typename T::value_type item;
    if (std::is_fundamental<detail::remove_cvref_t<decltype(item)>>::value) {
      item << ia;
    } else {
      serialize_iarchive tmp;
      tmp << ia;
      item << tmp;
      if (tmp.error) {
        ia.error = true;
        break;
      }
    }
    t.emplace_back(std::move(item));
  }
  return ia;
}

}  // namespace rpc_core
