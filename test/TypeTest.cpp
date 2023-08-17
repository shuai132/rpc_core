#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"

#define SERIALIZE_AND_ASSERT(a, b) ASSERT(b.deserialize(a.serialize()))

namespace RpcCoreTest {

struct MyData {
  uint8_t a;
  uint16_t b;
};

void TypeTest() {
  using namespace RpcCore;
  {
    RpcCore_LOGI("Raw<uint64_t>...");
    const uint64_t value = 0x12345678abcd;
    Raw<uint64_t> a(value);
    ASSERT(a == a.value);
    Raw<uint64_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }
  {
    RpcCore_LOGI("Struct...");
    MyData data{1, 2};
    Struct<MyData> a;
    ASSERT(a.align_size == alignof(MyData));
    a.value = data;
    Struct<MyData> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b.value.a == 1);
    ASSERT(b.value.b == 2);
    ASSERT(0 == memcmp(&a.value, &b.value, sizeof(MyData)));  // NOLINT
  }
  {
    RpcCore_LOGI("String/Binary...");
    uint8_t data[] = {0, 2, 4, 255, 0 /*important*/, 1, 3, 5};
    Binary a((char*)data, sizeof(data));
    Binary b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b.size() == sizeof(data));
    ASSERT(b == a);
  }
  {
    RpcCore_LOGI("Tuple...");
    Bool msg1 = true;
    Raw<uint32_t> msg2 = 12;
    String msg3 = "test";
    Tuple<Bool, Raw<uint32_t>, String> a(msg1, msg2, msg3);
    Tuple<Bool, Raw<uint32_t>, String> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(std::get<0>(b) == msg1);
    ASSERT(std::get<1>(b) == msg2);
    ASSERT(std::get<2>(b) == msg3);
    ASSERT(a == b);
  }
}

}  // namespace RpcCoreTest
