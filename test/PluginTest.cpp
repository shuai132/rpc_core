#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"
#include "plugin/Flatbuffers.hpp"
#include "plugin/Json.hpp"
#include "plugin/JsonType.h"
#include "plugin/fb/FbMsg_generated.h"

namespace RpcCoreTest {

void PluginTest() {
  using namespace RpcCore;
  {
    RpcCore_LOGI("Json...");
    nlohmann::json a;
    a["id"] = 1;
    a["name"] = "example";
    a["age"] = 18;

    auto payload = serialize(a);
    RpcCore_LOGI("Json: %s", payload.c_str());
    RpcCore_LOGI("Json: size: %zu", payload.size());

    nlohmann::json b;
    deserialize(payload, b);
    ASSERT(b["id"] == a["id"]);
    ASSERT(b["name"] == a["name"]);
    ASSERT(b["age"] == a["age"]);
  }

  {
    RpcCore_LOGI("JsonMsg<JsonType>...");
    JsonType a;
    a.id = 1;
    a.name = "example";
    a.age = 18;

    auto payload = serialize(a);
    RpcCore_LOGI("JsonType: %s", payload.c_str());
    RpcCore_LOGI("JsonType: size: %zu", payload.size());

    JsonType b;
    deserialize(payload, b);
    ASSERT(b.id == a.id);
    ASSERT(b.name == a.name);
    ASSERT(b.age == a.age);
  }

  {
    RpcCore_LOGI("Flatbuffers...");
    msg::FbMsgT a;
    a.id = 1;
    a.name = "example";
    a.age = 18;

    // flatbuffers payload is not readable
    auto payload = serialize(a);
    RpcCore_LOGI("Flatbuffers: size: %zu", payload.size());

    msg::FbMsgT b;
    deserialize(payload, b);
    ASSERT(b.id == a.id);
    ASSERT(b.name == a.name);
    ASSERT(b.age == a.age);
  }
}

}  // namespace RpcCoreTest
