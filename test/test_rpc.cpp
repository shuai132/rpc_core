#include "assert_def.h"
#include "rpc_core.hpp"
#include "serialize/CustomType.h"
#include "test.h"

namespace rpc_core_test {

void test_rpc() {
  using namespace rpc_core;

  // loopback connection for test purposes, a connection should be implemented in practical use
  auto loopback = loopback_connection::create();

  // create rpc as server
  auto rpc_s = rpc::create(loopback.first);

  // timer implementation:
  // should be paired with the current application's event loop
  // to ensure message sending, receiving and timeout callbacks occur on the same thread.
  // this example uses a loopback connection and does not perform timeout testing
  rpc_s->set_timer([](uint32_t ms, const rpc::timeout_cb& cb) {
    RPC_CORE_UNUSED(ms);
    RPC_CORE_UNUSED(cb);
  });

  // mark as ready, on connected
  rpc_s->set_ready(true);

  // create rpc as client
  auto rpc_c = rpc::create(loopback.second);
  rpc_c->set_timer([](uint32_t ms, const rpc::timeout_cb& cb) {
    RPC_CORE_UNUSED(ms);
    RPC_CORE_UNUSED(cb);
  });
  rpc_c->set_ready(true);

  /**
   * simple usage
   * as an example, support send and receive std::string type
   * also support structures and most STL containers (see serialization section)
   */
  {
    // The Receiver
    rpc_s->subscribe("cmd", [](const std::string& msg) -> std::string {
      ASSERT(msg == "hello");
      return "world";
    });

    // The Sender
    rpc_c->cmd("cmd")
        ->msg(std::string("hello"))
        ->rsp([](const std::string& rsp) {
          ASSERT(rsp == "world");
        })
        ->call();
  }

  /**
   * detail usage
   */
  {
    RPC_CORE_LOG("1. send and receive data");
    rpc_s->subscribe("cmd1", [&](const std::string& msg) -> std::string {
      RPC_CORE_LOGI("get cmd1: %s", msg.c_str());
      ASSERT(msg == "test");
      return "ok";
    });

    bool pass = false;
    auto request = rpc_c->cmd("cmd1")
                       ->msg(std::string("test"))
                       ->rsp([&](const std::string& rsp) {
                         RPC_CORE_LOGI("get rsp from cmd1: %s", rsp.c_str());
                         ASSERT(rsp == "ok");
                         pass = true;
                       })
                       // or: ->rsp([](const std::string& msg, finally_t type){})
                       ->timeout([] {
                         RPC_CORE_LOGI("timeout");
                       })
                       ->finally([](finally_t type) {
                         RPC_CORE_LOGI("finally: type:%s", rpc_core::finally_t_str(type));
                       });
    RPC_CORE_LOGI("request->call()");
    ASSERT(!pass);
    request->call();
    ASSERT(pass);

    /// test other request api
    RPC_CORE_LOGI("call() can more than once");
    pass = false;
    request->call();
    ASSERT(pass);

    RPC_CORE_LOGI("request can be cancel");
    pass = false;
    request->cancel();
    request->call();
    ASSERT(!pass);

    RPC_CORE_LOGI("request can be resume");
    request->reset_cancel();
    request->call();
    ASSERT(pass);

    RPC_CORE_LOGI("add to dispose, use RAII");
    pass = false;
    {  // RAII dispose
      dispose dispose;
      request->add_to(dispose);
    }
    request->call();
    ASSERT(!pass);

    request->reset_cancel();
    {  // remove
      dispose dispose;
      request->add_to(dispose);
      dispose.remove(request);
    }
    request->call();
    ASSERT(pass);

    RPC_CORE_LOGI("can create request first");
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

    RPC_CORE_LOGI("call(cmd, msg, rsp)");
    pass = false;
    rpc_c->call("cmd1", std::string("test"), [&](const std::string& rsp) {
      ASSERT(rsp == "ok");
      pass = true;
    });
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

  RPC_CORE_LOG("2. test complex structures(including STL containers)");
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

  RPC_CORE_LOG("3. test finally");
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

  RPC_CORE_LOG("4. test various usage");
  {
    RPC_CORE_LOG("4.1 has parameter, has return value");
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

    RPC_CORE_LOG("4.2 has parameter, no return value");
    {
      bool pass_cmd = false;
      rpc_s->subscribe("cmd4", [&](const std::string& msg) {
        ASSERT(msg == "test");
        pass_cmd = true;
      });
      rpc_c->cmd("cmd4")->msg(std::string("test"))->call();
      ASSERT(pass_cmd);
    }

    RPC_CORE_LOG("4.3 no parameter, has return value");
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

    RPC_CORE_LOG("4.4 no parameter, no return value");
    {
      bool pass_cmd = false;
      rpc_s->subscribe("cmd4", [&]() {
        pass_cmd = true;
      });
      rpc_c->cmd("cmd4")->call();
      ASSERT(pass_cmd);
    }
  }

  RPC_CORE_LOG("5. test ping pong");
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

  RPC_CORE_LOG("6. test dispose");
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

#ifdef RPC_CORE_FEATURE_FUTURE
  RPC_CORE_LOG("7. test future api");
  {
    {
      auto result = rpc_c->ping("test")->future<std::string>().get();
      ASSERT(result.type == finally_t::normal);
      ASSERT(result.data == "test");
    }
    {
      auto result = rpc_c->ping()->future().get();
      ASSERT(result.type == finally_t::normal);
    }
  }
#endif

  RPC_CORE_LOG("8. test rpc which not ready");
  {
    bool pass = false;
    auto rpc_tmp = rpc::create();
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

  RPC_CORE_LOG("9. test compile: lambda capture mutable");
  {
    auto rpc = rpc::create();
    std::string tmp;
    rpc->subscribe("", [tmp]() mutable {
      tmp = "";
    });
    rpc->cmd("")->rsp([tmp]() mutable {
      tmp = "";
    });
  }

  RPC_CORE_LOG("10. rsp finally");
  {
    bool pass_cmd = false;
    bool pass_rsp = false;
    rpc_s->subscribe("cmd", [&](const std::string& msg) -> std::string {
      ASSERT(msg == "test");
      pass_cmd = true;
      return "test";
    });
    rpc_c->cmd("cmd")
        ->msg(std::string("test"))
        ->rsp([&](const std::string& msg, finally_t type) {
          ASSERT(msg == "test");
          ASSERT(type == finally_t::normal);
          pass_rsp = true;
        })
        ->call();
    ASSERT(pass_cmd);
    ASSERT(pass_rsp);
  }

  RPC_CORE_LOG("11. subscribe async");
  {
    bool pass_cmd = false;
    bool pass_rsp = false;

    rpc_s->subscribe("cmd", [&](request_response<std::string, std::string> rr) {
      ASSERT(rr->req == "test");
      pass_cmd = true;
      rr->rsp("test");
      /// or you can set result on other thread
      // std::thread([rr = std::move(rr)] {
      //   std::this_thread::sleep_for(std::chrono::seconds(1));
      //   rr->rsp("test"); // should post to rpc thread
      // }).detach();
    });
    rpc_c->cmd("cmd")
        ->msg(std::string("test"))
        ->rsp([&](const std::string& msg, finally_t type) {
          ASSERT(msg == "test");
          ASSERT(type == finally_t::normal);
          pass_rsp = true;
        })
        ->call();
    ASSERT(pass_cmd);
    ASSERT(pass_rsp);

    // check more type
    rpc_s->subscribe("cmd2", [&](const request_response<uint32_t, std::vector<std::string>>& rr) {
      ASSERT(rr->req == 999);
      pass_cmd = true;
      rr->rsp({{"test"}});
    });
    rpc_c->cmd("cmd2")
        ->msg<uint32_t>(999)
        ->rsp([&](std::vector<std::string> value, finally_t type) {
          ASSERT(value.size() == 1);
          ASSERT(value[0] == "test");
          ASSERT(type == finally_t::normal);
          pass_rsp = true;
        })
        ->call();
    ASSERT(pass_cmd);
    ASSERT(pass_rsp);
  }

  RPC_CORE_LOG("12. subscribe async: use coroutine or custom scheduler");
#if 0
  {
    /// scheduler for dispatch rsp to asio context
    auto scheduler_asio_dispatch = [&](auto handle) {
      asio::dispatch(context, std::move(handle));
    };
    /// scheduler for use asio coroutine
    auto scheduler_asio_coroutine = [&](auto handle) {
      asio::co_spawn(context, [handle = std::move(handle)]() -> asio::awaitable<void> {
        co_await handle();
      }, asio::detached);
    };

    /// 1. common usage
    rpc->subscribe("cmd", [&](request_response<std::string, std::string> rr) {
      // call rsp when data ready
      rr->rsp("world");
      // or run on context thread
      asio::dispatch(context, [rr = std::move(rr)]{ rr->rsp("world"); });
      // or run on context thread, use asio coroutine
      asio::co_spawn(context, [&, rr = std::move(rr)]() -> asio::awaitable<void> {
        asio::steady_timer timer(context);
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait();
        rr->rsp("world");
      }, asio::detached);
    });

    /// 2. custom scheduler, automatic dispatch
    rpc->subscribe("cmd", [](const request_response<std::string, std::string>& rr) {
      rr->rsp("world");
    }, scheduler_asio_dispatch);

    /// 3. custom scheduler, simple way to use asio coroutine
    rpc->subscribe("cmd", [&](request_response<std::string, std::string> rr) -> asio::awaitable<void> {
      LOG("session on cmd: %s", rr->req.c_str());
      asio::steady_timer timer(context);
      timer.expires_after(std::chrono::seconds(1));
      co_await timer.async_wait();
      rr->rsp("world");
    }, scheduler_asio_coroutine);
  }
#endif
}

}  // namespace rpc_core_test
