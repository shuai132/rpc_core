#include <cinttypes>

// should include before "rpc_core.hpp"
#include "type/TestStruct.h"

// normal include
#include "assert_def.h"
#include "rpc_core.hpp"
#include "test.h"

namespace rpc_core_test {

void RpcTest() {
  using namespace RPC_CORE_NAMESPACE;

  // 此示例使用回环连接 实际使用时需自定义连接
  auto connection = std::make_shared<loopback_connection>();

  // 创建rpc 收发消息
  auto rpc = rpc::create(connection);

  // 定时器实现 应配合当前应用的事件循环 以确保消息收发和超时回调在同一个线程
  // 此示例使用回环连接 不做超时测试
  rpc->set_timer([](uint32_t ms, const rpc::timeout_cb& cb) {});

  /**
   * 最简单的示例
   */
  {
    // The Receiver
    rpc->subscribe("cmd", [](const std::string& msg) -> std::string {
      assert(msg == "hello");
      return "world";
    });

    // The Sender
    rpc->cmd("cmd")
        ->msg(std::string("hello"))
        ->rsp([](const std::string& rsp) {
          assert(rsp == "world");
        })
        ->call();
  }

  /**
   * 注册和发送消息 根据使用场景不同 提供以下几种方式
   */
  {
    RPC_CORE_LOG("1. 收发消息完整测试");
    // 注册监听
    rpc->subscribe("cmd1", [&](const std::string& msg) -> std::string {
      RPC_CORE_LOGI("get cmd1: %s", msg.c_str());
      ASSERT(msg == "test");
      return "ok";
    });

    // 请求支持很多方法 可根据需求使用所需部分
    bool pass = false;
    auto request = rpc->cmd("cmd1")
                       ->msg(std::string("test"))
                       ->rsp([&](const std::string& rsp) {
                         RPC_CORE_LOGI("get rsp from cmd1: %s", rsp.c_str());
                         ASSERT(rsp == "ok");
                         pass = true;
                       })
                       ->timeout([] {
                         RPC_CORE_LOGI("超时");
                       })
                       ->finally([](finish_type type) {
                         RPC_CORE_LOGI("完成: type:%d", (int)type);
                       });
    RPC_CORE_LOGI("执行请求");
    ASSERT(!pass);
    request->call();
    ASSERT(pass);

    // 其他功能测试
    RPC_CORE_LOGI("多次调用");
    pass = false;
    request->call();
    ASSERT(pass);
    RPC_CORE_LOGI("可以取消");
    pass = false;
    request->cancel();
    request->call();
    ASSERT(!pass);
    RPC_CORE_LOGI("恢复取消");
    request->reset_cancel();
    request->call();
    ASSERT(pass);
    RPC_CORE_LOGI("添加到dispose");
    pass = false;
    dispose dispose;
    request->add_to(dispose);
    dispose.dismiss();
    request->call();
    ASSERT(!pass);
    RPC_CORE_LOGI("先创建request");
    pass = false;
    request::create()
        ->cmd("cmd1")
        ->msg(std::string("test"))
        ->rsp([&](const std::string& rsp) {
          ASSERT(rsp == "ok");
          pass = true;
        })
        ->call(rpc);
    ASSERT(pass);
  }

  RPC_CORE_LOG("2. 值类型双端收发验证");
  {
    bool pass = false;

    const uint64_t VALUE = 0x00001234abcd0000;

    RPC_CORE_LOGI("TEST_VALUE: 0x%016" PRIx64, VALUE);
    rpc->subscribe("cmd2", [&](const uint64_t& msg) -> uint64_t {
      RPC_CORE_LOGI("get cmd2: 0x%016" PRIx64, msg);
      ASSERT(msg == VALUE);
      return VALUE;
    });

    rpc->cmd("cmd2")
        ->msg(VALUE)
        ->rsp([&](const uint64_t& rsp) {
          RPC_CORE_LOGI("get rsp from cmd2: 0x%016" PRIx64, rsp);
          ASSERT(rsp == VALUE);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RPC_CORE_LOG("3. 自定义的结构体类型");
  {
    bool pass = false;
    TestStruct testStruct{1, 2, 3};

    rpc->subscribe("cmd3", [&](const TestStruct& msg) -> TestStruct {
      RPC_CORE_LOGI("get cmd3");
      ASSERT(msg.a == 1);
      ASSERT(msg.b == 2);
      ASSERT(msg.c == 3);
      return testStruct;
    });
    rpc->cmd("cmd3")
        ->msg(testStruct)
        ->rsp([&](const TestStruct& rsp) {
          RPC_CORE_LOGI("get rsp from cmd3");
          ASSERT(rsp.a == 1);
          ASSERT(rsp.b == 2);
          ASSERT(rsp.c == 3);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RPC_CORE_LOG("4. finally测试");
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
        ->finally([&](finish_type type) {
          ASSERT(type == finish_type::normal);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass);
    ASSERT(pass_finally);

    pass_finally = false;
    rpc->cmd("cmd4")
        ->msg(std::string("test"))
        ->finally([&](finish_type type) {
          ASSERT(type == finish_type::no_need_rsp);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass_finally);
  }

  RPC_CORE_LOG("5. 多种传参排列组合测试");
  {
    RPC_CORE_LOG("5.1 有参数 有返回");
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

    RPC_CORE_LOG("5.2 有参数 无返回");
    {
      bool pass_cmd = false;
      rpc->subscribe("cmd5", [&](const std::string& msg) {
        ASSERT(msg == "cmd");
        pass_cmd = true;
      });
      rpc->cmd("cmd5")->msg(std::string("cmd"))->call();
      ASSERT(pass_cmd);
    }

    RPC_CORE_LOG("5.3 无参数 有返回");
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

    RPC_CORE_LOG("5.4 无参数 无返回");
    {
      bool pass_cmd = false;
      rpc->subscribe("cmd5", [&]() {
        pass_cmd = true;
      });
      rpc->cmd("cmd5")->call();
      ASSERT(pass_cmd);
    }
  }

  RPC_CORE_LOG("ping pong测试");
  {
    bool pass = false;
    rpc->ping("ping")
        ->rsp([&](const std::string& payload) {
          RPC_CORE_LOGI("get rsp from ping: %s", payload.c_str());
          ASSERT(payload == "ping");
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RPC_CORE_LOG("dispose");
  {
    auto request = rpc->ping("ping")
                       ->rsp([&](const std::string& payload) {
                         ASSERT(false);
                       })
                       ->finally([&](finish_type type) {
                         // 在call之前已取消 不会触发
                         ASSERT(false);
                       });
    {
      dispose dispose;
      request->add_to(dispose);
    }
    request->call();
  }

#ifndef RPC_CORE_FEATURE_DISABLE_FUTURE
  RPC_CORE_LOG("Future模式");
  {
    {
      auto result = rpc->ping("ping")->future<std::string>().get();
      ASSERT(result.type == finish_type::normal);
      ASSERT(result.data == "ping");
    }
    {
      auto result = rpc->ping()->future<void>().get();
      ASSERT(result.type == finish_type::normal);
    }
  }
#endif
}

}  // namespace rpc_core_test
