#pragma once

#include <memory>
#include <utility>

// config
#include "config.hpp"

#ifdef RPC_CORE_FEATURE_CO_ASIO
#include <asio.hpp>
#endif

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
  friend class request;

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
    dispatcher_->set_ready(ready);
  }

  // Start a new logical session. Ordinary reconnects only change set_ready().
  // Keep subscriptions, readiness and the sequence counter.
  inline void reset_session() {
    auto keeper = shared_from_this();
    dispatcher_->reset_session();
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
    dispatcher_->subscribe_cmd(cmd, [handle = std::make_shared<F>(std::move(handle)), scheduler = std::move(scheduler)](const detail::msg_wrapper& msg, detail::msg_dispatcher::reply_handle reply) mutable {
      using request_response = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;
      using request_response_impl = typename request_response::element_type;
      static_assert(detail::is_request_response<request_response>::value, "should be request_response<>");
      using Req = decltype(request_response_impl::req);
      using Rsp = typename request_response_impl::RspType;
      auto r = msg.unpack_as<Req>();
      if (!r.first) return;
      auto rr = request_response_impl::create();
      rr->req = std::move(r.second);
      rr->rsp = [weak = rr->weak_ptr(), reply = std::move(reply), state = std::make_shared<reply_state>(reply_state::idle)](Rsp rsp) -> result<void> {
        // Copies share state even after request_response expires. Keep captures
        // alive locally: sending may invoke callbacks that replace this function.
        auto shared_state = state;
        if (*shared_state != reply_state::idle) return {finally_t::busy};
        auto owner = weak.lock();
        auto send = reply;
        *shared_state = reply_state::sending;
        // Serialization can reenter this reply; exceptions must also release it.
        struct reply_scope {
          reply_state& state;
          ~reply_scope() { if (state == reply_state::sending) state = reply_state::idle; }
        } scope{*shared_state};
        auto data = serialize(std::move(rsp));
        auto status = send(std::move(data));
        *shared_state = status ? reply_state::sent : reply_state::idle;
        if (owner) owner->rsp_ready = (*shared_state == reply_state::sent);
        return status;
      };
      if (scheduler) {
        // Share subscription state and keep it alive after unsubscribe. Keep rr
        // in the task as well, so coroutine handlers may borrow it by const ref.
        scheduler([handle, rr = std::move(rr)]() mutable -> typename detail::callable_traits<F>::return_type {
          return (*handle)(rr);
        });
      } else {
        (void)(*handle)(std::move(rr));
      }
    });
  }

  inline void unsubscribe(const cmd_type& cmd) {
    dispatcher_->unsubscribe_cmd(cmd);
  }

 public:
  inline request_s create_request();

  inline request_s cmd(cmd_type cmd);

  inline request_s ping(std::string payload = {});

  template <typename Msg>
  inline void call(cmd_type cmd, Msg&& message);

  template <typename Msg, typename Rsp>
  inline void call(cmd_type cmd, Msg&& message, Rsp&& rsp);

#ifdef RPC_CORE_FEATURE_CO_ASIO
  template <typename R = void>
  inline asio::awaitable<result<R>> co_call(cmd_type cmd);

  template <typename R = void, typename Msg>
  inline asio::awaitable<result<R>> co_call(cmd_type cmd, Msg&& message);
#endif

#ifdef RPC_CORE_FEATURE_CO_CUSTOM
  template <typename R = void>
  inline RPC_CORE_FEATURE_CO_CUSTOM_R RPC_CORE_FEATURE_CO_CUSTOM(cmd_type cmd);

  template <typename R = void, typename Msg>
  inline RPC_CORE_FEATURE_CO_CUSTOM_R RPC_CORE_FEATURE_CO_CUSTOM(cmd_type cmd, Msg&& message);
#endif

 public:
  inline seq_type make_seq() {
    return dispatcher_->make_seq(seq_);
  }

  inline result<void> send_request(request const* request);

  inline bool is_ready() const {
    return is_ready_;
  }

 private:
  inline void unsubscribe_rsp(seq_type seq) {
    dispatcher_->unsubscribe_rsp(seq);
  }

 private:
  enum class reply_state { idle, sending, sent };

  template <typename F, bool F_ReturnIsEmpty, bool F_ParamIsEmpty>
  struct subscribe_helper;

  template <typename F>
  struct subscribe_helper<F, false, false> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper& msg, detail::msg_dispatcher::reply_handle reply) mutable {
        using F_Param = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;

        auto r = msg.unpack_as<F_Param>();
        if (r.first) {
          auto response = handle(std::move(r.second));
          reply(serialize(response));
        }
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, true, false> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper& msg, detail::msg_dispatcher::reply_handle reply) mutable {
        using F_Param = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;

        auto r = msg.unpack_as<F_Param>();
        if (r.first) {
          handle(std::move(r.second));
          reply({});
        }
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, false, true> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper&, detail::msg_dispatcher::reply_handle reply) mutable {
        auto response = handle();
        reply(serialize(response));
      });
    }
  };

  template <typename F>
  struct subscribe_helper<F, true, true> {
    void operator()(const cmd_type& cmd, F handle, detail::msg_dispatcher* dispatcher) {
      dispatcher->subscribe_cmd(cmd, [handle = std::move(handle)](const detail::msg_wrapper&, detail::msg_dispatcher::reply_handle reply) mutable {
        handle();
        reply({});
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
