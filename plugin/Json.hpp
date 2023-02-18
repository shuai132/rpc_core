#pragma once

#include "RpcCore.hpp"
#include "nlohmann/json.hpp"

namespace RpcCore {

struct Json : Message, public nlohmann::json {
  using nlohmann::json::basic_json;

  std::string serialize() const override {
    return dump();
  }

  bool deSerialize(const std::string& data) override {
    try {
      nlohmann::json::parse(data).swap(*this);
    } catch (std::exception& e) {
      RpcCore_LOGE("deSerialize: %s", e.what());
      return false;
    }
    return true;
  }
};

}  // namespace RpcCore
