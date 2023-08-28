#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  std::tie(t.first, t.second) >> oa;
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using first_type = detail::remove_cvref_t<decltype(t.first)>;
  using second_type = detail::remove_cvref_t<decltype(t.second)>;
  auto& tt = (std::pair<first_type, second_type>&)t;
  std::tuple<first_type, second_type> tup;
  tup << ia;
  std::tie(tt.first, tt.second) = std::move(tup);
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
