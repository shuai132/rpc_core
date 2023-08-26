#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type>
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {
  uint32_t size = t.size();
  oa << size;
  for (auto& item : t) {
    if (std::is_fundamental<detail::remove_cvref_t<decltype(item)>>::value) {
      oa << item;
    } else {
      serialize_oarchive tmp;
      tmp << item;
      oa << tmp;
    }
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type>
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {
  uint32_t size;
  ia >> size;
  for (uint32_t i = 0; i < size; ++i) {
    typename T::value_type item;
    if (std::is_fundamental<detail::remove_cvref_t<decltype(item)>>::value) {
      ia >> item;
      if (ia.error) break;
    } else {
      serialize_iarchive tmp;
      ia >> tmp;
      tmp >> item;
      if (tmp.error) {
        ia.error = true;
        break;
      }
    }
    t.emplace_back(std::move(item));
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
