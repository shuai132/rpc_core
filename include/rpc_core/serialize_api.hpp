#pragma once

// config
#include "config.hpp"

// include
#include "serialize_type.hpp"

namespace rpc_core {

template <typename T>
inline std::string serialize(T&& t) {
  serialize_oarchive ar;
  std::forward<T>(t) >> ar;
  return std::move(ar.data);
}

template <typename T>
inline bool deserialize(const detail::string_view& data, T& t) {
  serialize_iarchive ar(data);
  t << ar;
  return !ar.error;
}

inline bool deserialize(std::string&& data, std::string& t) {
  std::swap(data, t);
  return true;
}

}  // namespace rpc_core
