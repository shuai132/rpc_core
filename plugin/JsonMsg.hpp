#pragma once

#include "RpcCore.hpp"
#include "nlohmann/json.hpp"

namespace RpcCore {

template <typename T, int indent = -1>
struct JsonMsg : RpcCore::Message {
  JsonMsg() = default;

  JsonMsg(T msg) {  // NOLINT
    this->msg = std::move(msg);
  }

  std::string serialize() const override {
    return nlohmann::json(msg).dump(indent);
  }

  bool deSerialize(const std::string& data) override {
    try {
      msg = nlohmann::json::parse(data).get<T>();
    } catch (std::exception& e) {
      RpcCore_LOGE("deSerialize: %s", e.what());
      return false;
    }
    return true;
  }

  T* operator->() {
    return &msg;
  }

  T msg;
};

}  // namespace RpcCore
