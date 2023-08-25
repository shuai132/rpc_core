#pragma once

// config
#include "config.hpp"

// include
#include "detail/noncopyable.hpp"
#include "detail/string_view.hpp"

namespace RPC_CORE_NAMESPACE {

struct serialize_oarchive : detail::noncopyable {
  std::string data;
};

inline serialize_oarchive& operator<<(serialize_oarchive& oa, const serialize_oarchive& t) {
  uint32_t size = t.data.size();
  oa.data.append((char*)&size, sizeof(uint32_t));
  oa.data.append(t.data);
  return oa;
}

inline serialize_oarchive& operator<<(serialize_oarchive& oa, serialize_oarchive&& t) {
  if (oa.data.empty()) {
    oa.data = std::move(t.data);
    return oa;
  }
  uint32_t size = t.data.size();
  oa.data.append((char*)&size, sizeof(uint32_t));
  oa.data.append(t.data);
  return oa;
}

struct serialize_iarchive : detail::noncopyable {
  serialize_iarchive() = default;
  serialize_iarchive(detail::string_view sv) : data_((char*)sv.data()), size_(sv.size()) {}  // NOLINT(google-explicit-constructor)
  serialize_iarchive(const char* data, size_t size) : data_((char*)data), size_(size) {}
  char* data_ = nullptr;
  size_t size_ = 0;
  bool error_ = false;
};

inline serialize_iarchive& operator>>(serialize_iarchive& ia, serialize_iarchive& t) {
  uint32_t size = *(uint32_t*)ia.data_;
  ia.data_ += sizeof(uint32_t);
  t.data_ = ia.data_;
  t.size_ = size;
  ia.data_ += size;
  ia.size_ -= sizeof(uint32_t) + size;
  return ia;
}

template <typename T>
inline std::string serialize(const T& t) {
  serialize_oarchive ar;
  ar << t;
  return std::move(ar.data);
}

template <typename T>
inline bool deserialize(const detail::string_view& data, T& t) {
  serialize_iarchive ar(data);
  ar >> t;
  return !ar.error_;
}

}  // namespace RPC_CORE_NAMESPACE
