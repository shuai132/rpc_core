// include first
#include "plugin/JsonType.h"
#include "plugin/RawType.h"
#include "plugin/fb/FbMsg_generated.h"
#include "plugin/flatbuffers.hpp"
#include "plugin/json.hpp"

// include other
#include "assert_def.h"
#include "rpc_core.hpp"
#include "test.h"

namespace rpc_core_test {

void test_plugin() {
  using namespace RPC_CORE_NAMESPACE;
  {
    RPC_CORE_LOGI("RawType...");
    RawType a;
    a.id = 1;
    a.name = "test";
    a.age = 18;

    auto payload = serialize(a);
    // payload is not readable
    RPC_CORE_LOGI("RawType: size: %zu", payload.size());

    RawType b;
    deserialize(payload, b);
    ASSERT(a.id == b.id);
    ASSERT(a.name == b.name);
    ASSERT(a.age == b.age);
  }

  {
    RPC_CORE_LOGI("json...");
    nlohmann::json a;
    a["id"] = 1;
    a["name"] = "test";
    a["age"] = 18;

    auto payload = serialize(a);
    RPC_CORE_LOGI("json: %s", payload.c_str());
    RPC_CORE_LOGI("json: size: %zu", payload.size());

    nlohmann::json b;
    deserialize(payload, b);
    ASSERT(b["id"] == a["id"]);
    ASSERT(b["name"] == a["name"]);
    ASSERT(b["age"] == a["age"]);
  }

  {
    RPC_CORE_LOGI("JsonType...");
    JsonType a;
    a.id = 1;
    a.name = "test";
    a.age = 18;

    auto payload = serialize(a);
    RPC_CORE_LOGI("JsonType: %s", payload.c_str());
    RPC_CORE_LOGI("JsonType: size: %zu", payload.size());

    JsonType b;
    deserialize(payload, b);
    ASSERT(b.id == a.id);
    ASSERT(b.name == a.name);
    ASSERT(b.age == a.age);
  }

  {
    RPC_CORE_LOGI("flatbuffers...");
    msg::FbMsgT a;
    a.id = 1;
    a.name = "test";
    a.age = 18;

    auto payload = serialize(a);
    // flatbuffers payload is not readable
    RPC_CORE_LOGI("flatbuffers: size: %zu", payload.size());

    msg::FbMsgT b;
    deserialize(payload, b);
    ASSERT(b.id == a.id);
    ASSERT(b.name == a.name);
    ASSERT(b.age == a.age);
  }
}

}  // namespace rpc_core_test
