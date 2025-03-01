#include "rpc_core.hpp"
#include "assert_def.h"

using namespace rpc_core;

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

int main() {
  test_container_adaptor_invariants();
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
