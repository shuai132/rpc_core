#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type>
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {
  uint32_t size = t.size();
  oa << size;
  for (auto& item : t) {
    serialize_oarchive tmp;
    tmp << item;
    oa << tmp;
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type>
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {
  uint32_t size;
  ia >> size;
  for (uint32_t i = 0; i < size; ++i) {
    typename T::value_type item;
    serialize_iarchive tmp;
    ia >> tmp;
    tmp >> item;
    if (tmp.error_) {
      ia.error_ = true;
      break;
    }
    t.emplace(std::move(item));
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
