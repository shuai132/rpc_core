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

class msg_dispatcher : public std::enable_shared_from_this<msg_dispatcher>, noncopyable {
 public:
  using cmd_handle = std::function<std::pair<bool, msg_wrapper>(msg_wrapper)>;
  using rsp_handle = std::function<bool(msg_wrapper)>;

  using timeout_cb = std::function<void()>;
  using timer_impl = std::function<void(uint32_t ms, timeout_cb)>;

 public:
  explicit msg_dispatcher(std::shared_ptr<connection> conn) : conn_(std::move(conn)) {}

  void init() {
    auto self = std::weak_ptr<msg_dispatcher>(shared_from_this());
    conn_->on_recv_package = ([RPC_CORE_MOVE_LAMBDA(self)](const std::string& payload) {
      auto self_lock = self.lock();
      if (!self_lock) {
        RPC_CORE_LOGD("msg_dispatcher expired");
        return;
      }
      bool success;
      auto msg = coder::deserialize(payload, success);
      if (success) {
        self_lock->dispatch(std::move(msg));
      } else {
        RPC_CORE_LOGE("payload deserialize error");
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
          RPC_CORE_LOGD("<= seq:%u type:ping", msg.seq);
          msg.type = static_cast<msg_wrapper::msg_type>(msg_wrapper::response | msg_wrapper::pong);
          RPC_CORE_LOGD("=> seq:%u type:pong", msg.seq);
          conn_->send_package_impl(coder::serialize(msg));
          return;
        }

        // command
        RPC_CORE_LOGD("<= seq:%u cmd:%s", msg.seq, msg.cmd.c_str());
        const auto& cmd = msg.cmd;
        auto it = cmd_handle_map_.find(cmd);
        if (it == cmd_handle_map_.cend()) {
          RPC_CORE_LOGD("not subscribe cmd for: %s", cmd.c_str());
          const bool need_rsp = msg.type & msg_wrapper::need_rsp;
          if (need_rsp) {
            RPC_CORE_LOGD("=> seq:%u type:rsp", msg.seq);
            msg_wrapper rsp;
            rsp.seq = msg.seq;
            rsp.type = static_cast<msg_wrapper::msg_type>(msg_wrapper::msg_type::response | msg_wrapper::msg_type::no_such_cmd);
            conn_->send_package_impl(coder::serialize(rsp));
          }
          return;
        }
        const auto& fn = it->second;
        const bool need_rsp = msg.type & msg_wrapper::need_rsp;
        auto resp = fn(std::move(msg));
        if (need_rsp && resp.first) {
          RPC_CORE_LOGD("=> seq:%u type:rsp", resp.second.seq);
          conn_->send_package_impl(coder::serialize(resp.second));
        }
      } break;

      case msg_wrapper::response: {
        // pong or response
        RPC_CORE_LOGD("<= seq:%u type:%s", msg.seq, (msg.type & detail::msg_wrapper::msg_type::pong) ? "pong" : "rsp");
        auto it = rsp_handle_map_.find(msg.seq);
        if (it == rsp_handle_map_.cend()) {
          RPC_CORE_LOGD("no rsp for seq:%u", msg.seq);
          break;
        }
        const auto& cb = it->second;
        if (not cb) {
          RPC_CORE_LOGE("rsp can not be null");
          return;
        }
        if (cb(std::move(msg))) {
          RPC_CORE_LOGV("rsp_handle_map_.size=%zu", rsp_handle_map_.size());
        } else {
          RPC_CORE_LOGE("may deserialize error");
        }
        rsp_handle_map_.erase(it);
      } break;

      default:
        RPC_CORE_LOGE("unknown type");
    }
  }

 public:
  inline void subscribe_cmd(const cmd_type& cmd, cmd_handle handle) {
    RPC_CORE_LOGD("subscribe cmd:%s", cmd.c_str());
    cmd_handle_map_[cmd] = std::move(handle);
  }

  void unsubscribe_cmd(const cmd_type& cmd) {
    auto it = cmd_handle_map_.find(cmd);
    if (it != cmd_handle_map_.cend()) {
      RPC_CORE_LOGD("erase cmd:%s", cmd.c_str());
      cmd_handle_map_.erase(it);
    } else {
      RPC_CORE_LOGD("not subscribe cmd for: %s", cmd.c_str());
    }
  }

  void subscribe_rsp(seq_type seq, rsp_handle handle, RPC_CORE_MOVE_PARAM(timeout_cb) timeout_cb, uint32_t timeout_ms) {
    RPC_CORE_LOGD("subscribe_rsp seq:%u", seq);
    if (handle == nullptr) return;

    if (timer_impl_ == nullptr) {
      RPC_CORE_LOGW("no timeout will cause memory leak!");
      return;
    }

    rsp_handle_map_[seq] = std::move(handle);
    auto self = std::weak_ptr<msg_dispatcher>(shared_from_this());
    timer_impl_(timeout_ms, [RPC_CORE_MOVE_LAMBDA(self), seq, RPC_CORE_MOVE_LAMBDA(timeout_cb)] {
      auto self_lock = self.lock();
      if (!self_lock) {
        RPC_CORE_LOGD("seq:%u timeout after destroy", seq);
        return;
      }
      auto it = self_lock->rsp_handle_map_.find(seq);
      if (it != self_lock->rsp_handle_map_.cend()) {
        if (timeout_cb) {
          timeout_cb();
        }
        self_lock->rsp_handle_map_.erase(seq);
        RPC_CORE_LOGV("Timeout seq=%d, rsp_handle_map_.size=%zu", seq, this->rsp_handle_map_.size());
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
  timer_impl timer_impl_;
};

}  // namespace detail
}  // namespace rpc_core
