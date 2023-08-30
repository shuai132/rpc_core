#pragma once

#include <array>

#include "src/detail/callable/helpers.hpp"

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_array : std::false_type {};

template <class T, size_t Size>
struct is_std_array<std::array<T, Size>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_array<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  uint32_t size = t.size();
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

template <typename T, typename std::enable_if<detail::is_std_array<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  uint32_t size;
  size << ia;
  for (uint32_t i = 0; i < size; ++i) {
    typename T::value_type item;
    if (std::is_fundamental<detail::remove_cvref_t<decltype(item)>>::value) {
      item << ia;
      if (ia.error) break;
    } else {
      serialize_iarchive tmp;
      tmp << ia;
      item << tmp;
      if (tmp.error) {
        ia.error = true;
        break;
      }
    }
    t[i] = std::move(item);
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
