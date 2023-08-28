#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  uint32_t size = t.size();
  size >> oa;
  for (auto& item : t) {
    serialize_oarchive tmp;
    item >> tmp;
    tmp >> oa;
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  uint32_t size;
  size << ia;
  for (uint32_t i = 0; i < size; ++i) {
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
