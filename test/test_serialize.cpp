#include <cinttypes>
#include <cmath>
#include <limits>

#include "assert_def.h"
#include "rpc_core.hpp"
#include "serialize/CustomType.h"

namespace rpc_core_test {

static size_t last_serialize_size = 0;

template <typename T, typename R>
void serialize_test(T&& a, R& b) {
  std::string data = rpc_core::serialize(std::forward<T>(a));
  last_serialize_size = data.size();
  RPC_CORE_LOGI("  size: %zu", last_serialize_size);
  bool ret = rpc_core::deserialize(std::move(data), b);
  ASSERT(ret);
}

template <typename T>
void raw_type_test() {
  T a = 123;
  T b;
  serialize_test(a, b);
  ASSERT(a == b);
}

// Only instantiate serialization tests for supported native floating formats.
template <typename T>
void test_floating_type(std::false_type) {}

template <typename T>
void test_floating_type(std::true_type) {
  using limits = std::numeric_limits<T>;
  const size_t wire_size = std::is_same<T, float>::value ? 4 : 8;
  const std::string one = wire_size == 4 ? std::string("\x00\x00\x80\x3f", 4)
      : std::string("\x00\x00\x00\x00\x00\x00\xf0\x3f", 8);
  ASSERT(rpc_core::serialize(T(1)) == one);
  for (auto original : {T(0), -T(0), T(1) + limits::epsilon(), limits::min(),
                        limits::max(), limits::denorm_min(), limits::infinity(), -limits::infinity()}) {
    auto data = rpc_core::serialize(original);
    ASSERT(data.size() == wire_size);
    T decoded = 123;
    ASSERT(rpc_core::deserialize(data, decoded));
    ASSERT(decoded == original && std::signbit(decoded) == std::signbit(original));
  }
  T decoded = 123;
  ASSERT(rpc_core::deserialize(rpc_core::serialize(limits::quiet_NaN()), decoded));
  ASSERT(std::isnan(decoded));
  for (size_t size = 0; size < wire_size; ++size) {
    ASSERT(!rpc_core::deserialize(one.substr(0, size), decoded));
    ASSERT(decoded == 0);
  }
  auto nested = std::make_tuple(T(1.25), 42, T(-2.5));
  decltype(nested) restored;
  ASSERT(rpc_core::deserialize(rpc_core::serialize(nested), restored));
  ASSERT(nested == restored);
}

#define RAW_TYPE_TEST(t)       \
  RPC_CORE_LOGI("  <" #t ">"); \
  raw_type_test<t>();

#define ASSERT_SERIALIZE_SIZE(x) ASSERT(last_serialize_size == x)

static bool is_little_endian() {
  int x = 1;
  return *(char*)&x != 0;
}

static void test_auto_size() {
  using namespace rpc_core::detail;
  auto test_auto_size = [](size_t value, int except_size) {
    RPC_CORE_LOGI("value: 0x%" PRIxMAX "(%" PRIiMAX ") except: %d", value, value, except_size);
    auto_size a(value);
    std::string payload = a.serialize();
    ASSERT(payload.size() == (size_t)except_size);
    auto_size b;
    int cost = b.deserialize(payload.data(), payload.size());
    ASSERT(cost == except_size);
    ASSERT(value == b.value);
  };
  test_auto_size(0x00, 1);
  test_auto_size(0x01, 2);
  test_auto_size(0xff, 2);
  test_auto_size(0xfff, 3);
  test_auto_size(0xffff, 3);
  test_auto_size(0xfffff, 4);
  test_auto_size(0xffffff, 4);
  test_auto_size(0xfffffff, 5);
  test_auto_size(0xffffffff, 5);

  auto test_auto_int = [](intmax_t value, int except_size) {
    RPC_CORE_LOGI("value: 0x%" PRIxMAX "(%" PRIiMAX ") except: %d", value, value, except_size);
    auto_intmax a(value);
    std::string payload = a.serialize();
    ASSERT(payload.size() == (size_t)except_size);
    auto_intmax b;
    int cost = b.deserialize(payload.data(), payload.size());
    ASSERT(cost == except_size);
    ASSERT(value == b.value);
  };
  test_auto_int(0x00, 1);
  test_auto_int(0xff, 2);
  test_auto_int(-0xff, 2);
  test_auto_int(0xffff, 3);
  test_auto_int(-0xffff, 3);
  test_auto_int(0xffffff, 4);
  test_auto_int(-0xffffff, 4);
  test_auto_int(intmax_t(0xffffffff), 5);
  test_auto_int(-(intmax_t(0xffffffff)), 5);

  auto test_auto_uint = [](uintmax_t value, int except_size) {
    RPC_CORE_LOGI("value: 0x%" PRIxMAX "(%" PRIiMAX ") except: %d", value, value, except_size);
    auto_uintmax a(value);
    std::string payload = a.serialize();
    ASSERT(payload.size() == (size_t)except_size);
    auto_uintmax b;
    int cost = b.deserialize(payload.data(), payload.size());
    ASSERT(cost == except_size);
    ASSERT(value == b.value);
  };
  test_auto_uint(0x00, 1);
  test_auto_uint(0xff, 2);
  test_auto_uint(0xffff, 3);
  test_auto_uint(0xffffff, 4);
  test_auto_uint(uintmax_t(0xffffffff), 5);
}

void test_serialize() {
  {
    std::tuple<> empty;
    ASSERT(rpc_core::serialize(empty).empty());
    ASSERT(rpc_core::deserialize(std::string{}, empty));
    std::vector<bool> bits{true, false, true}, decoded;
    ASSERT(rpc_core::deserialize(rpc_core::serialize(bits), decoded));
    ASSERT(decoded == bits);
    std::bitset<3> flags;
    for (const auto& invalid : {"", "01", "012", "0101"}) {
      ASSERT(!rpc_core::deserialize(std::string(invalid), flags));
    }
    ASSERT(rpc_core::deserialize(std::string("101"), flags));
    ASSERT(flags.to_ulong() == 5);
  }
  {
    int value = 0;
    auto too_large = static_cast<intmax_t>((std::numeric_limits<int>::max)()) + 1;
    auto too_small = static_cast<intmax_t>((std::numeric_limits<int>::min)()) - 1;
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(too_large), value));
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(too_small), value));
    unsigned int unsigned_value = 0;
    auto unsigned_large = static_cast<uintmax_t>((std::numeric_limits<unsigned int>::max)()) + 1;
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(unsigned_large), unsigned_value));
  }
  {
    std::wstring wide;
    ASSERT(rpc_core::deserialize(rpc_core::serialize(std::wstring(L"wide")), wide));
    ASSERT(wide == L"wide");
    std::u16string utf16;
    ASSERT(rpc_core::deserialize(rpc_core::serialize(std::u16string(u"utf16")), utf16));
    ASSERT(utf16 == u"utf16");
    ASSERT(!rpc_core::deserialize(std::string(1, 'x'), utf16));
  }
  /// only support little endian
  ASSERT(is_little_endian());

  /// auto_size
  test_auto_size();

  /// raw type
  {
    int32_t a = -1;
    int32_t b = 0;

    std::string data = rpc_core::serialize(a);
    bool ret = rpc_core::deserialize(data, b);
    ASSERT(ret);

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
    ASSERT_SERIALIZE_SIZE(2);
    RAW_TYPE_TEST(uint32_t);
    ASSERT_SERIALIZE_SIZE(2);
    RAW_TYPE_TEST(int64_t);
    ASSERT_SERIALIZE_SIZE(2);
    RAW_TYPE_TEST(uint64_t);
    ASSERT_SERIALIZE_SIZE(2);
    RAW_TYPE_TEST(float);
    ASSERT_SERIALIZE_SIZE(4);
    RAW_TYPE_TEST(double);
    ASSERT_SERIALIZE_SIZE(8);
    test_floating_type<float>(std::true_type{});
    test_floating_type<double>(std::true_type{});
    test_floating_type<long double>(std::integral_constant<bool,
        rpc_core::detail::binary_float_format<long double>::supported>{});
  }

  /// enum
  {
    enum class Enum {
      k_0,
      k_1,
    };
    RPC_CORE_LOGI("enum...");
    Enum a = Enum::k_1;
    Enum b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2);
  }

  /// std::array
  {
    RPC_CORE_LOGI("std::array...");
    std::array<uint32_t, 3> a{1, 2, 3};
    std::array<uint32_t, 3> b{};
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }

  /// std::string
  {
    RPC_CORE_LOGI("std::string...");
    const std::string test = "test";
    std::string a = test;
    std::string b;
    serialize_test(a, b);
    ASSERT(b == a);
    ASSERT(b == test);
    ASSERT_SERIALIZE_SIZE(a.size());
  }

  /// std::string zero overhead
  {
    RPC_CORE_LOGI("std::string is zero overhead...");
    const std::string test = "test";
    std::string a = test;
    std::string b;
    auto r = rpc_core::serialize(std::move(a));
    rpc_core::deserialize(std::move(r), b);
    ASSERT(a.empty());
    ASSERT(r.empty());
    ASSERT(b == test);
  }

  /// std::wstring
  {
    RPC_CORE_LOGI("std::wstring...");
    std::wstring a = L"中文";
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
    ASSERT_SERIALIZE_SIZE((1) + (2) + (2 /*size*/ + msg3.size()));
  }

  /// std::pair
  {
    RPC_CORE_LOGI("std::pair...");
    std::pair<std::string, std::string> a{"k:1", "v:1"};
    std::pair<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(b == a);
    ASSERT_SERIALIZE_SIZE((2 /*size*/ + 3) * 2);
  }

  /// list_like type
  {
    RPC_CORE_LOGI("std::vector...");
    std::vector<uint32_t> a{1, 2, 3};
    std::vector<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }
  {
    RPC_CORE_LOGI("std::list...");
    std::list<uint32_t> a{1, 2, 3};
    std::list<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }
  {
    RPC_CORE_LOGI("std::deque...");
    std::deque<uint32_t> a{1, 2, 3};
    std::deque<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
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
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
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
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
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
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }

  /// std::bitset
  {
    RPC_CORE_LOGI("std::bitset...");
    std::bitset<8> a;
    a.set();
    std::bitset<8> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(8 /*11111111*/);
  }

  /// std::forward_list
  {
    RPC_CORE_LOGI("std::forward_list...");
    std::forward_list<uint32_t> a{1, 2, 3};
    std::forward_list<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }

  /// std::set
  {
    RPC_CORE_LOGI("std::set...");
    std::set<uint32_t> a{1, 2, 3};
    std::set<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }

  /// std::multiset
  {
    RPC_CORE_LOGI("std::multiset...");
    std::multiset<uint32_t> a{1, 2, 3};
    std::multiset<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }

  /// std::unordered_multiset
  {
    RPC_CORE_LOGI("std::unordered_multiset...");
    std::unordered_multiset<uint32_t> a{1, 2, 3};
    std::unordered_multiset<uint32_t> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + 2 * 3);
  }

  /// std::map
  {
    RPC_CORE_LOGI("std::map...");
    std::map<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::map<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + (2 /*std::pair*/ + (2 + 3) * 2) * 3);
  }

  /// std::unordered_map
  {
    RPC_CORE_LOGI("std::unordered_map...");
    std::unordered_map<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::unordered_map<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + (2 /*std::pair*/ + (2 + 3) * 2) * 3);
  }

  /// std::multimap
  {
    RPC_CORE_LOGI("std::multimap...");
    std::multimap<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::multimap<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + (2 /*std::pair*/ + (2 + 3) * 2) * 3);
  }

  /// std::unordered_multimap
  {
    RPC_CORE_LOGI("std::unordered_multimap...");
    std::unordered_multimap<std::string, std::string> a{{"k:1", "v:1"}, {"k:2", "v:2"}, {"k:3", "v:3"}};
    std::unordered_multimap<std::string, std::string> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2 /*size*/ + (2 /*std::pair*/ + (2 + 3) * 2) * 3);
  }

  /// ptr
  {
    RPC_CORE_LOGI("ptr...");
    int* a = (int*)123;
    int* b = nullptr;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2);  // auto size
  }

  /// binary_wrap
  {
    RPC_CORE_LOGI("binary_wrap...");
    {
      uint8_t array[] = {1, 2, 3};
      size_t data_bytes = sizeof(array);
      rpc_core::binary_wrap a(array, data_bytes);
      rpc_core::binary_wrap b;
      serialize_test(a, b);
      ASSERT(b.size == data_bytes);
      ASSERT(b.data != array);
      ASSERT(0 == memcmp(array, b.data, data_bytes));
      ASSERT_SERIALIZE_SIZE(2 /*size*/ + data_bytes);
    }
    {
      uint8_t array[] = {1, 2, 3};
      struct Test {
        uint8_t* ptr = nullptr;
        rpc_core::binary_wrap bin;
        RPC_CORE_DEFINE_TYPE_INNER(ptr, bin);
      };
      Test a;
      a.ptr = array;
      a.bin = {array, sizeof(array)};
      Test b;
      serialize_test(a, b);
      ASSERT(b.ptr == array);
      ASSERT(b.bin.size == sizeof(array));
      ASSERT(b.bin.data != array);
      ASSERT(0 == memcmp(array, b.bin.data, sizeof(array)));
    }
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
    std::complex<float> a;
    a.real(1.23f);
    a.imag(3.21f);
    std::complex<float> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(4 /*float*/ * 2);
  }

  /// std::duration
  {
    RPC_CORE_LOGI("std::duration...");
    std::chrono::seconds a(123);
    std::chrono::seconds b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(2);  // auto size
  }

  /// std::time_point
  {
    RPC_CORE_LOGI("std::time_point...");
    std::chrono::time_point<std::chrono::steady_clock> a = std::chrono::time_point<std::chrono::steady_clock>::max();
    std::chrono::time_point<std::chrono::steady_clock> b;
    serialize_test(a, b);
    ASSERT(a == b);
    ASSERT_SERIALIZE_SIZE(9);
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
    ASSERT_SERIALIZE_SIZE((2) + (2 /*ids bytes*/ + 2 /*ids size*/ + 2 * 3) + (2 /*name bytes*/ + 4));
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
    ASSERT_SERIALIZE_SIZE(23);
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
    ASSERT_SERIALIZE_SIZE(4);
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
    ASSERT_SERIALIZE_SIZE(14);
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
    ASSERT_SERIALIZE_SIZE(14);
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
    ASSERT_SERIALIZE_SIZE(39);
  }

  // Duration counts must fit their representation, including unsigned bounds.
  {
    using Wide = std::chrono::duration<int64_t>;
    using Narrow = std::chrono::duration<int32_t>;
    Narrow narrow(42);
    for (auto invalid : {int64_t(INT32_MAX) + 1, int64_t(INT32_MIN) - 1}) {
      ASSERT(!rpc_core::deserialize(rpc_core::serialize(Wide(invalid)), narrow));
      ASSERT(narrow.count() == 42);
    }
    for (auto valid : {INT32_MIN, 0, INT32_MAX}) {
      ASSERT(rpc_core::deserialize(rpc_core::serialize(Wide(valid)), narrow));
      ASSERT(narrow.count() == valid);
    }
    using Unsigned = std::chrono::duration<uint32_t>;
    Unsigned unsigned_count(42);
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(Wide(-1)), unsigned_count));
    ASSERT(unsigned_count.count() == 42);
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(Wide(int64_t(UINT32_MAX) + 1)), unsigned_count));
    ASSERT(unsigned_count.count() == 42);
    using UnsignedWide = std::chrono::duration<uint64_t>;
    for (auto count : {uint64_t(0), uint64_t(INT64_MAX), uint64_t(INT64_MAX) + 1, uint64_t(UINT64_MAX)}) {
      UnsignedWide restored;
      auto data = rpc_core::serialize(UnsignedWide(count));
      ASSERT(rpc_core::deserialize(data, restored));
      ASSERT(restored.count() == count);
      if (count > uint64_t(INT64_MAX)) {
        Wide signed_count;
        ASSERT(!rpc_core::deserialize(data, signed_count));
      }
    }
    using Point = std::chrono::time_point<std::chrono::system_clock, Narrow>;
    Point point(Narrow(42));
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(Wide(int64_t(INT32_MAX) + 1)), point));
    ASSERT(point.time_since_epoch().count() == 42);
    std::chrono::duration<double> floating;
    ASSERT(rpc_core::deserialize(rpc_core::serialize(std::chrono::duration<double>(0.25)), floating));
    ASSERT(floating.count() == 0.25);
  }

  // Integer limits must not access memory out of bounds.
  {
    for (auto value : {std::numeric_limits<intmax_t>::min(), std::numeric_limits<intmax_t>::max()}) {
      intmax_t decoded = 0;
      ASSERT(rpc_core::deserialize(rpc_core::serialize(value), decoded));
      ASSERT(decoded == value);
    }
  }

  // Reject truncated fields and lengths exceeding the destination capacity.
  {
    double number = 0;
    ASSERT(!rpc_core::deserialize(std::string(7, '\0'), number));
    intmax_t integer = 0;
    ASSERT(!rpc_core::deserialize(std::string(), integer));
    ASSERT(!rpc_core::deserialize(std::string(1, '\x08'), integer));
    ASSERT(!rpc_core::deserialize(std::string(10, '\x09'), integer));
    std::array<int, 1> array{};
    ASSERT(!rpc_core::deserialize(rpc_core::serialize(std::array<int, 2>{{1, 2}}), array));
    std::tuple<std::string> nested;
    ASSERT(!rpc_core::deserialize(std::string("\x01\x05", 2), nested));
    std::vector<int> list;
    ASSERT(!rpc_core::deserialize(std::string("\x01\x01", 2), list));
    rpc_core::binary_wrap binary;
    ASSERT(!rpc_core::deserialize(std::string("\x01\x05", 2), binary));
    bool flag = false;
    ASSERT(rpc_core::serialize(true) == std::string(1, '\x01'));
    ASSERT(rpc_core::serialize(false) == std::string(1, '\x00'));
    ASSERT(rpc_core::deserialize(rpc_core::serialize(true), flag));
    ASSERT(flag);
    ASSERT(rpc_core::deserialize(rpc_core::serialize(false), flag));
    ASSERT(!flag);
    ASSERT(rpc_core::deserialize(std::string(1, '\x02'), flag));
    ASSERT(flag);
  }
}

}  // namespace rpc_core_test
