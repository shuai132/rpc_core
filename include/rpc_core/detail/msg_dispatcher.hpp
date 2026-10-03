#pragma once

#include <map>
#include <memory>
#include <utility>

#include "../connection.hpp"
#include "../result.hpp"
#include "coder.hpp"
#include "log.h"
#include "noncopyable.hpp"

namespace rpc_core {
namespace detail {

class msg_dispatcher : public std::enable_shared_from_this<msg_dispatcher>, noncopyable {
 public:
  using reply_handle = std::function<result<void>(std::string)>;
  using cmd_handle = std::function<void(const msg_wrapper&, reply_handle)>;
  using rsp_handle = std::function<bool(msg_wrapper)>;

  using timeout_cb = std::function<void()>;
  using timer_impl = std::function<void(uint32_t ms, timeout_cb)>;

 public:
  explicit msg_dispatcher(std::shared_ptr<connection> conn) : conn_(std::move(conn)) {}

  ~msg_dispatcher() {
    for (auto& pending : rsp_handle_map_) {
      if (pending.second.expired) pending.second.expired();
    }
  }

  void init() {
    conn_->on_recv_package = ([self = std::weak_ptr<msg_dispatcher>(shared_from_this())](const std::string& payload) {
      auto self_lock = self.lock();
      if (!self_lock) {
        RPC_CORE_LOGD("msg_dispatcher expired");
        return;
      }
      auto decoded = coder::deserialize(payload);
      if (decoded.first) {
        self_lock->dispatch(std::move(decoded.second));
      } else {
        RPC_CORE_LOGE("payload deserialize error");
      }
    });
  }

 private:
  result<void> send_response(const msg_wrapper& msg, uint32_t generation) {
    if (generation != session_generation_) return {finally_t::session_reset};
    if (responses_paused_) return {finally_t::rpc_not_ready};
    auto payload = coder::serialize(msg);
    if (!payload.first) {
      RPC_CORE_LOGE("response serialization failed");
      return {finally_t::rsp_serialize_error};
    }
    return {conn_->send_package(std::move(payload.second)) ? finally_t::normal : finally_t::rpc_not_ready};
  }

  void dispatch(msg_wrapper msg) {
    const auto generation = session_generation_;
    switch (msg.type & (msg_wrapper::command | msg_wrapper::response)) {
      case msg_wrapper::command: {
        // ping
        const bool is_ping = msg.type & msg_wrapper::ping;
        if (is_ping) {
          RPC_CORE_LOGD("<= seq:%u type:ping", msg.seq);
          msg.type = static_cast<msg_wrapper::msg_type>(msg_wrapper::response | msg_wrapper::pong);
          RPC_CORE_LOGD("=> seq:%u type:pong", msg.seq);
          send_response(msg, generation);
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
            send_response(rsp, generation);
          }
          return;
        }
        // Keep the same callable and its state alive even if it unsubscribes itself.
        auto fn = it->second;
        const bool need_rsp = msg.type & msg_wrapper::need_rsp;
        auto reply = [weak = std::weak_ptr<msg_dispatcher>(shared_from_this()), seq = msg.seq, generation, need_rsp](std::string data) -> result<void> {
          if (!need_rsp) return {finally_t::no_need_rsp};
          auto self = weak.lock();
          if (!self) return {finally_t::rpc_expired};
          msg_wrapper response;
          response.seq = seq;
          response.type = msg_wrapper::response;
          response.data = std::move(data);
          return self->send_response(response, generation);
        };
        (*fn)(msg, std::move(reply));
      } break;

      case msg_wrapper::response: {
        // pong or response
        RPC_CORE_LOGD("<= seq:%u type:%s", msg.seq, (msg.type & detail::msg_wrapper::msg_type::pong) ? "pong" : "rsp");
        auto it = rsp_handle_map_.find(msg.seq);
        if (it == rsp_handle_map_.cend() || !it->second.handle) {
          RPC_CORE_LOGD("no rsp for seq:%u", msg.seq);
          break;
        }
        bool handled = false;
        {
          // Keep this call visible to reset_session() while custom decoding runs.
          // Cleanup also runs if a user callback throws, and cannot erase a replacement.
          struct response_scope {
            msg_dispatcher& owner;
            seq_type seq;
            uint64_t registration;
            ~response_scope() {
              auto pending = owner.rsp_handle_map_.find(seq);
              if (pending != owner.rsp_handle_map_.end() && pending->second.registration == registration) {
                owner.rsp_handle_map_.erase(pending);
              }
            }
          } scope{*this, msg.seq, it->second.registration};
          auto cb = std::move(it->second.handle);
          it->second.handle = nullptr;
          handled = cb(std::move(msg));
        }
        if (handled) {
          RPC_CORE_LOGV("rsp_handle_map_.size=%zu", rsp_handle_map_.size());
        } else {
          RPC_CORE_LOGE("may deserialize error");
        }
      } break;

      default:
        RPC_CORE_LOGE("unknown type");
    }
  }

 public:
  seq_type make_seq(seq_type& next) const {
    // A long-lived pending request may still own an ID after the counter wraps.
    while (rsp_handle_map_.find(next) != rsp_handle_map_.end()) ++next;
    return next++;
  }

  void set_ready(bool ready) {
    responses_paused_ = !ready;
  }

  void reset_session() {
    ++session_generation_;
    // Detach all old registrations before callbacks can register new requests.
    decltype(rsp_handle_map_) previous;
    previous.swap(rsp_handle_map_);
    for (auto& pending : previous) {
      if (pending.second.reset) pending.second.reset();
    }
  }

  inline void subscribe_cmd(const cmd_type& cmd, cmd_handle handle) {
    RPC_CORE_LOGD("subscribe cmd:%s", cmd.c_str());
    cmd_handle_map_[cmd] = std::make_shared<cmd_handle>(std::move(handle));
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

  void subscribe_rsp(seq_type seq, rsp_handle handle, timeout_cb timeout_cb, uint32_t timeout_ms,
                     std::function<void()> expired = nullptr, std::function<void()> reset = nullptr) {
    RPC_CORE_LOGD("subscribe_rsp seq:%u", seq);
    if (handle == nullptr) return;

    const auto registration = ++registration_id_;
    rsp_handle_map_[seq] = {std::move(handle), std::move(expired), std::move(reset), registration};
    // Keep the callable itself alive across synchronous completion and reentry.
    auto timer = timer_impl_;
    if (!timer) {
      RPC_CORE_LOGW("no timeout will cause memory leak!");
      return;
    }

    (*timer)(timeout_ms, [self = std::weak_ptr<msg_dispatcher>(shared_from_this()), seq, registration, timeout_cb = std::move(timeout_cb)] {
      auto self_lock = self.lock();
      if (!self_lock) {
        RPC_CORE_LOGD("seq:%u timeout after destroy", seq);
        return;
      }
      auto it = self_lock->rsp_handle_map_.find(seq);
      if (it != self_lock->rsp_handle_map_.cend() && it->second.registration == registration && it->second.handle) {
        // Stop accepting this attempt's response, but keep it visible to reset_session().
        it->second.handle = nullptr;
        if (timeout_cb) {
          timeout_cb();
        }
        it = self_lock->rsp_handle_map_.find(seq);
        if (it != self_lock->rsp_handle_map_.cend() && it->second.registration == registration) {
          self_lock->rsp_handle_map_.erase(it);
        }
        RPC_CORE_LOGV("Timeout seq=%d, rsp_handle_map_.size=%zu", seq, self_lock->rsp_handle_map_.size());
      }
    });
  }

  void unsubscribe_rsp(seq_type seq) {
    auto it = rsp_handle_map_.find(seq);
    if (it != rsp_handle_map_.cend()) {
      RPC_CORE_LOGD("erase rsp seq:%u", seq);
      rsp_handle_map_.erase(it);
    }
  }

  inline void set_timer_impl(timer_impl timer_impl) {
    timer_impl_ = timer_impl ? std::make_shared<msg_dispatcher::timer_impl>(std::move(timer_impl)) : nullptr;
  }

 private:
  std::shared_ptr<connection> conn_;
  std::map<cmd_type, std::shared_ptr<cmd_handle>> cmd_handle_map_;
  struct pending_response {
    rsp_handle handle;
    timeout_cb expired;
    timeout_cb reset;
    uint64_t registration;
  };
  std::map<seq_type, pending_response> rsp_handle_map_;
  std::shared_ptr<timer_impl> timer_impl_;
  uint32_t session_generation_ = 0;
  uint64_t registration_id_ = 0;
  // Incoming RPCs historically work before set_ready() is first called.
  bool responses_paused_ = false;
};

}  // namespace detail
}  // namespace rpc_core
