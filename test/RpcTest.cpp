#include <cinttypes>

#include "RpcCore.hpp"
#include "Test.h"
#include "assert_def.h"

namespace RpcCoreTest {

void RpcTest() {
  using namespace RpcCore;

  // 此示例使用回环连接 实际使用时需自定义连接
  auto connection = std::make_shared<LoopbackConnection>();

  // 创建Rpc 收发消息
  auto rpc = Rpc::create(connection);

  // 定时器实现 应配合当前应用的事件循环 以确保消息收发和超时回调在同一个线程
  // 此示例使用回环连接 不做超时测试
  rpc->setTimer([](uint32_t ms, const Rpc::TimeoutCb& cb) {});

  /**
   * 注册和发送消息 根据使用场景不同 提供以下几种方式
   */
  {
    const std::string TEST("Hello World");
    std::string test = TEST;
    RpcCore_LOGI("测试开始...");
    RpcCore_LOGI("TEST: %s, test: %s", TEST.c_str(), test.c_str());
    ASSERT(TEST == test);

    RpcCore_LOG("1. 收发消息完整测试");
    bool pass = false;
    // 在机器A上注册监听
    rpc->subscribe("cmd1", [&](const std::string& msg) -> std::string {
      RpcCore_LOGI("get cmd1: %s", msg.c_str());
      ASSERT(msg == TEST);
      return test + "test";
    });
    // 在机器B上发送请求 请求支持很多方法 可根据需求使用所需部分
    auto request = rpc->cmd("cmd1")
                       ->msg(test)
                       ->rsp([&](const std::string& rsp) {
                         RpcCore_LOGI("get rsp from cmd1: %s", rsp.c_str());
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
        ->cmd("cmd1")
        ->msg(test)
        ->rsp([&](const std::string& rsp) {
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

    RpcCore_LOGI("TEST_VALUE: 0x%016" PRIx64, VALUE);
    rpc->subscribe("cmd2", [&](const uint64_t& msg) -> uint64_t {
      RpcCore_LOGI("get cmd2: 0x%016" PRIx64, msg);
      ASSERT(msg == VALUE);
      return VALUE;
    });

    rpc->cmd("cmd2")
        ->msg(VALUE)
        ->rsp([&](const uint64_t& rsp) {
          RpcCore_LOGI("get rsp from cmd2: 0x%016" PRIx64, rsp);
          ASSERT(rsp == VALUE);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RpcCore_LOG("3. 自定义的结构体类型");
  {
    struct TestStruct {
      uint8_t a;
      uint16_t b;
      uint32_t c;
    };

    bool pass = false;
    TestStruct testStruct{1, 2, 3};

    rpc->subscribe("cmd3", [&](const TestStruct& msg) -> TestStruct {
      RpcCore_LOGI("get cmd3");
      ASSERT(msg.a == 1);
      ASSERT(msg.b == 2);
      ASSERT(msg.c == 3);
      return testStruct;
    });
    rpc->cmd("cmd3")
        ->msg(testStruct)
        ->rsp([&](const TestStruct& rsp) {
          RpcCore_LOGI("get rsp from cmd3");
          ASSERT(rsp.a == 1);
          ASSERT(rsp.b == 2);
          ASSERT(rsp.c == 3);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RpcCore_LOG("4. finally测试");
  {
    bool pass = false;
    bool pass_finally = false;
    rpc->subscribe("cmd4", [&](const std::string& msg) {
      return msg;
    });
    rpc->cmd("cmd4")
        ->msg(std::string("test"))
        ->rsp([&](const std::string& rsp) {
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
    rpc->cmd("cmd4")
        ->msg(std::string("test"))
        ->finally([&](FinishType type) {
          ASSERT(type == FinishType::NO_NEED_RSP);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass_finally);
  }

  RpcCore_LOG("5. 多种传参排列组合测试");
  {
    RpcCore_LOG("5.1 有参数 有返回");
    {
      bool pass_cmd = false;
      bool pass_rsp = false;
      rpc->subscribe("cmd5", [&](const std::string& msg) -> std::string {
        ASSERT(msg == "cmd");
        pass_cmd = true;
        return "rsp";
      });
      rpc->cmd("cmd5")
          ->msg(std::string("cmd"))
          ->rsp([&](const std::string& msg) {
            ASSERT(msg == "rsp");
            pass_rsp = true;
          })
          ->call();
      ASSERT(pass_cmd);
      ASSERT(pass_rsp);
    }

    RpcCore_LOG("5.2 有参数 无返回");
    {
      bool pass_cmd = false;
      rpc->subscribe("cmd5", [&](const std::string& msg) {
        ASSERT(msg == "cmd");
        pass_cmd = true;
      });
      rpc->cmd("cmd5")->msg("cmd")->call();
      ASSERT(pass_cmd);
    }

    RpcCore_LOG("5.3 无参数 有返回");
    {
      bool pass_cmd = false;
      bool pass_rsp = false;
      rpc->subscribe("cmd5", [&]() -> std::string {
        pass_cmd = true;
        return "rsp";
      });
      rpc->cmd("cmd5")
          ->rsp([&](const std::string& msg) {
            pass_rsp = true;
          })
          ->call();
      ASSERT(pass_cmd);
      ASSERT(pass_rsp);
    }

    RpcCore_LOG("5.4 无参数 无返回");
    {
      bool pass_cmd = false;
      rpc->subscribe("cmd5", [&]() {
        pass_cmd = true;
      });
      rpc->cmd("cmd5")->call();
      ASSERT(pass_cmd);
    }
  }

  RpcCore_LOG("ping pong测试");
  {
    bool pass = false;
    rpc->ping("ping")
        ->rsp([&](const std::string& payload) {
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
                       ->rsp([&](const std::string& payload) {
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

  RpcCore_LOG("Future模式");
  {
    //    {
    //      auto result = rpc->ping("ping")->future<std::string>().get();
    //      ASSERT(result.type == FinishType::NORMAL);
    //      ASSERT(result.data == "ping");
    //    }
    //    {
    //      auto result = rpc->ping()->future<Void>().get();
    //      ASSERT(result.type == FinishType::NORMAL);
    //    }
  }
}

}  // namespace RpcCoreTest
