#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"

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
    b.deSerialize(a.serialize());
    ASSERT(b == a);
  }
  {
    RpcCore_LOGI("Struct...");
    MyData data{1, 2};
    Struct<MyData> a;
    ASSERT(a.align_size == alignof(MyData));
    a.value = data;
    Struct<MyData> b;
    b.deSerialize(a.serialize());
    ASSERT(b.value.a == 1);
    ASSERT(b.value.b == 2);
    ASSERT(0 == memcmp(&a.value, &b.value, sizeof(MyData)));  // NOLINT
  }
  {
    RpcCore_LOGI("String/Binary...");
    uint8_t data[] = {0, 2, 4, 255, 0 /*important*/, 1, 3, 5};
    Binary a((char*)data, sizeof(data));
    Binary b;
    b.deSerialize(a.serialize());
    ASSERT(b.size() == sizeof(data));
    ASSERT(b == a);
  }
}

}  // namespace RpcCoreTest
