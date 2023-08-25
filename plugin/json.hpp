#pragma once

#include "nlohmann/json.hpp"
#include "src/detail/log.h"
#include "src/serialize.hpp"

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {
  oa << t.dump();
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {
  try {
    nlohmann::json::parse(ia.data_, ia.data_ + ia.size_).swap(t);
  } catch (std::exception& e) {
    RPC_CORE_LOGE("deserialize: %s", e.what());
    ia.error_ = true;
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
