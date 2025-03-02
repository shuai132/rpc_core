#pragma once

#include "request.hpp"
#include "rpc.hpp"

namespace rpc_core {

request_s rpc::create_request() {
  return request::create(shared_from_this());
}

request_s rpc::cmd(cmd_type cmd) {
  return create_request()->cmd(std::move(cmd));
}

request_s rpc::ping(std::string payload) {
  return create_request()->ping()->msg(std::move(payload));
}

template <typename Msg>
inline void rpc::call(cmd_type cmd, Msg&& message) {
  this->cmd(std::move(cmd))->msg(std::forward<Msg>(message))->call();
}

template <typename Msg, typename Rsp>
inline void rpc::call(cmd_type cmd, Msg&& message, Rsp&& rsp) {
  this->cmd(std::move(cmd))->msg(std::forward<Msg>(message))->rsp(std::forward<Rsp>(rsp))->call();
}

#ifdef RPC_CORE_FEATURE_CO_ASIO
template <typename R>
inline asio::awaitable<result<R>> rpc::co_call(cmd_type cmd) {
  return this->cmd(std::move(cmd))->co_call<R>();
}

template <typename R, typename Msg>
inline asio::awaitable<result<R>> rpc::co_call(cmd_type cmd, Msg&& message) {
  return this->cmd(std::move(cmd))->msg(std::forward<Msg>(message))->template co_call<R>();
}
#endif

result<void> rpc::send_request(request const* request) {
  auto options = request->active_;
  if (!options) return {finally_t::canceled};
  detail::msg_wrapper msg;
  msg.type = static_cast<detail::msg_wrapper::msg_type>(detail::msg_wrapper::command | (options->is_ping ? detail::msg_wrapper::ping : 0) |
                                                        (options->need_rsp ? detail::msg_wrapper::need_rsp : 0));
  msg.cmd = options->cmd;
  msg.seq = request->seq_;
  msg.request_payload = &options->payload;
  auto payload = detail::coder::serialize(msg);
  if (!payload.first) {
    RPC_CORE_LOGE("request serialization failed");
    return {finally_t::req_serialize_error};
  }
  RPC_CORE_LOGD("=> seq:%u type:%s %s", msg.seq, (msg.type & detail::msg_wrapper::msg_type::ping) ? "ping" : "cmd", msg.cmd.c_str());
  if (options->need_rsp) {
    auto weak = request::request_w(request->self_keeper_);
    const auto call_id = request->call_id_;
    const auto seq = request->seq_;
    auto handle = options->rsp_handle;
    // Guard both the logical call and the individual retry attempt.
    dispatcher_->subscribe_rsp(seq, handle ? detail::msg_dispatcher::rsp_handle(
        [weak, call_id, seq, handle = std::move(handle)](detail::msg_wrapper response) mutable {
          auto pending = weak.lock();
          if (!pending || !pending->matches_attempt(call_id, seq)) return true;
          return handle(std::move(response));
        }) : nullptr,
        [weak, call_id, seq] {
          auto pending = weak.lock();
          if (pending && pending->matches_attempt(call_id, seq)) pending->on_timeout();
        }, options->timeout_ms,
        [weak, call_id, seq] {
          auto pending = weak.lock();
          if (pending && pending->matches_attempt(call_id, seq)) pending->on_finish(finally_t::rpc_expired);
        });
  }
  conn_->send_package_impl(std::move(payload.second));
  return {finally_t::normal};
}

}  // namespace rpc_core
