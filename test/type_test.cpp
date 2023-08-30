#include <codecvt>

// include first
#include "type/CustomType.h"

// include other
#include "assert_def.h"
#include "rpc_core.hpp"

namespace rpc_core_test {

static size_t last_serialize_size = 0;

template <typename T, typename R>
void serialize_test(const T& a, R& b) {
  std::string data = RPC_CORE_NAMESPACE::serialize(a);
  last_serialize_size = data.size();
  RPC_CORE_LOGI("  size: %zu", last_serialize_size);
  bool ret = RPC_CORE_NAMESPACE::deserialize(data, b);
  ASSERT(ret);
}

template <typename T>
void raw_type_test() {
  T a;
  memset(&a, 0xFF, sizeof(T));
  T b;
  serialize_test(a, b);
  auto ok = (0 == memcmp(&a, &b, sizeof(T)));  // NOLINT
  ASSERT(ok);
}

#define RAW_TYPE_TEST(t)       \
  RPC_CORE_LOGI("  <" #t ">"); \
  raw_type_test<t>();

#define ASSERT_SERIALIZE_SIZE(x) ASSERT(last_serialize_size == x)

static bool is_little_endian() {
  int x = 1;
  return *(char*)&x != 0;
}

void TypeTest() {
  ASSERT(is_little_endian());

  /// raw type
  {
    RPC_CORE_LOGI("raw type test...");
    RAW_TYPE_TEST(char);
    ASSERT_SERIALIZE_SIZE(1);
    RAW_TYPE_TEST(int8_t);
    ASSERT_SERIALIZE_SIZE(1);
    RAW_TYPE_TEST(uint8_t);
    ASSERT_SERIALIZE_SIZE(1);
    RAW_TYPE_TEST(int16_t);
    ASSERT_SERIALIZE_SIZE(2);
    RAW_TYPE_TEST(uint16_t);
    ASSERT_SERIALIZE_SIZE(2);
    RAW_TYPE_TEST(int32_t);
    ASSERT_SERIALIZE_SIZE(4);
    RAW_TYPE_TEST(uint32_t);
    ASSERT_SERIALIZE_SIZE(4);
    RAW_TYPE_TEST(int64_t);
    ASSERT_SERIALIZE_SIZE(8);
    RAW_TYPE_TEST(uint64_t);
    ASSERT_SERIALIZE_SIZE(8);
    RAW_TYPE_TEST(float);
    ASSERT_SERIALIZE_SIZE(4);
    RAW_TYPE_TEST(double);
    ASSERT_SERIALIZE_SIZE(8);
    RAW_TYPE_TEST(long double);
    ASSERT_SERIALIZE_SIZE(16);
  }

  /// std::array
  {
    RPC_CORE_LOGI("std::array...");
    std::array<uint32_t, 3> a{1, 2, 3};
    std::array<uint32_t, 3> b{};
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(16);
  }

  /// std::string
  {
    RPC_CORE_LOGI("std::string...");
    std::string a = "test";
    std::string b;
    serialize_test(a, b);
    ASSERT(b == a);
    ASSERT_SERIALIZE_SIZE(a.size());
  }

  /// std::wstring
  {
    RPC_CORE_LOGI("std::wstring...");
    std::wstring a = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes("你好，世界！");
    std::wstring b;
    serialize_test(a, b);
    ASSERT(b == a);
    ASSERT_SERIALIZE_SIZE(a.size() * sizeof(wchar_t));
  }

  /// std::tuple
  {
    RPC_CORE_LOGI("std::tuple...");
    bool msg1 = true;
    uint32_t msg2 = 12;
    std::string msg3 = "test";
    std::tuple<bool, uint32_t, std::string> a(msg1, msg2, msg3);
    std::tuple<bool, uint32_t, std::string> b;
    serialize_test(a, b);
    ASSERT(std::get<0>(b) == msg1);
    ASSERT(std::get<1>(b) == msg2);
    ASSERT(std::get<2>(b) == msg3);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE((1) + (4) + (4 + msg3.size()));
  }

  /// std::pair
  {
    RPC_CORE_LOGI("std::pair...");
    std::pair<std::string, std::string> a{"k:1", "v:1"};
    std::pair<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(b == a);
    ASSERT_SERIALIZE_SIZE((4 + 3) * 2);
  }

  /// list_like type
  {
    RPC_CORE_LOGI("std::vector...");
    std::vector<uint32_t> a{1, 2, 3};
    std::vector<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }
  {
    RPC_CORE_LOGI("std::list...");
    std::list<uint32_t> a{1, 2, 3};
    std::list<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
  }
  {
    RPC_CORE_LOGI("std::deque...");
    std::deque<uint32_t> a{1, 2, 3};
    std::deque<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }

  /// std container adaptors
  {
    RPC_CORE_LOGI("std::stack...");
    std::stack<uint32_t> a;
    a.push(1);
    a.push(2);
    a.push(3);
    std::stack<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }
  {
    RPC_CORE_LOGI("std::queue...");
    std::queue<uint32_t> a;
    a.push(1);
    a.push(2);
    a.push(3);
    std::queue<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }
  {
    RPC_CORE_LOGI("std::priority_queue...");
    std::priority_queue<uint32_t> a;
    a.push(1);
    a.push(2);
    a.push(3);
    std::priority_queue<uint32_t> b;
    serialize_test(a, b);
    for (uint32_t i = 0; i < a.size(); ++i) {
      ASSERT(a.top() == b.top());
      a.pop();
      b.pop();
    }
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }

  /// std::bitset
  {
    RPC_CORE_LOGI("std::bitset...");
    std::bitset<8> a;
    a.set();
    std::bitset<8> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(8);
  }

  /// std::forward_list
  {
    RPC_CORE_LOGI("std::forward_list...");
    std::forward_list<uint32_t> a{1, 2, 3};
    std::forward_list<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }

  /// std::set
  {
    RPC_CORE_LOGI("std::set...");
    std::set<uint32_t> a{1, 2, 3};
    std::set<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }

  /// std::multiset
  {
    RPC_CORE_LOGI("std::multiset...");
    std::multiset<uint32_t> a{1, 2, 3};
    std::multiset<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }

  /// std::unordered_multiset
  {
    RPC_CORE_LOGI("std::unordered_multiset...");
    std::unordered_multiset<uint32_t> a{1, 2, 3};
    std::unordered_multiset<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + 4 * 3);
  }

  /// std::map
  {
    RPC_CORE_LOGI("std::map...");
    std::map<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::map<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + (4 /*std::pair*/ + (4 + 3) * 2) * 3);
  }

  /// std::unordered_map
  {
    RPC_CORE_LOGI("std::unordered_map...");
    std::unordered_map<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::unordered_map<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + (4 /*std::pair*/ + (4 + 3) * 2) * 3);
  }

  /// std::multimap
  {
    RPC_CORE_LOGI("std::multimap...");
    std::multimap<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::multimap<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + (4 /*std::pair*/ + (4 + 3) * 2) * 3);
  }

  /// std::unordered_multimap
  {
    RPC_CORE_LOGI("std::unordered_multimap...");
    std::unordered_multimap<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::unordered_multimap<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*size*/ + (4 /*std::pair*/ + (4 + 3) * 2) * 3);
  }

  /// std::shared_ptr
  {
    RPC_CORE_LOGI("std::shared_ptr...");
    {
      std::shared_ptr<std::string> a = std::make_shared<std::string>("test");
      std::shared_ptr<std::string> b;
      serialize_test(a, b);
      ASSERT(*a == *b);
      ASSERT_SERIALIZE_SIZE(1 /*flag*/ + a->size());
    }
    {
      std::shared_ptr<std::string> a = nullptr;
      std::shared_ptr<std::string> b;
      serialize_test(a, b);
      ASSERT(a == b);
      ASSERT_SERIALIZE_SIZE(1 /*flag*/);
    }
  }

  /// std::unique_ptr
  {
    RPC_CORE_LOGI("std::unique_ptr...");
    {
      std::unique_ptr<std::string> a = std::unique_ptr<std::string>(new std::string("test"));
      std::unique_ptr<std::string> b;
      serialize_test(a, b);
      ASSERT(*a == *b);
      ASSERT_SERIALIZE_SIZE(1 /*flag*/ + a->size());
    }
    {
      std::unique_ptr<std::string> a = nullptr;
      std::unique_ptr<std::string> b;
      serialize_test(a, b);
      ASSERT(a == b);
      ASSERT_SERIALIZE_SIZE(1 /*flag*/);
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
      serialize_test(a, b);
      ASSERT(a == b);
      ASSERT_SERIALIZE_SIZE(4 /*float*/ * 2);
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
      serialize_test(a, b);
      ASSERT(a == b);
      ASSERT_SERIALIZE_SIZE(72);
    }
    static_assert(std::is_fundamental<uint32_t>::value, "");
    static_assert(sizeof(uint32_t) == 4, "");
  }

  /// std::duration
  {
    RPC_CORE_LOGI("std::duration...");
    std::chrono::seconds a(123);
    std::chrono::seconds b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(8);
  }

  /// std::time_point
  {
    RPC_CORE_LOGI("std::time_point...");
    std::chrono::time_point<std::chrono::steady_clock> a = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(8);
  }

  /// custom class/struct
  {
    RPC_CORE_LOGI("custom type...");
    CustomType a;
    a.id = 1;
    a.ids = {1, 2, 3};
    a.name = "test";
    CustomType b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(32);
  }

  {
    RPC_CORE_LOGI("custom ptr...");
    CustomTypePtr a;
    a.int_n = nullptr;
    a.int_v = (int32_t*)1;
    a.unique_ptr_n = nullptr;
    a.unique_ptr_v = std::unique_ptr<int32_t>(new int32_t(1));
    a.shared_ptr_n = nullptr;
    a.shared_ptr_v = std::make_shared<int32_t>(1);
    CustomTypePtr b;
    serialize_test(a, b);
    ASSERT(b.int_n == nullptr);
    ASSERT(b.int_v == (int32_t*)1);
    ASSERT(b.unique_ptr_n == nullptr);
    ASSERT(*b.unique_ptr_v == 1);
    ASSERT(b.shared_ptr_n == nullptr);
    ASSERT(*b.shared_ptr_v == 1);
    ASSERT_SERIALIZE_SIZE(52);
  }

  {
    RPC_CORE_LOGI("custom type(different alignas)...");
    CustomType2 a;
    a.id1 = 1;
    a.id2 = 2;
    a.id3 = 3;
    CustomType3 b;
    serialize_test(a, b);
    ASSERT(a.id1 == b.id1);
    ASSERT(a.id2 == b.id2);
    ASSERT(a.id3 == b.id3);
    ASSERT_SERIALIZE_SIZE(6);
  }
  {
    RPC_CORE_LOGI("custom type(nest define)...");
    test::CustomTypeNest a;
    a.c2.id1 = 1;
    a.c2.id2 = 2;
    a.c2.id3 = 3;
    test::CustomTypeNest b;
    serialize_test(a, b);
    ASSERT(a.c2.id1 == b.c2.id1);
    ASSERT(a.c2.id2 == b.c2.id2);
    ASSERT(a.c2.id3 == b.c2.id3);
    ASSERT_SERIALIZE_SIZE(32);
  }
  {
    RPC_CORE_LOGI("custom type(inner)...");
    test::CustomTypeNest2 a;
    a.c2.id1 = 1;
    a.c2.id2 = 2;
    a.c2.id3 = 3;
    test::CustomTypeNest2 b;
    serialize_test(a, b);
    ASSERT(a.c2.id1 == b.c2.id1);
    ASSERT(a.c2.id2 == b.c2.id2);
    ASSERT(a.c2.id3 == b.c2.id3);
    ASSERT_SERIALIZE_SIZE(32);
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
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(69);
  }
}

}  // namespace rpc_core_test
