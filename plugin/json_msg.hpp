#pragma once

#include "nlohmann/json.hpp"
#include "src/detail/log.h"
#include "src/serialize.hpp"

#define DEFINE_JSON_CLASS(CLASS)                                                               \
  namespace RPC_CORE_NAMESPACE {                                                               \
  template <typename T, typename std::enable_if<std::is_same<CLASS, T>::value, int>::type = 0> \
  std::string serialize(const T& t) {                                                          \
    return nlohmann::json(t).dump(-1);                                                         \
  }                                                                                            \
  template <typename T, typename std::enable_if<std::is_same<CLASS, T>::value, int>::type = 0> \
  bool deserialize(const detail::string_view& data, T& t) {                                    \
    try {                                                                                      \
      t = nlohmann::json::parse(data.data(), data.data() + data.size()).get<T>();              \
    } catch (std::exception & e) {                                                             \
      RPC_CORE_LOGE("deserialize: %s", e.what());                                              \
      return false;                                                                            \
    }                                                                                          \
    return true;                                                                               \
  }                                                                                            \
  }  // namespace RPC_CORE_NAMESPACE
