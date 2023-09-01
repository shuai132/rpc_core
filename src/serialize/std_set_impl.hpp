#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<detail::is_set_like<T>::value, int>::type>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::size_type size(t.size());
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

template <typename T, typename std::enable_if<detail::is_set_like<T>::value, int>::type>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::size_type size;
  size << ia;
  for (uint32_t i = 0; i < size.size; ++i) {
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
    t.emplace(std::move(item));
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
