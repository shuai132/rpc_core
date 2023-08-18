#pragma once

#include <string>
#include <utility>

#include "../Type.hpp"
#include "copyable.hpp"
#include "log.h"

namespace RpcCore {
namespace detail {

struct MsgWrapper : copyable {  // NOLINT
  enum MsgType : uint8_t {
    COMMAND = 1 << 0,
    RESPONSE = 1 << 1,
    NEED_RSP = 1 << 2,
    PING = 1 << 3,
    PONG = 1 << 4,
  };

  SeqType seq;
  CmdType cmd;
  MsgType type;
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
      RpcCore_LOGE("deserialize error, msg info:%s", dump().c_str());
    }
    return std::make_pair(ok, std::move(message));
  }

  static MsgWrapper MakeCmd(CmdType cmd, SeqType seq, bool isPing, bool needRsp, std::string data) {
    MsgWrapper msg;
    msg.type = static_cast<MsgType>(MsgWrapper::COMMAND | (isPing ? MsgWrapper::PING : 0) | (needRsp ? MsgWrapper::NEED_RSP : 0));
    msg.cmd = std::move(cmd);
    msg.seq = seq;
    msg.data = std::move(data);
    return msg;
  }

  template <typename Message>
  static std::pair<bool, MsgWrapper> MakeRsp(SeqType seq, Message* message = nullptr, bool success = true) {
    static_assert(!std::is_same<void, Message>::value, "");
    MsgWrapper msg;
    msg.type = MsgWrapper::RESPONSE;
    msg.seq = seq;
    if (message != nullptr) {
      msg.data = serialize(*message);
    }
    return std::make_pair(success, std::move(msg));
  }

  template <typename Message>
  static std::pair<bool, MsgWrapper> MakeRsp(SeqType seq, void* message = nullptr, bool success = true) {
    static_assert(std::is_same<void, Message>::value, "");
    MsgWrapper msg;
    msg.type = MsgWrapper::RESPONSE;
    msg.seq = seq;
    return std::make_pair(success, std::move(msg));
  }
};

}  // namespace detail
}  // namespace RpcCore
