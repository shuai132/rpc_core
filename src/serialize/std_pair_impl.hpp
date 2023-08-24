#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type>
std::string serialize(const T& t) {
  return serialize(std::tie(t.first, t.second));
}

template <typename T, typename std::enable_if<detail::is_std_pair<T>::value, int>::type>
bool deserialize(const detail::string_view& data, T& t) {
  using first_type = detail::remove_cvref_t<decltype(t.first)>;
  using second_type = detail::remove_cvref_t<decltype(t.second)>;
  auto& tt = (std::pair<first_type, second_type>&)t;
  std::tuple<first_type, second_type> tup;
  if (!deserialize(data, tup)) {
    return false;
  }
  std::tie(tt.first, tt.second) = std::move(tup);
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
