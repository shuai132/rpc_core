#pragma once

#include "src/Serialize.hpp"

struct RawType {
  int id = 0;
  std::string name;
  uint8_t age = 0;
};
RpcCore_DEFINE_TYPE(RawType, id, name, age);
