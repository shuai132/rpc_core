#pragma once

namespace RpcCore {

template <typename T, typename std::enable_if<std::is_fundamental<T>::value, int>::type>
serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  oa.ss_.append(RpcCore::serialize(t));
  return oa;
}

template <typename T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type>
serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  auto payload = RpcCore::serialize(t);
  uint32_t size = payload.size();
  oa.ss_.append(std::string((char*)&size, sizeof(uint32_t)));
  oa.ss_.append(std::move(payload));
  return oa;
}

template <class T, typename std::enable_if<std::is_fundamental<T>::value, int>::type>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error_) return ia;
  size_t cost_len;
  ia.error_ = !RpcCore::deserialize(detail::string_view(ia.data_, 0), t, &cost_len);
  ia.data_ += cost_len;
  return ia;
}

template <class T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error_) return ia;
  uint32_t size = *(uint32_t*)(ia.data_);
  ia.data_ += sizeof(uint32_t);
  ia.error_ = !RpcCore::deserialize(detail::string_view(ia.data_, size), t);
  ia.data_ += size;
  return ia;
}

}  // namespace RpcCore
