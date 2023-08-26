#pragma once

#include "nlohmann/json.hpp"
#include "src/detail/log.h"
#include "src/serialize.hpp"

#define DEFINE_JSON_CLASS(CLASS)                                                               \
  namespace RPC_CORE_NAMESPACE {                                                               \
  template <typename T, typename std::enable_if<std::is_same<CLASS, T>::value, int>::type = 0> \
  serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {                         \
    oa.data.append(nlohmann::json(t).dump(-1));                                                \
    return oa;                                                                                 \
  }                                                                                            \
  template <typename T, typename std::enable_if<std::is_same<CLASS, T>::value, int>::type = 0> \
  serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {                               \
    try {                                                                                      \
      t = nlohmann::json::parse(ia.data, ia.data + ia.size).get<T>();                          \
    } catch (std::exception & e) {                                                             \
      RPC_CORE_LOGE("deserialize: %s", e.what());                                              \
      ia.error = true;                                                                         \
    }                                                                                          \
    return ia;                                                                                 \
  }                                                                                            \
  }  // namespace RPC_CORE_NAMESPACE
