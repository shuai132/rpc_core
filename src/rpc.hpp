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
#include "request.hpp"

namespace RPC_CORE_NAMESPACE {

class rpc : detail::noncopyable, public std::enable_shared_from_this<rpc>, public request::rpc_proto {
 public:
  using timeout_cb = detail::msg_dispatcher::timeout_cb;

 public:
  template <typename... Args>
  static std::shared_ptr<rpc> create(Args&&... args) {
    return std::shared_ptr<rpc>(new rpc(std::forward<Args>(args)...), [](rpc* p) {
      delete p;
    });
  }

 private:
  explicit rpc(std::shared_ptr<connection> conn = std::make_shared<connection>()) : conn_(conn), dispatcher_(std::move(conn)) {}

  ~rpc() override {
    RPC_CORE_LOGD("~rpc");
  };

 public:
  inline std::shared_ptr<connection> get_connection() const {
    return conn_;
  }

  inline void set_timer(detail::msg_dispatcher::timer_impl timer_impl) {
    dispatcher_.set_timer_impl(std::move(timer_impl));
  }

 public:
  template <typename F>
  void subscribe(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle) {
    constexpr bool F_ReturnIsEmpty = std::is_void<typename detail::callable_traits<F>::return_type>::value;
    constexpr bool F_ParamIsEmpty = detail::callable_traits<F>::argc == 0;
    subscribe_helper<F, F_ReturnIsEmpty, F_ParamIsEmpty>()(cmd, std::move(handle), &dispatcher_);
  }

  inline void unsubscribe(const cmd_type& cmd) {
    dispatcher_.unsubscribeCmd(cmd);
  }

 public:
  inline request_s create_request() {
    return request::create(shared_from_this());
  }

  inline request_s cmd(cmd_type cmd) {
    return create_request()->cmd(std::move(cmd));
  }

  inline request_s ping(std::string payload = "") {
    return create_request()->ping()->msg(std::move(payload));
  }

 public:
  seq_type make_seq() override {
    return seq_++;
  }

  void send_request(const request_s& request) override {
    const bool need_rsp = request->need_rsp();
    if (need_rsp) {
      dispatcher_.subscribeRsp(request->seq(), request->rsp_handle(), request->timeoutCb_, request->timeout_ms(), request->is_ping_);
    }
    auto msg = detail::msg_wrapper::make_cmd(request->cmd(), request->seq(), request->is_ping_, need_rsp, request->payload());
    conn_->send_package_impl(detail::coder::serialize(msg));
  }

 private:
  template <typename F, bool F_ReturnIsEmpty, bool F_ParamIsEmpty>
  struct subscribe_helper;

  template <typename F>
  struct subscribe_helper<F, false, false> {
    void operator()(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribeCmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
        using F_Param = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;
        using F_Return = detail::remove_cvref_t<typename detail::callable_traits<F>::return_type>;

        auto r = msg.unpackAs<F_Param>();
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
      dispatcher->subscribeCmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
        using F_Param = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;

        auto r = msg.unpackAs<F_Param>();
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
      dispatcher->subscribeCmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
        using F_Return = typename detail::callable_traits<F>::return_type;

        F_Return ret = handle();
        return detail::msg_wrapper::make_rsp(msg.seq, &ret, true);
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, true, true> {
    void operator()(const cmd_type& cmd, RPC_CORE_MOVE_PARAM(F) handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribeCmd(cmd, [RPC_CORE_MOVE_LAMBDA(handle)](const detail::msg_wrapper& msg) {
        handle();
        return detail::msg_wrapper::make_rsp<uint8_t>(msg.seq, nullptr, true);
      });
    }
  };

 private:
  std::shared_ptr<connection> conn_;
  detail::msg_dispatcher dispatcher_;
  seq_type seq_{0};
};

}  // namespace RPC_CORE_NAMESPACE
