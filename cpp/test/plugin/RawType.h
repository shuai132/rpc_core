#pragma once

#include "rpc_core/serialize.hpp"

struct RawType {
  uint8_t id = 0;
  std::string name;
  uint8_t age = 0;
};
RPC_CORE_DEFINE_TYPE(RawType, id, name, age);
