#pragma once

#include <string>
#include <utility>

#include "../type.hpp"
#include "copyable.hpp"
#include "log.h"
#include "msg_wrapper.hpp"

namespace rpc_core {
namespace detail {

struct async_helper : noncopyable {
  std::function<bool()> is_ready;
  std::function<std::string()> get_data;
  std::function<void(std::string)> send_async_response;
};
using async_helper_s = std::shared_ptr<async_helper>;

struct msg_wrapper : copyable {  // NOLINT
  enum msg_type : uint8_t {
    command = 1 << 0,
    response = 1 << 1,
    need_rsp = 1 << 2,
    ping = 1 << 3,
    pong = 1 << 4,
    no_such_cmd = 1 << 5,
  };

  enum class response_state : uint8_t {
    response_sync = 1 << 0,
    response_async = 1 << 1,
    serialize_error = 1 << 2,
  };

  seq_type seq;
  cmd_type cmd;
  msg_type type;
  std::string data;
  std::string const* request_payload = nullptr;

  response_state response_state;
  async_helper_s async_helper;

  std::string dump() const {
    char tmp[100];
    snprintf(tmp, 100, "seq:%u, type:%u, cmd:%s", seq, type, cmd.c_str());
    return tmp;
  }

  template <typename T>
  std::pair<bool, T> unpack_as() const {
    T message;
    bool ok = deserialize(data, message);
    if (!ok) {
      RPC_CORE_LOGE("deserialize error, msg info:%s", dump().c_str());
    }
    return std::make_pair(ok, std::move(message));
  }

  template <typename T>
  static std::pair<bool, msg_wrapper> make_rsp(seq_type seq, T* t = nullptr, bool success = true) {
    msg_wrapper msg;
    msg.type = msg_wrapper::response;
    msg.seq = seq;
    if (success && t != nullptr) {
      msg.data = serialize(*t);
    }
    msg.response_state = success ? response_state::response_sync : response_state::serialize_error;
    return std::make_pair(success, std::move(msg));
  }

  static std::pair<bool, msg_wrapper> make_rsp_async(seq_type seq, detail::async_helper_s async_helper, bool success = true) {
    msg_wrapper msg;
    msg.type = msg_wrapper::response;
    msg.seq = seq;
    msg.async_helper = std::move(async_helper);
    msg.response_state = success ? response_state::response_async : response_state::serialize_error;
    return std::make_pair(success, std::move(msg));
  }
};

}  // namespace detail
}  // namespace rpc_core
