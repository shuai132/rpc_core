#pragma once

// config
#include "config.hpp"

// include
#include "detail/noncopyable.hpp"
#include "detail/string_view.hpp"
#include "serialize/detail/auto_size.hpp"

namespace rpc_core {

struct serialize_oarchive : detail::noncopyable {
  std::string data;
};

inline serialize_oarchive& operator>>(const serialize_oarchive& t, serialize_oarchive& oa) {
  oa.data.append(detail::auto_size(t.data.size()).serialize());
  oa.data.append(t.data);
  return oa;
}

inline serialize_oarchive& operator>>(serialize_oarchive&& t, serialize_oarchive& oa) {
  if (oa.data.empty()) {
    oa.data = std::move(t.data);
    return oa;
  }
  oa.data.append(detail::auto_size(t.data.size()).serialize());
  oa.data.append(t.data);
  return oa;
}

struct serialize_iarchive : detail::noncopyable {
  serialize_iarchive() = default;
  serialize_iarchive(detail::string_view sv) : data((char*)sv.data()), size(sv.size()) {}  // NOLINT(google-explicit-constructor)
  serialize_iarchive(const char* data, size_t size) : data((char*)data), size(size) {}
  char* data = nullptr;
  size_t size = 0;
  bool error = false;
};

inline serialize_iarchive& operator<<(serialize_iarchive& t, serialize_iarchive& ia) {
  detail::auto_size size;
  int cost = size.deserialize(ia.data);
  ia.data += cost;
  t.data = ia.data;
  t.size = size.value;
  ia.data += size.value;
  ia.size -= cost + size.value;
  return ia;
}

template <typename T>
inline std::string serialize(const T& t) {
  serialize_oarchive ar;
  t >> ar;
  return std::move(ar.data);
}

template <typename T>
inline bool deserialize(const detail::string_view& data, T& t) {
  serialize_iarchive ar(data);
  t << ar;
  return !ar.error;
}

template <typename T, typename std::enable_if<detail::is_auto_size_type<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  oa.data.append(t.serialize());
  return oa;
}

template <typename T, typename std::enable_if<detail::is_auto_size_type<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  int cost = t.deserialize((uint8_t*)ia.data);
  ia.data += cost;
  ia.size -= cost;
  return ia;
}

}  // namespace rpc_core
