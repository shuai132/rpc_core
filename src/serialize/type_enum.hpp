#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  oa.data.append((char*)&t, sizeof(T));
  return oa;
}

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  t = {};
  memcpy(&t, ia.data, detail::min<size_t>(sizeof(t), sizeof(T)));
  ia.data += 1;
  ia.size -= 1;
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
