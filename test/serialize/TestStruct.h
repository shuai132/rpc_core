#pragma once

#include "rpc_core/serialize.hpp"

struct TestStruct {
  uint8_t a;
  uint16_t b;
  uint32_t c;
};
RPC_CORE_DEFINE_TYPE(TestStruct, a, b, c);
