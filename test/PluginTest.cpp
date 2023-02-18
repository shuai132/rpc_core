#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"
#include "plugin/FlatbuffersMsg.hpp"
#include "plugin/JsonMsg.hpp"
#include "plugin/JsonType.h"
#include "plugin/fb/FbMsg_generated.h"

namespace RpcCoreTest {

void PluginTest() {
  using namespace RpcCore;
  {
    RpcCore_LOGI("JsonMsg<JsonType>...");
    JsonMsg<JsonType> a;
    a->id = 1;
    a->name = "example";
    a->age = 18;

    auto payload = a.serialize();
    RpcCore_LOGI("JsonType: %s", payload.c_str());
    RpcCore_LOGI("JsonType: size: %zu", payload.size());

    JsonMsg<JsonType> b;
    b.deSerialize(payload);
    ASSERT(b->id == a->id);
    ASSERT(b->name == a->name);
    ASSERT(b->age == a->age);
  }

  {
    RpcCore_LOGI("FlatbuffersMsg<msg::FbMsgT>...");
    FlatbuffersMsg<msg::FbMsgT> a;
    a->id = 1;
    a->name = "example";
    a->age = 18;

    // flatbuffers payload is not readable
    auto payload = a.serialize();
    RpcCore_LOGI("FlatbuffersMsg: size: %zu", payload.size());

    FlatbuffersMsg<msg::FbMsgT> b;
    b.deSerialize(payload);
    ASSERT(b->id == a->id);
    ASSERT(b->name == a->name);
    ASSERT(b->age == a->age);
  }
}

}  // namespace RpcCoreTest
