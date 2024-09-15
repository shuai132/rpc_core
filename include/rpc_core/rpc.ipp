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

void rpc::send_request(request const* request) {
  if (request->need_rsp_) {
    dispatcher_->subscribe_rsp(request->seq_, request->rsp_handle_, request->timeout_cb_, request->timeout_ms_);
  }
  detail::msg_wrapper msg;
  msg.type = static_cast<detail::msg_wrapper::msg_type>(detail::msg_wrapper::command | (request->is_ping_ ? detail::msg_wrapper::ping : 0) |
                                                        (request->need_rsp_ ? detail::msg_wrapper::need_rsp : 0));
  msg.cmd = request->cmd_;
  msg.seq = request->seq_;
  msg.request_payload = &request->payload_;
  RPC_CORE_LOGD("=> seq:%u type:%s %s", msg.seq, (msg.type & detail::msg_wrapper::msg_type::ping) ? "ping" : "cmd", msg.cmd.c_str());
  conn_->send_package_impl(detail::coder::serialize(msg));
}

}  // namespace rpc_core
