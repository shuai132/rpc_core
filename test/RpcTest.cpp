#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"

namespace RpcCoreTest {

namespace AppMsg {
const char* CMD1 = "CMD1";
const char* CMD2 = "CMD2";
const char* CMD3 = "CMD3";
const char* CMD4 = "CMD4";
}  // namespace AppMsg

struct TestStruct {
  uint8_t a;
  uint16_t b;
  uint32_t c;
};

void RpcTest() {
  using namespace RpcCore;
  using String = RpcCore::String;

  // 回环的连接 用于测试 实际使用应为具体传输协议实现的Connection
  auto connection = std::make_shared<LoopbackConnection>();

  // 创建Rpc 收发消息
  auto rpc = Rpc::create(connection);
  rpc->setTimer([](uint32_t ms, const Rpc::TimeoutCb& cb) {
    // 定时器实现 应当配合当前应用的事件循环 确保消息收发和超时在同一个线程
    // 此示例为回环的连接 不需要具体实现
  });

  /**
   * 注册和发送消息 根据使用场景不同 提供以下几种方式
   * 注:
   * 1. 收发类型为Message即可，可自定义序列化/反序列化。
   * 2. 内部提供了常用的Message子类型。包括二进制数据 也可使用内部题二进制值类型。
   * 3. send可附加超时回调和超时时间
   */
  {
    const std::string TEST("Hello World");
    String test = TEST;
    RpcCore_LOGI("测试开始...");
    RpcCore_LOGI("TEST: %s, test: %s", TEST.c_str(), test.c_str());
    ASSERT(TEST == test);

    RpcCore_LOG("1. 收发消息完整测试");
    bool pass = false;
    // 在机器A上注册监听
    rpc->subscribe<String, String>(AppMsg::CMD1, [&](const String& msg) {
      RpcCore_LOGI("get AppMsg::CMD1: %s", msg.c_str());
      ASSERT(msg == TEST);
      return test + "test";
    });
    // 在机器B上发送请求 请求支持很多方法 可根据需求使用所需部分
    auto request = rpc->createRequest()
                       ->cmd(AppMsg::CMD1)
                       ->msg(test)
                       ->rsp<String>([&](const String& rsp) {
                         RpcCore_LOGI("get rsp from AppMsg::CMD1: %s", rsp.c_str());
                         ASSERT(rsp == TEST + "test");
                         pass = true;
                       })
                       ->timeout([] {
                         RpcCore_LOGI("超时");
                       })
                       ->finally([](FinishType type) {
                         RpcCore_LOGI("完成: type:%d", (int)type);
                       });
    RpcCore_LOGI("执行请求");
    ASSERT(!pass);
    request->call();
    ASSERT(pass);

    // 其他功能测试
    RpcCore_LOGI("多次调用");
    pass = false;
    request->call();
    ASSERT(pass);
    RpcCore_LOGI("可以取消");
    pass = false;
    request->cancel();
    request->call();
    ASSERT(!pass);
    RpcCore_LOGI("恢复取消");
    request->unCancel();
    request->call();
    ASSERT(pass);
    RpcCore_LOGI("添加到Dispose");
    pass = false;
    Dispose dispose;
    request->addTo(dispose);
    dispose.dispose();
    request->call();
    ASSERT(!pass);
    RpcCore_LOGI("先创建Request");
    pass = false;
    Request::create()
        ->cmd(AppMsg::CMD1)
        ->msg(test)
        ->rsp<String>([&](const String& rsp) {
          ASSERT(rsp == TEST + "test");
          pass = true;
        })
        ->call(rpc);
    ASSERT(pass);
  }

  RpcCore_LOG("2. 值类型双端收发验证");
  {
    bool pass = false;

    const uint64_t VALUE = 0x00001234abcd0000;
    using UInt64_t = Raw<uint64_t>;

    RpcCore_LOGI("TEST_VALUE: 0x%016llx", VALUE);
    rpc->subscribe<UInt64_t, UInt64_t>(AppMsg::CMD2, [&](const UInt64_t& msg) {
      RpcCore_LOGI("get AppMsg::CMD2: 0x%llx", msg.value);
      ASSERT(msg.value == VALUE);
      return VALUE;
    });

    rpc->createRequest()
        ->cmd(AppMsg::CMD2)
        ->msg(UInt64_t(VALUE))
        ->rsp<UInt64_t>([&](const UInt64_t& rsp) {
          RpcCore_LOGI("get rsp from AppMsg::CMD2: 0x%llx", rsp.value);
          ASSERT(rsp.value == VALUE);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RpcCore_LOG("3. 自定义的结构体类型");
  {
    bool pass = false;
    TestStruct testStruct{1, 2, 3};
    using RStruct = RpcCore::Struct<TestStruct>;

    rpc->subscribe<RStruct, RStruct>(AppMsg::CMD3, [&](const RStruct& msg) {
      RpcCore_LOGI("get AppMsg::CMD3");
      ASSERT(msg.value.a == 1);
      ASSERT(msg.value.b == 2);
      ASSERT(msg.value.c == 3);
      return testStruct;
    });
    rpc->createRequest()
        ->cmd(AppMsg::CMD3)
        ->msg(RStruct(testStruct))
        ->rsp<RStruct>([&](const RStruct& rsp) {
          RpcCore_LOGI("get rsp from AppMsg::CMD3");
          ASSERT(rsp.value.a == 1);
          ASSERT(rsp.value.b == 2);
          ASSERT(rsp.value.c == 3);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RpcCore_LOG("4. finally测试");
  {
    bool pass = false;
    bool pass_finally = false;
    rpc->subscribe<String, String>(AppMsg::CMD4, [&](const String& msg) {
      return msg;
    });
    rpc->createRequest()
        ->cmd(AppMsg::CMD4)
        ->msg(String("test"))
        ->rsp<String>([&](const String& rsp) {
          ASSERT(rsp == "test");
          pass = true;
        })
        ->finally([&](FinishType type) {
          ASSERT(type == FinishType::NORMAL);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass);
    ASSERT(pass_finally);

    pass_finally = false;
    rpc->createRequest()
        ->cmd(AppMsg::CMD4)
        ->msg(String("test"))
        ->finally([&](FinishType type) {
          ASSERT(type == FinishType::NO_NEED_RSP);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass_finally);
  }

  RpcCore_LOG("PING PONG测试");
  {
    bool pass = false;
    rpc->ping("ping")
        ->rsp<String>([&](const String& payload) {
          RpcCore_LOGI("get rsp from ping: %s", payload.c_str());
          ASSERT(payload == "ping");
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RpcCore_LOG("Dispose");
  {
    auto request = rpc->ping("ping")
                       ->rsp<String>([&](const String& payload) {
                         ASSERT(false);
                       })
                       ->finally([&](FinishType type) {
                         // 在call之前已取消 不会触发
                         ASSERT(false);
                       });
    {
      Dispose dispose;
      request->addTo(dispose);
    }
    request->call();
  }
}

}  // namespace RpcCoreTest
