#pragma once

#include "request.hpp"
#include "rpc.hpp"

namespace rpc_core {

void request::call(const rpc_s& rpc) {
  waiting_rsp_ = true;

  if (canceled_) {
    on_finish(finally_t::canceled);
    return;
  }

  self_keeper_ = shared_from_this();
  if (rpc) {
    rpc_ = rpc;
  } else if (rpc_.expired()) {
    on_finish(finally_t::rpc_expired);
    return;
  }

  auto r = rpc_.lock();
  if (!r->is_ready()) {
    on_finish(finally_t::rpc_not_ready);
    return;
  }
  seq_ = r->make_seq();
  r->send_request(this);
  if (!need_rsp_) {
    on_finish(finally_t::no_need_rsp);
  }
}

request_s request::add_to(dispose& dispose) {
  auto self = shared_from_this();
  dispose.add(self);
  return self;
}

}  // namespace rpc_core
