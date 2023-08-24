#pragma once

#include <string>
#include <utility>

#include "../type.hpp"
#include "copyable.hpp"
#include "log.h"

namespace RPC_CORE_NAMESPACE {
namespace detail {

struct msg_wrapper : copyable {  // NOLINT
  enum msg_type : uint8_t {
    COMMAND = 1 << 0,
    RESPONSE = 1 << 1,
    NEED_RSP = 1 << 2,
    PING = 1 << 3,
    PONG = 1 << 4,
  };

  seq_type seq;
  cmd_type cmd;
  msg_type type;
  std::string data;

  std::string dump() const {
    char tmp[100];
    snprintf(tmp, 100, "seq:%u, type:%u, cmd:%s", seq, type, cmd.c_str());
    return tmp;
  }

  template <typename T>
  std::pair<bool, T> unpackAs() const {
    T message;
    bool ok = deserialize(data, message);
    if (not ok) {
      RPC_CORE_LOGE("deserialize error, msg info:%s", dump().c_str());
    }
    return std::make_pair(ok, std::move(message));
  }

  static msg_wrapper make_cmd(cmd_type cmd, seq_type seq, bool is_ping, bool need_rsp, std::string data) {
    msg_wrapper msg;
    msg.type = static_cast<msg_type>(msg_wrapper::COMMAND | (is_ping ? msg_wrapper::PING : 0) | (need_rsp ? msg_wrapper::NEED_RSP : 0));
    msg.cmd = std::move(cmd);
    msg.seq = seq;
    msg.data = std::move(data);
    return msg;
  }

  template <typename T>
  static std::pair<bool, msg_wrapper> make_rsp(seq_type seq, T* t = nullptr, bool success = true) {
    msg_wrapper msg;
    msg.type = msg_wrapper::RESPONSE;
    msg.seq = seq;
    if (success && t != nullptr) {
      msg.data = serialize(*t);
    }
    return std::make_pair(success, std::move(msg));
  }
};

}  // namespace detail
}  // namespace RPC_CORE_NAMESPACE
