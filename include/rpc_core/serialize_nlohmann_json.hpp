#pragma once

#include "detail/log.h"
#include "detail/string_view.hpp"
#include "nlohmann/json.hpp"

namespace rpc_core {

template <typename T>
inline std::string serialize(T&& t) {
  return nlohmann::json(t).dump(-1);
}

template <typename T>
inline bool deserialize(const detail::string_view& data, T& t) {
  try {
    t = nlohmann::json::parse(data.data(), data.data() + data.size()).get<T>();
    return true;
  } catch (std::exception& e) {
    RPC_CORE_LOGE("deserialize: %s", e.what());
    return false;
  }
}

}  // namespace rpc_core
