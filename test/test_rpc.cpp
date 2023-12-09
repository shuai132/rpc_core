#include "assert_def.h"
#include "rpc_core.hpp"
#include "serialize/CustomType.h"
#include "test.h"

namespace rpc_core_test {

void test_rpc() {
  using namespace rpc_core;

  // 此示例使用回环连接 实际使用时需自定义连接
  auto loopback = loopback_connection::create();

  // 创建rpc server
  auto rpc_s = rpc::create(loopback.first);

  // 定时器实现 应配合当前应用的事件循环 以确保消息收发和超时回调在同一个线程
  // 此示例使用回环连接 不做超时测试
  rpc_s->set_timer([](uint32_t ms, const rpc::timeout_cb& cb) {
    RPC_CORE_UNUSED(ms);
    RPC_CORE_UNUSED(cb);
  });

  // 已连接时设置ready
  rpc_s->set_ready(true);

  // 创建rpc client
  auto rpc_c = rpc::create(loopback.second);
  rpc_c->set_timer([](uint32_t ms, const rpc::timeout_cb& cb) {
    RPC_CORE_UNUSED(ms);
    RPC_CORE_UNUSED(cb);
  });
  rpc_c->set_ready(true);

  /**
   * 简单示例
   * 以收发`std::string`为例，支持结构体和绝大多数STL容器（见序列化章节）。
   */
  {
    // The Receiver
    rpc_s->subscribe("cmd", [](const std::string& msg) -> std::string {
      assert(msg == "hello");
      return "world";
    });

    // The Sender
    rpc_c->cmd("cmd")
        ->msg(std::string("hello"))
        ->rsp([](const std::string& rsp) {
          assert(rsp == "world");
        })
        ->call();
  }

  /**
   * 详细测试
   * 根据使用场景不同 提供以下几种方式
   */
  {
    RPC_CORE_LOG("1. 收发消息完整测试");
    // 注册监听
    rpc_s->subscribe("cmd1", [&](const std::string& msg) -> std::string {
      RPC_CORE_LOGI("get cmd1: %s", msg.c_str());
      ASSERT(msg == "test");
      return "ok";
    });

    // 请求支持很多方法 可根据需求使用所需部分
    bool pass = false;
    auto request = rpc_c->cmd("cmd1")
                       ->msg(std::string("test"))
                       ->rsp([&](const std::string& rsp) {
                         RPC_CORE_LOGI("get rsp from cmd1: %s", rsp.c_str());
                         ASSERT(rsp == "ok");
                         pass = true;
                       })
                       ->timeout([] {
                         RPC_CORE_LOGI("timeout");
                       })
                       ->finally([](finally_t type) {
                         RPC_CORE_LOGI("finally: type:%s", rpc_core::request::finally_t_str(type));
                       });
    RPC_CORE_LOGI("执行请求");
    ASSERT(!pass);
    request->call();
    ASSERT(pass);

    /// 其他功能测试
    RPC_CORE_LOGI("多次调用");
    pass = false;
    request->call();
    ASSERT(pass);

    RPC_CORE_LOGI("测试取消");
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
    {  // RAII dispose
      dispose dispose;
      request->add_to(dispose);
    }
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
        ->call(rpc_c);
    ASSERT(pass);

    RPC_CORE_LOGI("no_such_cmd");
    pass = false;
    request::create()
        ->cmd("cmd_xx")
        ->msg(std::string("test"))
        ->rsp([] {
          ASSERT(false);
        })
        ->finally([&](finally_t type) {
          ASSERT(type == finally_t::no_such_cmd);
          pass = true;
        })
        ->call(rpc_c);
    ASSERT(pass);
  }

  RPC_CORE_LOG("2. 复杂结构体类型测试（包含STL容器）");
  {
    bool pass = false;
    CustomType customType;
    customType.id = 1;
    customType.ids = {1, 2, 3};
    customType.name = "test";

    rpc_s->subscribe("cmd2", [&](const CustomType& msg) -> CustomType {
      RPC_CORE_LOGI("get cmd2");
      ASSERT(msg == customType);
      return customType;
    });
    rpc_c->cmd("cmd2")
        ->msg(customType)
        ->rsp([&](const CustomType& rsp) {
          RPC_CORE_LOGI("get rsp from cmd2");
          ASSERT(rsp == customType);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RPC_CORE_LOG("3. finally测试");
  {
    bool pass = false;
    bool pass_finally = false;
    rpc_s->subscribe("cmd3", [&](const std::string& msg) {
      return msg;
    });
    rpc_c->cmd("cmd3")
        ->msg(std::string("test"))
        ->rsp([&](const std::string& rsp) {
          ASSERT(rsp == "test");
          pass = true;
        })
        ->finally([&](finally_t type) {
          ASSERT(type == finally_t::normal);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass);
    ASSERT(pass_finally);

    pass_finally = false;
    rpc_c->cmd("cmd3")
        ->msg(std::string("test"))
        ->finally([&](finally_t type) {
          ASSERT(type == finally_t::no_need_rsp);
          ASSERT(!pass_finally);
          pass_finally = true;
        })
        ->call();
    ASSERT(pass_finally);
  }

  RPC_CORE_LOG("4. 多种使用场景测试");
  {
    RPC_CORE_LOG("4.1 有参数 有返回");
    {
      bool pass_cmd = false;
      bool pass_rsp = false;
      rpc_s->subscribe("cmd4", [&](const std::string& msg) -> std::string {
        ASSERT(msg == "test");
        pass_cmd = true;
        return "test";
      });
      rpc_c->cmd("cmd4")
          ->msg(std::string("test"))
          ->rsp([&](const std::string& msg) {
            ASSERT(msg == "test");
            pass_rsp = true;
          })
          ->call();
      ASSERT(pass_cmd);
      ASSERT(pass_rsp);
    }

    RPC_CORE_LOG("4.2 有参数 无返回");
    {
      bool pass_cmd = false;
      rpc_s->subscribe("cmd4", [&](const std::string& msg) {
        ASSERT(msg == "test");
        pass_cmd = true;
      });
      rpc_c->cmd("cmd4")->msg(std::string("test"))->call();
      ASSERT(pass_cmd);
    }

    RPC_CORE_LOG("4.3 无参数 有返回");
    {
      bool pass_cmd = false;
      bool pass_rsp = false;
      rpc_s->subscribe("cmd4", [&]() -> std::string {
        pass_cmd = true;
        return "test";
      });
      rpc_c->cmd("cmd4")
          ->rsp([&](const std::string& msg) {
            pass_rsp = true;
            ASSERT(msg == "test");
          })
          ->call();
      ASSERT(pass_cmd);
      ASSERT(pass_rsp);
    }

    RPC_CORE_LOG("4.4 无参数 无返回");
    {
      bool pass_cmd = false;
      rpc_s->subscribe("cmd4", [&]() {
        pass_cmd = true;
      });
      rpc_c->cmd("cmd4")->call();
      ASSERT(pass_cmd);
    }
  }

  RPC_CORE_LOG("5. ping pong测试");
  {
    bool pass = false;
    rpc_c->ping("test")
        ->rsp([&](const std::string& payload) {
          RPC_CORE_LOGI("get rsp from ping: %s", payload.c_str());
          ASSERT(payload == "test");
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }

  RPC_CORE_LOG("6. dispose测试");
  {
    bool pass = false;
    auto request = rpc_c->ping("test")
                       ->rsp([&](const std::string& payload) {
                         RPC_CORE_UNUSED(payload);
                         ASSERT(false);
                       })
                       ->finally([&](finally_t type) {
                         ASSERT(type == finally_t::canceled);
                         pass = true;
                       });
    {
      dispose dispose;
      request->add_to(dispose);
    }
    request->call();
    ASSERT(pass);
  }

#ifndef RPC_CORE_FEATURE_DISABLE_FUTURE
  RPC_CORE_LOG("7. future模式测试");
  {
    {
      auto result = rpc_c->ping("test")->future<std::string>().get();
      ASSERT(result.type == finally_t::normal);
      ASSERT(result.data == "test");
    }
    {
      auto result = rpc_c->ping()->future<void>().get();
      ASSERT(result.type == finally_t::normal);
    }
  }
#endif

  RPC_CORE_LOG("8. 未ready的rpc对象");
  {
    bool pass = false;
    auto rpc_tmp = rpc::create(loopback.first);
    rpc_tmp->cmd("cmd")->call();  // should not crash
    rpc_tmp->cmd("cmd")
        ->finally([&](finally_t type) {
          RPC_CORE_LOG("finally: %d", (int)type);
          ASSERT(type == finally_t::rpc_not_ready);
          pass = true;
        })
        ->call();
    ASSERT(pass);
  }
}

}  // namespace rpc_core_test
