#pragma once

#include "nlohmann/json.hpp"
#include "src/Serialize.hpp"

namespace RpcCore {

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  return t.dump();
}

template <typename T, typename std::enable_if<std::is_same<nlohmann::json, T>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  try {
    nlohmann::json::parse(data.data(), data.data() + data.size()).swap(t);
  } catch (std::exception& e) {
    RpcCore_LOGE("deserialize: %s", e.what());
    return false;
  }
  return true;
}

}  // namespace RpcCore
