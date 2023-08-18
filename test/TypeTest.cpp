#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"

#define SERIALIZE_AND_ASSERT(a, b) ASSERT(deserialize(serialize(a), b))

namespace RpcCoreTest {

struct MyData {
  uint8_t a;
  uint16_t b;
};

void TypeTest() {
  using namespace RpcCore;
  {
    RpcCore_LOGI("uint64_t...");
    uint64_t a = 0x12345678abcd;
    uint64_t b;
    serialize(a);
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }
  {
    RpcCore_LOGI("struct...");
    MyData a{1, 2};
    MyData b{};
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b.a == 1);
    ASSERT(b.b == 2);
  }
  {
    RpcCore_LOGI("std::string...");
    std::string a = "test";
    std::string b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }
  {
    RpcCore_LOGI("std::tuple...");
    bool msg1 = true;
    uint32_t msg2 = 12;
    std::string msg3 = "test";
    std::tuple<bool, uint32_t, std::string> a(msg1, msg2, msg3);
    std::tuple<bool, uint32_t, std::string> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(std::get<0>(b) == msg1);
    ASSERT(std::get<1>(b) == msg2);
    ASSERT(std::get<2>(b) == msg3);
    ASSERT(a == b);
  }
}

}  // namespace RpcCoreTest
