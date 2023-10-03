#pragma once

#include "nlohmann/json.hpp"
#include "rpc_core/detail/log.h"
#include "rpc_core/serialize.hpp"

#define RPC_CORE_DEFINE_TYPE_NLOHMANN_JSON(CLASS)                                              \
  template <typename T, typename std::enable_if<std::is_same<CLASS, T>::value, int>::type = 0> \
  ::rpc_core::serialize_oarchive& operator>>(const T& t, ::rpc_core::serialize_oarchive& oa) { \
    oa.data.append(nlohmann::json(t).dump(-1));                                                \
    return oa;                                                                                 \
  }                                                                                            \
  template <typename T, typename std::enable_if<std::is_same<CLASS, T>::value, int>::type = 0> \
  ::rpc_core::serialize_iarchive& operator<<(T& t, ::rpc_core::serialize_iarchive& ia) {       \
    try {                                                                                      \
      t = nlohmann::json::parse(ia.data, ia.data + ia.size).get<T>();                          \
    } catch (std::exception & e) {                                                             \
      RPC_CORE_LOGE("deserialize: %s", e.what());                                              \
      ia.error = true;                                                                         \
    }                                                                                          \
    return ia;                                                                                 \
  }

#define RPC_CORE_DEFINE_TYPE_JSON(Type, ...)            \
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, __VA_ARGS__) \
  RPC_CORE_DEFINE_TYPE_NLOHMANN_JSON(Type);
