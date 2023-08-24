#pragma once

#include "nlohmann/json.hpp"
#include "plugin/json_msg.hpp"

struct JsonType {
  int id = 0;
  std::string name;
  uint8_t age = 0;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(JsonType, id, name, age);
};

DEFINE_JSON_CLASS(JsonType);
