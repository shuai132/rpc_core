#include <algorithm>

#include "assert_def.h"
#include "plugin/fb/FbMsg_generated.h"
#include "rpc_core/plugin/flatbuffers.hpp"

using namespace rpc_core;

static void test_reused_target_clears_absent_fields() {
  msg::FbMsgT target;
  target.id = 42;
  target.age = 18;
  target.name = "previous message";
  const auto populated = serialize(target);
  const auto empty = serialize(msg::FbMsgT{});
  ASSERT(deserialize(empty, target));
  ASSERT(target.id == 0 && target.age == 0 && target.name.empty());
  ASSERT(deserialize(populated, target));
  ASSERT(target.id == 42 && target.age == 18 && target.name == "previous message");
  ASSERT(deserialize(empty, target));
  ASSERT(target.name.empty());
}

static void test_invalid_archives_preserve_target() {
  msg::FbMsgT target;
  target.id = 42;
  target.name = "unchanged";
  const auto original = serialize(target);
  const auto replacement = serialize(msg::FbMsgT{});
  serialize_iarchive failed(replacement);
  failed.error = true;
  target << failed;
  ASSERT(failed.error && serialize(target) == original);

  for (const auto& invalid : {detail::string_view(nullptr, 0), detail::string_view(nullptr, replacement.size()),
                              detail::string_view(replacement.data(), 1),
                              detail::string_view(replacement.data(), FLATBUFFERS_MAX_BUFFER_SIZE)}) {
    ASSERT(!deserialize(invalid, target));
    ASSERT(serialize(target) == original);
  }
  auto corrupt = replacement;
  std::fill(corrupt.begin(), corrupt.begin() + sizeof(flatbuffers::uoffset_t), char(0xff));
  ASSERT(!deserialize(corrupt, target));
  ASSERT(serialize(target) == original);
  ASSERT(deserialize(replacement, target));
  ASSERT(target.id == 0 && target.name.empty());
}

int main() {
  test_reused_target_clears_absent_fields();
  test_invalid_archives_preserve_target();
}
