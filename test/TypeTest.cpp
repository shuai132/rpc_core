#include <array>

#include "CustomType.h"
#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"

#define SERIALIZE_AND_ASSERT(a, b) ASSERT(RpcCore::deserialize(RpcCore::serialize(a), b))

namespace RpcCoreTest {

void TypeTest() {
  /// trivial type
  {
    RpcCore_LOGI("uint64_t...");
    uint64_t a = 0x12345678abcd;
    uint64_t b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }
  {
    RpcCore_LOGI("std::array...");
    std::array<uint32_t, 3> a{1, 2, 3};
    std::array<uint32_t, 3> b{};
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::string
  {
    RpcCore_LOGI("std::string...");
    std::string a = "test";
    std::string b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }

  /// std::tuple
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

  /// list_like type
  {
    RpcCore_LOGI("std::vector...");
    std::vector<uint32_t> a{1, 2, 3};
    std::vector<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }
  {
    RpcCore_LOGI("std::list...");
    std::list<uint32_t> a{1, 2, 3};
    std::list<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }
  {
    RpcCore_LOGI("std::deque...");
    std::deque<uint32_t> a{1, 2, 3};
    std::deque<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// custom class/struct
  {
    RpcCore_LOGI("custom type...");
    CustomType a;
    a.id = 1;
    a.ids = {1, 2, 3};
    a.name = "test";
    CustomType b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }
  {
    RpcCore_LOGI("custom type...");
    CustomType2 a;
    a.id1 = 1;
    a.id2 = 2;
    a.id3 = 3;
    CustomType3 b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a.id1 == b.id1);
    ASSERT(a.id2 == b.id2);
    ASSERT(a.id3 == b.id3);
  }

  /// misc types
  {
    RpcCore_LOGI("misc types...");
    CustomType customType;
    customType.id = 1;
    customType.ids = {1, 2, 3};
    customType.name = "test";
    std::tuple<bool, std::vector<std::tuple<uint32_t>>, std::string, CustomType> a{true, {{1}, {2}}, "test", customType};
    std::tuple<bool, std::vector<std::tuple<uint32_t>>, std::string, CustomType> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }
}

}  // namespace RpcCoreTest
