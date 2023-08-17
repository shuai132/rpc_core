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

  bool deserialize(const detail::string_view& data) override {
    try {
      msg = nlohmann::json::parse(data.data(), data.data() + data.size()).get<T>();
    } catch (std::exception& e) {
      RpcCore_LOGE("deserialize: %s", e.what());
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
