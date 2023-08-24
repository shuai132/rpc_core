#pragma once

#include "src/serialize.hpp"

struct RawType {
  int id = 0;
  std::string name;
  uint8_t age = 0;
};
RPC_CORE_DEFINE_TYPE(RawType, id, name, age);
