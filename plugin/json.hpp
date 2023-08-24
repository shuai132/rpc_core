#pragma once

#include "nlohmann/json.hpp"
#include "src/detail/log.h"
#include "src/serialize.hpp"

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
std::string serialize(const T& t) {
  return t.dump();
}

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t) {
  try {
    nlohmann::json::parse(data.data(), data.data() + data.size()).swap(t);
  } catch (std::exception& e) {
    RPC_CORE_LOGE("deserialize: %s", e.what());
    return false;
  }
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
