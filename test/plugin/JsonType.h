#pragma once

#include "nlohmann/json.hpp"
#include "rpc_core/plugin/json_msg.hpp"

struct JsonType {
  int id = 0;
  std::string name;
  uint8_t age = 0;
};
RPC_CORE_DEFINE_TYPE_JSON(JsonType, id, name, age);
