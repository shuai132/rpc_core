#pragma once

#include "RpcCore.hpp"
#include "nlohmann/json.hpp"

namespace RpcCore {

struct Json : Message, public nlohmann::json {
  using nlohmann::json::basic_json;

  std::string serialize() const override {
    return dump();
  }

  bool deserialize(const detail::string_view& data) override {
    try {
      nlohmann::json::parse(data.data(), data.data() + data.size()).swap(*this);
    } catch (std::exception& e) {
      RpcCore_LOGE("deserialize: %s", e.what());
      return false;
    }
    return true;
  }
};

}  // namespace RpcCore
