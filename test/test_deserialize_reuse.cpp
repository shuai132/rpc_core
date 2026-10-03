#include "rpc_core.hpp"
#include "assert_def.h"
#include <stdexcept>

using namespace rpc_core;

struct counted_element {
  int value = 0;
  static int decodes;

  void operator>>(serialize_oarchive& ar) const { value >> ar; }
  void operator<<(serialize_iarchive& ar) {
    ++decodes;
    value << ar;
  }
  bool operator<(const counted_element& other) const { return value < other.value; }
  bool operator==(const counted_element& other) const { return value == other.value; }
};

int counted_element::decodes = 0;

struct counted_element_hash {
  size_t operator()(const counted_element& item) const { return std::hash<int>{}(item.value); }
};

template <typename Container>
static void check_container_element_boundaries(const Container& expected) {
  const auto encoded = serialize(expected);
  const auto element = serialize(counted_element{1});
  const auto first_end = detail::auto_size(2).serialize().size() +
      detail::auto_size(element.size()).serialize().size() + element.size();
  for (size_t length = 0; length < encoded.size(); ++length) {
    Container actual{};
    counted_element::decodes = 0;
    ASSERT(!deserialize(encoded.substr(0, length), actual));
    // Only complete element frames may reach a user-defined decoder.
    ASSERT(counted_element::decodes == (length >= first_end ? 1 : 0));
  }
  Container actual{};
  counted_element::decodes = 0;
  ASSERT(deserialize(encoded, actual));
  ASSERT(counted_element::decodes == 2 && actual == expected);
}

static void test_container_element_boundaries() {
  check_container_element_boundaries(std::array<counted_element, 2>{{{1}, {2}}});
  check_container_element_boundaries(std::vector<counted_element>{{1}, {2}});
  check_container_element_boundaries(std::list<counted_element>{{1}, {2}});
  check_container_element_boundaries(std::deque<counted_element>{{1}, {2}});
  check_container_element_boundaries(std::forward_list<counted_element>{{1}, {2}});
  check_container_element_boundaries(std::set<counted_element>{{1}, {2}});
  check_container_element_boundaries(std::multiset<counted_element>{{1}, {2}});
  check_container_element_boundaries(std::unordered_set<counted_element, counted_element_hash>{{1}, {2}});
  check_container_element_boundaries(std::unordered_multiset<counted_element, counted_element_hash>{{1}, {2}});
}

template <typename T>
void check_replacement(const T& expected, T actual) {
  auto encoded = serialize(expected);
  ASSERT(deserialize(encoded, actual));
  ASSERT(actual == expected);
  ASSERT(deserialize(encoded, actual));
  ASSERT(actual == expected);
  encoded.pop_back();
  ASSERT(!deserialize(encoded, actual));
  ASSERT(deserialize(serialize(T{}), actual));
  ASSERT(actual.empty());
}

struct Reverse {
  bool reverse;
  bool operator()(int a, int b) const { return reverse ? a > b : a < b; }
};

struct ImmutableCompare {
  const bool reverse = true;
  bool operator()(int a, int b) const { return reverse ? a > b : a < b; }
};

struct ImmutableHash {
  const size_t salt = 7;
  size_t operator()(int value) const { return std::hash<int>{}(value) ^ salt; }
};

struct ImmutableEqual {
  const int state = 1;
  bool operator()(int a, int b) const { return a == b; }
};

static void test_immutable_associative_state() {
  check_replacement(std::map<int, int, ImmutableCompare>{{1, 2}},
                    std::map<int, int, ImmutableCompare>{{1, 9}, {9, 9}});
  check_replacement(std::multimap<int, int, ImmutableCompare>{{1, 2}, {1, 3}},
                    std::multimap<int, int, ImmutableCompare>{{9, 9}});
  check_replacement(std::set<int, ImmutableCompare>{1, 2}, std::set<int, ImmutableCompare>{9});
  check_replacement(std::multiset<int, ImmutableCompare>{1, 1}, std::multiset<int, ImmutableCompare>{9});
  check_replacement(std::unordered_map<int, int, ImmutableHash, ImmutableEqual>{{1, 2}},
                    std::unordered_map<int, int, ImmutableHash, ImmutableEqual>{{9, 9}});
  check_replacement(std::unordered_multimap<int, int, ImmutableHash, ImmutableEqual>{{1, 2}, {1, 3}},
                    std::unordered_multimap<int, int, ImmutableHash, ImmutableEqual>{{9, 9}});
  check_replacement(std::unordered_set<int, ImmutableHash, ImmutableEqual>{1, 2},
                    std::unordered_set<int, ImmutableHash, ImmutableEqual>{9});
  check_replacement(std::unordered_multiset<int, ImmutableHash, ImmutableEqual>{1, 1},
                    std::unordered_multiset<int, ImmutableHash, ImmutableEqual>{9});

  std::unordered_map<int, int, ImmutableHash, ImmutableEqual> hashed;
  hashed.max_load_factor(0.25f);
  ASSERT(deserialize(serialize(std::map<int, int>{{1, 2}}), hashed));
  ASSERT(hashed.hash_function().salt == 7 && hashed.key_eq().state == 1);
  ASSERT(hashed.max_load_factor() == 0.25f && hashed.at(1) == 2);

  auto compare = [reverse = true](const std::unique_ptr<int>& a, const std::unique_ptr<int>& b) {
    return reverse ? *a > *b : *a < *b;
  };
  std::map<std::unique_ptr<int>, std::unique_ptr<int>, decltype(compare)> mapped(compare);
  mapped.emplace(std::unique_ptr<int>(new int(1)), std::unique_ptr<int>(new int(2)));
  mapped.emplace(std::unique_ptr<int>(new int(3)), std::unique_ptr<int>(new int(4)));
  auto encoded = serialize(mapped);
  ASSERT(deserialize(encoded, mapped));
  ASSERT(mapped.size() == 2 && *mapped.begin()->first == 3 && *mapped.begin()->second == 4);
  encoded.pop_back();
  ASSERT(!deserialize(encoded, mapped));
}

static void test_fixed_array_lengths() {
  const std::array<int, 3> expected{{1, 2, 3}};
  for (const auto& data : {serialize(std::array<int, 0>{}),
                           serialize(std::array<int, 2>{{1, 2}}),
                           serialize(std::array<int, 4>{{1, 2, 3, 4}})}) {
    auto actual = expected;
    ASSERT(!deserialize(data, actual));
    ASSERT(actual == expected);
    detail::msg_wrapper msg;
    msg.data = data;
    auto decoded = msg.unpack_as<std::array<int, 3>>();
    ASSERT(!decoded.first);
    ASSERT((decoded.second == std::array<int, 3>{}));
  }
  std::array<int, 3> actual{};
  ASSERT(deserialize(serialize(expected), actual) && actual == expected);
  std::array<int, 0> empty{};
  ASSERT(deserialize(serialize(empty), empty));
  std::tuple<std::array<int, 3>> nested;
  ASSERT(!deserialize(serialize(std::make_tuple(std::array<int, 0>{})), nested));

  auto connections = loopback_connection::create();
  auto server = rpc::create(connections.first);
  auto client = rpc::create(connections.second);
  client->set_ready(true);
  int calls = 0;
  server->subscribe("array", [&](std::array<int, 3> value) {
    ASSERT(value == expected);
    ++calls;
  });
  client->cmd("array")->msg(std::array<int, 0>{})->call();
  ASSERT(calls == 0);
  client->cmd("array")->msg(expected)->call();
  ASSERT(calls == 1);
}

template <typename T>
static void check_chrono_round_trip(const T& value) {
  auto encoded = serialize(value);
  T decoded{};
  ASSERT(deserialize(encoded, decoded) && decoded == value);
  encoded.pop_back();
  ASSERT(!deserialize(encoded, decoded));
  ASSERT(decoded == value);
}

static void test_floating_chrono() {
  for (double value : {0.0, 1.75, -0.125}) {
    check_chrono_round_trip(std::chrono::duration<double>(value));
    check_chrono_round_trip(std::chrono::duration<float>(static_cast<float>(value)));
    using time_point = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;
    check_chrono_round_trip(time_point(std::chrono::duration<double>(value)));
  }
  check_chrono_round_trip(std::chrono::milliseconds(-1234));
  ASSERT(serialize(std::chrono::milliseconds(-1234)) == detail::auto_intmax(-1234).serialize());
}

static void test_container_adaptor_invariants() {
  std::priority_queue<int, std::vector<int>, Reverse> source(Reverse{true});
  std::priority_queue<int, std::vector<int>, Reverse> target(Reverse{false});
  for (int value : {1, 3, 2}) source.push(value);
  ASSERT(deserialize(serialize(source), target));
  for (int expected : {3, 2, 1}) {
    ASSERT(target.top() == expected);
    target.pop();
  }
  std::priority_queue<int> decoded;
  ASSERT(deserialize(serialize(std::vector<int>{1, 2, 3}), decoded));
  ASSERT(decoded.top() == 3);
  auto truncated = serialize(std::vector<int>{1, 3, 2});
  truncated.pop_back();
  ASSERT(!deserialize(truncated, decoded));
  ASSERT(decoded.top() == 3);
  std::queue<int> queue;
  ASSERT(deserialize(serialize(std::deque<int>{1, 2}), queue));
  ASSERT(queue.front() == 1 && queue.back() == 2);
  std::stack<int> stack;
  ASSERT(deserialize(serialize(std::deque<int>{1, 2}), stack));
  ASSERT(stack.top() == 2);
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
struct throwing_heap_element {
  int value = 0;
  void operator>>(serialize_oarchive& ar) const { value >> ar; }
  void operator<<(serialize_iarchive& ar) {
    value << ar;
    if (!ar.error && value == 99) throw std::runtime_error("element decode failed");
  }
};

struct heap_compare {
  bool reverse;
  bool operator()(const throwing_heap_element& a, const throwing_heap_element& b) const noexcept {
    return reverse ? a.value > b.value : a.value < b.value;
  }
};

template <typename Container>
static void check_heap_after_decode_exception() {
  for (bool reverse : {false, true}) {
    std::priority_queue<throwing_heap_element, Container, heap_compare> target(heap_compare{reverse});
    target.push({42});
    const auto encoded = serialize(Container{{2}, {3}, {1}, {99}});
    bool caught = false;
    try {
      deserialize(encoded, target);
    } catch (const std::runtime_error& error) {
      caught = std::string(error.what()) == "element decode failed";
    }
    ASSERT(caught && target.size() == 3);
    ASSERT(target.top().value == (reverse ? 1 : 3));
    target.push({4});
    for (int i = 0; i < 4; ++i) {
      ASSERT(target.top().value == (reverse ? i + 1 : 4 - i));
      target.pop();
    }
    ASSERT(target.empty());
    ASSERT(deserialize(serialize(Container{{5}, {6}}), target));
    ASSERT(target.size() == 2 && target.top().value == (reverse ? 5 : 6));
  }
}
#endif

static void test_failed_binary_decode_preserves_storage_bounds() {
  std::string original = "old";
  binary_wrap value;
  const auto encoded = serialize(binary_wrap(&original[0], original.size()));
  ASSERT(deserialize(encoded, value));
  auto storage = value._data_;
  auto pointer = value.data;
  for (const auto& invalid : {std::string{}, serialize(size_t(1000)) + "short"}) {
    ASSERT(!deserialize(invalid, value));
    ASSERT(value.size == original.size() && value.data == pointer && value._data_ == storage);
    ASSERT(serialize(value) == encoded);
  }
  ASSERT(deserialize(serialize(binary_wrap{}), value));
  ASSERT(value.size == 0);
  ASSERT(deserialize(encoded, value));
  ASSERT(serialize(value) == encoded);

  auto nested = encoded;
  ASSERT(deserialize(serialize(binary_wrap(&nested[0], nested.size())), value));
  detail::string_view borrowed(static_cast<const char*>(value.data), value.size);
  ASSERT(deserialize(borrowed, value));
  ASSERT(serialize(value) == encoded);
}

enum class SmallUnsigned : uint8_t { value = 1 };
enum class SmallSigned : int8_t { value = 1 };
enum class WideSigned : intmax_t { value = 1 };
enum class WideUnsigned : uintmax_t { value = 1 };

static void test_enum_decode_rejects_narrowing() {
  SmallUnsigned small = SmallUnsigned::value;
  ASSERT(!deserialize(serialize(uintmax_t(256)), small));
  ASSERT(small == SmallUnsigned::value);
  SmallSigned signed_small = SmallSigned::value;
  for (auto invalid : {uintmax_t(128), uintmax_t(255), static_cast<uintmax_t>(intmax_t(-129))}) {
    ASSERT(!deserialize(serialize(invalid), signed_small));
    ASSERT(signed_small == SmallSigned::value);
  }
  for (int value : {-128, -1, 0, 1, 127}) {
    auto expected = static_cast<SmallSigned>(value);
    ASSERT(deserialize(serialize(expected), signed_small));
    ASSERT(signed_small == expected);
  }
  ASSERT(deserialize(serialize(uintmax_t(255)), small));
  ASSERT(static_cast<uint8_t>(small) == 255);
  for (auto value : {(std::numeric_limits<intmax_t>::min)(), intmax_t(-1), (std::numeric_limits<intmax_t>::max)()}) {
    auto expected = static_cast<WideSigned>(value);
    WideSigned actual{};
    ASSERT(deserialize(serialize(expected), actual) && actual == expected);
  }
  const auto largest = static_cast<WideUnsigned>((std::numeric_limits<uintmax_t>::max)());
  WideUnsigned actual{};
  ASSERT(deserialize(serialize(largest), actual) && actual == largest);
  for (const auto& invalid : {std::string{}, std::string("\x02\x01", 2)}) {
    small = SmallUnsigned::value;
    ASSERT(!deserialize(invalid, small));
    ASSERT(small == SmallUnsigned::value);
  }
}

static void test_pair_decode_preserves_reference_bindings() {
  int first = 10;
  std::string second = "old";
  std::pair<int&, std::string&> references(first, second);
  const auto encoded = serialize(std::make_pair(42, std::string("new")));
  ASSERT(deserialize(encoded, references));
  ASSERT(first == 42 && second == "new");
  ASSERT(&references.first == &first && &references.second == &second);
  first = 7;
  second = "unchanged";
  auto truncated = encoded.substr(0, encoded.size() - 1);
  ASSERT(!deserialize(truncated, references));
  ASSERT(first == 7 && second == "unchanged");
  std::pair<int, std::string> value{7, "unchanged"};
  ASSERT(!deserialize(truncated, value));
  ASSERT(value == std::make_pair(7, std::string("unchanged")));
  ASSERT(deserialize(encoded, value));
  ASSERT(value == std::make_pair(42, std::string("new")));
}

template <typename Char>
static void test_string_decode_from_own_storage() {
  using String = std::basic_string<Char>;
  for (size_t length : {size_t(8), size_t(128)}) {
    String source(length, Char('a'));
    for (size_t i = 0; i < length; ++i) source[i] = Char(i + 1);
    source[2] = Char(0);
    for (const auto& range : {std::make_pair(size_t(0), length), std::make_pair(size_t(0), length / 2),
                              std::make_pair(size_t(1), length - 1), std::make_pair(length / 4, length / 2)}) {
      auto actual = source;
      const auto expected = actual.substr(range.first, range.second);
      detail::string_view input(reinterpret_cast<const char*>(actual.data() + range.first), range.second * sizeof(Char));
      ASSERT(deserialize(input, actual));
      ASSERT(actual == expected);
    }
    if (sizeof(Char) > 1) {
      auto actual = source;
      const char* unaligned = reinterpret_cast<const char*>(actual.data()) + 1;
      String expected(length - 1, Char{});
      std::memcpy(&expected[0], unaligned, expected.size() * sizeof(Char));
      ASSERT(deserialize(detail::string_view(unaligned, expected.size() * sizeof(Char)), actual));
      ASSERT(actual == expected);
    }
  }

  String actual(128, Char('z'));
  actual.resize(actual.capacity(), Char('z'));
  auto expected = actual;
  expected.push_back(Char{});
  // The terminating zero is part of the readable buffer, even when growing requires allocation.
  detail::string_view input(reinterpret_cast<const char*>(actual.data()), (actual.size() + 1) * sizeof(Char));
  ASSERT(deserialize(input, actual));
  ASSERT(actual == expected);

  actual.reserve(512);
  actual.clear();
  const auto capacity = actual.capacity();
  const auto storage = actual.data();
  String external(128, Char('x'));
  const auto encoded = serialize(external);
  ASSERT(deserialize(encoded, actual));
  ASSERT(actual == external && actual.capacity() == capacity && actual.data() == storage);
  ASSERT(deserialize(detail::string_view(nullptr, 0), actual));
  ASSERT(actual.empty());
}

int main() {
  test_container_element_boundaries();
  test_string_decode_from_own_storage<char>();
  test_string_decode_from_own_storage<wchar_t>();
  test_string_decode_from_own_storage<char16_t>();
  test_string_decode_from_own_storage<char32_t>();
  test_pair_decode_preserves_reference_bindings();
  test_enum_decode_rejects_narrowing();
  test_failed_binary_decode_preserves_storage_bounds();
  test_container_adaptor_invariants();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  check_heap_after_decode_exception<std::vector<throwing_heap_element>>();
  check_heap_after_decode_exception<std::deque<throwing_heap_element>>();
#endif
  test_fixed_array_lengths();
  test_floating_chrono();
  test_immutable_associative_state();
  check_replacement(std::vector<int>{1, 2}, std::vector<int>{9});
  check_replacement(std::list<int>{1, 2}, std::list<int>{9});
  check_replacement(std::deque<std::string>{"a", "b"}, std::deque<std::string>{"old"});
  check_replacement(std::forward_list<int>{1, 2}, std::forward_list<int>{9});
  check_replacement(std::set<int>{1, 2}, std::set<int>{9});
  check_replacement(std::multiset<int>{1, 1}, std::multiset<int>{9});
  check_replacement(std::set<bool>{false, true}, std::set<bool>{true});
  check_replacement(std::unordered_set<int>{1, 2}, std::unordered_set<int>{9});
  check_replacement(std::unordered_multiset<int>{1, 1}, std::unordered_multiset<int>{9});
  check_replacement(std::map<int, int>{{1, 2}}, std::map<int, int>{{1, 9}, {9, 9}});
  check_replacement(std::multimap<int, int>{{1, 2}, {1, 3}}, std::multimap<int, int>{{9, 9}});
  check_replacement(std::unordered_map<int, int>{{1, 2}}, std::unordered_map<int, int>{{9, 9}});
  check_replacement(std::unordered_multimap<int, int>{{1, 2}}, std::unordered_multimap<int, int>{{9, 9}});
  std::map<int, int, Reverse> ordered(Reverse{true});
  ordered.emplace(9, 9);
  ASSERT(deserialize(serialize(std::map<int, int>{{1, 2}, {3, 4}}), ordered));
  ASSERT(ordered.key_comp().reverse && ordered.begin()->first == 3);

  std::vector<std::unique_ptr<int>> move_only;
  move_only.emplace_back(new int(3));
  auto encoded = serialize(move_only);
  ASSERT(deserialize(encoded, move_only));
  ASSERT(move_only.size() == 1 && *move_only.front() == 3);
  std::map<int, std::unique_ptr<int>> mapped;
  mapped.emplace(1, std::unique_ptr<int>(new int(3)));
  ASSERT(deserialize(serialize(mapped), mapped));
  ASSERT(mapped.size() == 1 && *mapped.at(1) == 3);

  auto shared = std::make_shared<int>(9);
  auto unique = std::unique_ptr<int>(new int(9));
  for (auto malformed : {std::string{}, std::string(1, '\x01')}) {
    ASSERT(!deserialize(malformed, shared));
    ASSERT(!deserialize(malformed, unique));
  }
  ASSERT(deserialize(serialize(std::shared_ptr<int>{}), shared) && !shared);
  ASSERT(deserialize(serialize(std::unique_ptr<int>{}), unique) && !unique);
  ASSERT(deserialize(serialize(std::make_shared<int>(5)), shared) && *shared == 5);
  ASSERT(deserialize(serialize(std::unique_ptr<int>(new int(5))), unique) && *unique == 5);
}
