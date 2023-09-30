#pragma once

#include <map>
#include <memory>
#include <utility>

#include "../connection.hpp"
#include "coder.hpp"
#include "log.h"
#include "noncopyable.hpp"

namespace rpc_core {
namespace detail {

class msg_dispatcher : noncopyable {
 public:
  using cmd_handle = std::function<std::pair<bool, msg_wrapper>(msg_wrapper)>;
  using rsp_handle = std::function<bool(msg_wrapper)>;

  using timeout_cb = std::function<void()>;
  using timer_impl = std::function<void(uint32_t ms, timeout_cb)>;

 public:
  explicit msg_dispatcher(std::shared_ptr<connection> conn) : conn_(std::move(conn)) {
    auto alive = std::weak_ptr<void>(is_alive_);
    conn_->on_recv_package = ([this, RPC_CORE_MOVE_LAMBDA(alive)](const std::string& payload) {
      if (alive.expired()) {
        RPC_CORE_LOGD("msg_dispatcher expired");
        return;
      }
      bool success;
      auto msg = coder::deserialize(payload, success);
      if (success) {
        this->dispatch(std::move(msg));
      } else {
        RPC_CORE_LOGE("payload can not be parsed, msg info");
      }
    });
  }

 private:
  void dispatch(msg_wrapper msg) {
    switch (msg.type & (msg_wrapper::command | msg_wrapper::response)) {
      case msg_wrapper::command: {
        // ping
        const bool is_ping = msg.type & msg_wrapper::ping;
        if (is_ping) {
          RPC_CORE_LOGD("<= seq:%u, type:%s", msg.seq, "ping");
          msg.type = static_cast<msg_wrapper::msg_type>(msg_wrapper::response | msg_wrapper::pong);
          RPC_CORE_LOGD("=> seq:%u, type:%s", msg.seq, "pong");
          conn_->send_package_impl(coder::serialize(msg));
          return;
        }

        // command
        const auto& cmd = msg.cmd;
        auto it = cmd_handle_map_.find(cmd);
        if (it == cmd_handle_map_.cend()) {
          RPC_CORE_LOGD("not register cmd for: %s", cmd.c_str());
          return;
        }
        const auto& fn = it->second;
        const bool need_rsp = msg.type & msg_wrapper::need_rsp;
        auto resp = fn(std::move(msg));
        if (need_rsp && resp.first) {
          conn_->send_package_impl(coder::serialize(resp.second));
        }
      } break;

      case msg_wrapper::response: {
        // pong or response
        const bool isPong = msg.type & msg_wrapper::pong;
        const auto handleMap = isPong ? &pong_handle_map_ : &rsp_handle_map_;

        RPC_CORE_LOGD("<= seq:%u, type:%s", msg.seq, (msg.type & detail::msg_wrapper::msg_type::pong) ? "pong" : "rsp");
        auto it = handleMap->find(msg.seq);
        if (it == handleMap->cend()) {
          RPC_CORE_LOGD("not register callback for response");
          break;
        }
        const auto& cb = it->second;
        if (not cb) {
          RPC_CORE_LOGE("rsp handle can not be null");
          return;
        }
        if (cb(std::move(msg))) {
          handleMap->erase(it);
          RPC_CORE_LOGV("handleMap->size=%zu", handleMap->size());
        } else {
          RPC_CORE_LOGE("may deserialize error");
        }
      } break;

      default:
        RPC_CORE_LOGE("unknown type");
    }
  }

 public:
  inline void subscribe_cmd(const cmd_type& cmd, cmd_handle handle) {
    RPC_CORE_LOGD("subscribe_cmd cmd:%s", cmd.c_str());
    cmd_handle_map_[cmd] = std::move(handle);
  }

  void unsubscribe_cmd(const cmd_type& cmd) {
    auto it = cmd_handle_map_.find(cmd);
    if (it != cmd_handle_map_.cend()) {
      RPC_CORE_LOGD("erase cmd:%s", cmd.c_str());
      cmd_handle_map_.erase(it);
    } else {
      RPC_CORE_LOGD("not register cmd for: %s", cmd.c_str());
    }
  }

  void subscribe_rsp(seq_type seq, rsp_handle handle, RPC_CORE_MOVE_PARAM(timeout_cb) timeout_cb, uint32_t timeout_ms, bool is_ping) {
    RPC_CORE_LOGD("subscribe_rsp seq:%u", seq);
    if (handle == nullptr) return;
    const auto handleMap = is_ping ? &pong_handle_map_ : &rsp_handle_map_;

    (*handleMap)[seq] = std::move(handle);

    if (timer_impl_ == nullptr) {
      RPC_CORE_LOGW("no timeout will cause memory leak!");
      return;
    }

    auto alive = std::weak_ptr<void>(is_alive_);
    timer_impl_(timeout_ms, [handleMap, seq, RPC_CORE_MOVE_LAMBDA(timeout_cb), RPC_CORE_MOVE_LAMBDA(alive)] {
      if (alive.expired()) {
        RPC_CORE_LOGD("seq:%u timeout after destroy", seq);
        return;
      }
      auto it = handleMap->find(seq);
      if (it != handleMap->cend()) {
        if (timeout_cb) {
          timeout_cb();
        }
        handleMap->erase(seq);
        RPC_CORE_LOGV("Timeout seq=%d, handleMap.size=%zu", seq, handleMap->size());
      }
    });
  }

  inline void set_timer_impl(timer_impl timer_impl) {
    timer_impl_ = std::move(timer_impl);
  }

 private:
  std::shared_ptr<connection> conn_;
  std::map<cmd_type, cmd_handle> cmd_handle_map_;
  std::map<seq_type, rsp_handle> rsp_handle_map_;
  std::map<seq_type, rsp_handle> pong_handle_map_;
  timer_impl timer_impl_;
  std::shared_ptr<void> is_alive_ = std::make_shared<uint8_t>();
};

}  // namespace detail
}  // namespace rpc_core
