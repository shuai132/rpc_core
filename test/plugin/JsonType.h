#pragma once

#include "nlohmann/json.hpp"
#include "plugin/JsonMsg.hpp"

struct JsonType {
  int id = 0;
  std::string name;
  uint8_t age = 0;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(JsonType, id, name, age);
};

DEFINE_JSON_CLASS(JsonType);

// 用于对比序列化体积
struct JsonType2 {
  int id = 0;
  std::string name;
  uint8_t age = 0;
};
RpcCore_DEFINE_TYPE(JsonType2, id, name, age);
