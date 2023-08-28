#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_fundamental<T>::value, int>::type>
inline serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  t >> oa;
  return oa;
}

template <typename T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type>
inline serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  serialize_oarchive tmp;
  t >> tmp;
  tmp >> oa;
  return oa;
}

template <class T, typename std::enable_if<std::is_fundamental<T>::value, int>::type>
inline serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error) return ia;
  t << ia;
  return ia;
}

template <class T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error) return ia;
  uint32_t size = *(uint32_t*)(ia.data);
  ia.data += sizeof(uint32_t);

  serialize_iarchive tmp(detail::string_view(ia.data, size));
  t << tmp;
  ia.error = tmp.error;

  ia.data += size;
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
