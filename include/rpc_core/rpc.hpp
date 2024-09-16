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
  template <typename F>
  void subscribe(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle) {
    constexpr bool F_ReturnIsEmpty = std::is_void<typename detail::callable_traits<F>::return_type>::value;
    constexpr bool F_ParamIsEmpty = detail::callable_traits<F>::argc == 0;
    subscribe_helper<F, F_ReturnIsEmpty, F_ParamIsEmpty>()(cmd, std::move(handle), dispatcher_.get());
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
    void operator()(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
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
    void operator()(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
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
    void operator()(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
        using F_Return = typename detail::callable_traits<F>::return_type;

        F_Return ret = handle();
        return detail::msg_wrapper::make_rsp(msg.seq, &ret, true);
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, true, true> {
    void operator()(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
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

}  // namespace rpc_core
