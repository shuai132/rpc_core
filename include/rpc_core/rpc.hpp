#pragma once

#include <memory>
#include <utility>

// config
#include "config.hpp"

// include
#include "connection.hpp"
#include "detail/callable/callable.hpp"
#include "detail/msg_dispatcher.hpp"
#include "detail/noncopyable.hpp"
#include "request_response.hpp"

namespace rpc_core {

class request;
using request_s = std::shared_ptr<request>;

class rpc : detail::noncopyable, public std::enable_shared_from_this<rpc> {
 public:
  using timeout_cb = detail::msg_dispatcher::timeout_cb;

 public:
  template <typename... Args>
  static std::shared_ptr<rpc> create(Args&&... args) {
    struct helper : public rpc {
      explicit helper(Args&&... a) : rpc(std::forward<Args>(a)...) {}
    };
    return std::make_shared<helper>(std::forward<Args>(args)...);
  }

 private:
  explicit rpc(std::shared_ptr<connection> conn = std::make_shared<default_connection>())
      : conn_(conn), dispatcher_(std::make_shared<detail::msg_dispatcher>(std::move(conn))) {
    dispatcher_->init();
    RPC_CORE_LOGD("rpc: %p", this);
  }

  ~rpc() {
    RPC_CORE_LOGD("~rpc: %p", this);
  };

 public:
  inline std::shared_ptr<connection> get_connection() const {
    return conn_;
  }

  inline void set_timer(detail::msg_dispatcher::timer_impl timer_impl) {
    dispatcher_->set_timer_impl(std::move(timer_impl));
  }

  inline void set_ready(bool ready) {
    is_ready_ = ready;
  }

 public:
  template <typename F, typename std::enable_if<!detail::fp_is_request_response<F>::value, int>::type = 0>
  void subscribe(const cmd_type& cmd, F handle) {
    constexpr bool F_ReturnIsEmpty = std::is_void<typename detail::callable_traits<F>::return_type>::value;
    constexpr bool F_ParamIsEmpty = detail::callable_traits<F>::argc == 0;
    subscribe_helper<F, F_ReturnIsEmpty, F_ParamIsEmpty>()(cmd, std::move(handle), dispatcher_.get());
  }

  template <class F>
  using Scheduler = std::function<void(std::function<typename detail::callable_traits<F>::return_type()>)>;

  template <typename F, typename std::enable_if<detail::fp_is_request_response<F>::value, int>::type = 0>
  void subscribe(const cmd_type& cmd, F handle) {
    static_assert(std::is_void<typename detail::callable_traits<F>::return_type>::value, "should return void");
    subscribe(cmd, std::move(handle), nullptr);
  }

  template <typename F, typename std::enable_if<detail::fp_is_request_response<F>::value, int>::type = 0>
  void subscribe(const cmd_type& cmd, F handle, Scheduler<F> scheduler) {
    static_assert(detail::callable_traits<F>::argc == 1, "should be request_response<>");
    dispatcher_->subscribe_cmd(cmd, [handle = std::move(handle), scheduler = std::move(scheduler)](const detail::msg_wrapper& msg) mutable {
      using request_response = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;
      using request_response_impl = typename request_response::element_type;
      static_assert(detail::is_request_response<request_response>::value, "should be request_response<>");
      using Req = decltype(request_response_impl::req);
      using Rsp = decltype(request_response_impl::rsp_data);
      request_response rr = request_response_impl::create();
      auto r = msg.unpack_as<Req>();
      // notice lifecycle: request_response hold async_helper
      // but async_helper->is_ready will hold rr lifetime for check if it is ready and get data
      // it will be release in msg_dispatcher after check
      auto async_helper = std::make_shared<detail::async_helper>();
      async_helper->is_ready = [rr] {
        return rr->rsp_ready;
      };
      async_helper->get_data = [rr = rr.get()] {
        return std::move(rr->rsp_data);
      };
      if (r.first) {
        rr->req = std::move(r.second);
        rr->rsp = [rr = rr.get(), hp = async_helper](Rsp rsp) mutable {
          if (rr->rsp_ready) {
            RPC_CORE_LOGD("rsp should only call once");
            return;
          }
          rr->rsp_ready = true;
          rr->rsp_data = std::move(rsp);
          if (hp->send_async_response) {  // means after handle()
            hp->send_async_response(serialize(std::move(rr->rsp_data)));
          }
        };
        if (scheduler) {
          scheduler(std::bind(handle, std::move(rr)));
        } else {
          (void)handle(std::move(rr));
        }
      }
      return detail::msg_wrapper::make_rsp_async(msg.seq, std::move(async_helper), r.first);
    });
  }

  inline void unsubscribe(const cmd_type& cmd) {
    dispatcher_->unsubscribe_cmd(cmd);
  }

 public:
  inline request_s create_request();

  inline request_s cmd(cmd_type cmd);

  inline request_s ping(std::string payload = {});

 public:
  inline seq_type make_seq() {
    return seq_++;
  }

  inline void send_request(request const* request);

  inline bool is_ready() const {
    return is_ready_;
  }

 private:
  template <typename F, bool F_ReturnIsEmpty, bool F_ParamIsEmpty>
  struct subscribe_helper;

  template <typename F>
  struct subscribe_helper<F, false, false> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper& msg) mutable {
        using F_Param = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;
        using F_Return = detail::remove_cvref_t<typename detail::callable_traits<F>::return_type>;

        auto r = msg.unpack_as<F_Param>();
        F_Return ret;
        if (r.first) {
          ret = handle(std::move(r.second));
        }
        return detail::msg_wrapper::make_rsp(msg.seq, &ret, r.first);
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, true, false> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper& msg) mutable {
        using F_Param = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;

        auto r = msg.unpack_as<F_Param>();
        if (r.first) {
          handle(std::move(r.second));
        }
        return detail::msg_wrapper::make_rsp<uint8_t>(msg.seq, nullptr, r.first);
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, false, true> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper& msg) mutable {
        using F_Return = typename detail::callable_traits<F>::return_type;

        F_Return ret = handle();
        return detail::msg_wrapper::make_rsp(msg.seq, &ret, true);
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, true, true> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper& msg) mutable {
        handle();
        return detail::msg_wrapper::make_rsp<uint8_t>(msg.seq, nullptr, true);
      });
    }
  };

 private:
  std::shared_ptr<connection> conn_;
  std::shared_ptr<detail::msg_dispatcher> dispatcher_;
  seq_type seq_{0};
  bool is_ready_ = false;
};

using rpc_s = std::shared_ptr<rpc>;
using rpc_w = std::weak_ptr<rpc>;

}  // namespace rpc_core
