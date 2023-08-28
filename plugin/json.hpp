#pragma once

#include "nlohmann/json.hpp"
#include "src/detail/log.h"
#include "src/serialize.hpp"

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  oa << t.dump();
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  try {
    nlohmann::json::parse(ia.data, ia.data + ia.size).swap(t);
  } catch (std::exception& e) {
    RPC_CORE_LOGE("deserialize: %s", e.what());
    ia.error = true;
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
