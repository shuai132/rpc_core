#pragma once

#include <forward_list>

#include "../detail/callable/helpers.hpp"

namespace rpc_core {

namespace detail {

template <typename T>
struct is_std_forward_list : std::false_type {};

template <typename... Args>
struct is_std_forward_list<std::forward_list<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_forward_list<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::auto_size size(std::distance(t.cbegin(), t.cend()));
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

template <typename T, typename std::enable_if<detail::is_std_forward_list<T>::value, int>::type = 0>
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
    t.emplace_front(std::move(item));
  }
  t.reverse();
  return ia;
}

}  // namespace rpc_core
