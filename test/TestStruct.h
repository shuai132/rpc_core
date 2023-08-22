#pragma once

#include "src/Serialize.hpp"

struct TestStruct {
  uint8_t a;
  uint16_t b;
  uint32_t c;
};
RpcCore_DEFINE_TYPE(TestStruct, a, b, c);
