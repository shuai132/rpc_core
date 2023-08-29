// include first
#include "type/CustomType.h"

// include other
#include "assert_def.h"
#include "rpc_core.hpp"

#define SERIALIZE_AND_ASSERT(a, b) ASSERT(RPC_CORE_NAMESPACE::deserialize(RPC_CORE_NAMESPACE::serialize(a), b))

namespace rpc_core_test {

template <typename T>
void raw_type_test() {
  T a;
  memset(&a, 0xFF, sizeof(T));
  T b;
  SERIALIZE_AND_ASSERT(a, b);
  auto ok = (0 == memcmp(&a, &b, sizeof(T)));  // NOLINT
  if (ok) {
    RPC_CORE_LOGI("  => ok! ");
  } else {
    RPC_CORE_LOGI("  => type will lose precision... ");
  }
}

#define RAW_TYPE_TEST(t)       \
  RPC_CORE_LOGI("  <" #t ">"); \
  raw_type_test<t>();

void TypeTest() {
  /// raw type
  {
    RPC_CORE_LOGI("raw type test...");
    RAW_TYPE_TEST(char);
    RAW_TYPE_TEST(signed char);
    RAW_TYPE_TEST(unsigned char);
    RAW_TYPE_TEST(int);
    RAW_TYPE_TEST(unsigned int);
    RAW_TYPE_TEST(long int);
    RAW_TYPE_TEST(unsigned long int);
    RAW_TYPE_TEST(long long int);
    RAW_TYPE_TEST(unsigned long long int);
    RAW_TYPE_TEST(float);
    RAW_TYPE_TEST(double);
    RAW_TYPE_TEST(long double);
  }

  /// std::array
  {
    RPC_CORE_LOGI("std::array...");
    std::array<uint32_t, 3> a{1, 2, 3};
    std::array<uint32_t, 3> b{};
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::string
  {
    RPC_CORE_LOGI("std::string...");
    std::string a = "test";
    std::string b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }

  /// std::tuple
  {
    RPC_CORE_LOGI("std::tuple...");
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

  /// std::pair
  {
    RPC_CORE_LOGI("std::pair...");
    std::pair<std::string, std::string> a{"k:1", "v:1"};
    std::pair<std::string, std::string> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b == a);
  }

  /// list_like type
  {
    RPC_CORE_LOGI("std::vector...");
    std::vector<uint32_t> a{1, 2, 3};
    std::vector<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }
  {
    RPC_CORE_LOGI("std::list...");
    std::list<uint32_t> a{1, 2, 3};
    std::list<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }
  {
    RPC_CORE_LOGI("std::deque...");
    std::deque<uint32_t> a{1, 2, 3};
    std::deque<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::set
  {
    RPC_CORE_LOGI("std::set...");
    std::set<uint32_t> a{1, 2, 3};
    std::set<uint32_t> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::map
  {
    RPC_CORE_LOGI("std::map...");
    std::map<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::map<std::string, std::string> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::unordered_map
  {
    RPC_CORE_LOGI("std::unordered_map...");
    std::unordered_map<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::unordered_map<std::string, std::string> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::shared_ptr
  {
    RPC_CORE_LOGI("std::shared_ptr...");
    {
      std::shared_ptr<std::string> a = std::make_shared<std::string>("test");
      std::shared_ptr<std::string> b;
      SERIALIZE_AND_ASSERT(a, b);
      ASSERT(*a == *b);
    }
    {
      std::shared_ptr<std::string> a = nullptr;
      std::shared_ptr<std::string> b;
      SERIALIZE_AND_ASSERT(a, b);
      ASSERT(a == b);
    }
  }

  /// std::unique_ptr
  {
    RPC_CORE_LOGI("std::unique_ptr...");
    {
      std::unique_ptr<std::string> a = std::unique_ptr<std::string>(new std::string("test"));
      std::unique_ptr<std::string> b;
      SERIALIZE_AND_ASSERT(a, b);
      ASSERT(*a == *b);
    }
    {
      std::unique_ptr<std::string> a = nullptr;
      std::unique_ptr<std::string> b;
      SERIALIZE_AND_ASSERT(a, b);
      ASSERT(a == b);
    }
  }

  /// std::complex
  {
    RPC_CORE_LOGI("std::complex...");
    {
      std::complex<float> a;
      a.real(1.23f);
      a.imag(3.21f);
      std::complex<float> b;
      SERIALIZE_AND_ASSERT(a, b);
      ASSERT(a == b);
    }
    {
      std::complex<CustomType> a;
      {
        CustomType t;
        t.id = 1;
        t.ids = {1, 2, 3};
        t.name = "test";
        a.real(t);
        a.imag(t);
      }
      std::complex<CustomType> b;
      SERIALIZE_AND_ASSERT(a, b);
      ASSERT(a == b);
    }
  }

  /// std::duration
  {
    RPC_CORE_LOGI("std::duration...");
    std::chrono::seconds a(123);
    std::chrono::seconds b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// std::time_point
  {
    RPC_CORE_LOGI("std::time_point...");
    std::chrono::time_point<std::chrono::steady_clock> a = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  /// custom class/struct
  {
    RPC_CORE_LOGI("custom type...");
    CustomType a;
    a.id = 1;
    a.ids = {1, 2, 3};
    a.name = "test";
    CustomType b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a == b);
  }

  {
    RPC_CORE_LOGI("custom ptr...");
    CustomTypePtr a;
    a.int_n = nullptr;
    a.int_v = (int*)1;
    a.unique_ptr_n = nullptr;
    a.unique_ptr_v = std::unique_ptr<int>(new int(1));
    a.shared_ptr_n = nullptr;
    a.shared_ptr_v = std::make_shared<int>(1);
    CustomTypePtr b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(b.int_n == nullptr);
    ASSERT(b.int_v == (int*)1);
    ASSERT(b.unique_ptr_n == nullptr);
    ASSERT(*b.unique_ptr_v == 1);
    ASSERT(b.shared_ptr_n == nullptr);
    ASSERT(*b.shared_ptr_v == 1);
  }

  {
    RPC_CORE_LOGI("custom type(different alignas)...");
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
  {
    RPC_CORE_LOGI("custom type(nest define)...");
    test::CustomTypeNest a;
    a.c2.id1 = 1;
    a.c2.id2 = 2;
    a.c2.id3 = 3;
    test::CustomTypeNest b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a.c2.id1 == b.c2.id1);
    ASSERT(a.c2.id2 == b.c2.id2);
    ASSERT(a.c2.id3 == b.c2.id3);
  }
  {
    RPC_CORE_LOGI("custom type(inner)...");
    test::CustomTypeNest2 a;
    a.c2.id1 = 1;
    a.c2.id2 = 2;
    a.c2.id3 = 3;
    test::CustomTypeNest2 b;
    SERIALIZE_AND_ASSERT(a, b);
    ASSERT(a.c2.id1 == b.c2.id1);
    ASSERT(a.c2.id2 == b.c2.id2);
    ASSERT(a.c2.id3 == b.c2.id3);
  }

  /// misc types
  {
    RPC_CORE_LOGI("misc types...");
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

}  // namespace rpc_core_test
