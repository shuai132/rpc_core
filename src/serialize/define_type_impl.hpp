#pragma once

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_fundamental<T>::value, int>::type>
inline serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  oa << t;
  return oa;
}

template <typename T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type>
inline serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  serialize_oarchive tmp;
  tmp << t;
  oa << tmp;
  return oa;
}

template <class T, typename std::enable_if<std::is_fundamental<T>::value, int>::type>
inline serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error_) return ia;
  ia >> t;
  return ia;
}

template <class T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error_) return ia;
  uint32_t size = *(uint32_t*)(ia.data_);
  ia.data_ += sizeof(uint32_t);

  serialize_iarchive tmp(detail::string_view(ia.data_, size));
  tmp >> t;
  ia.error_ = tmp.error_;

  ia.data_ += size;
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
