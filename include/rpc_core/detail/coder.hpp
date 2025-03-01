#pragma once

#ifdef RPC_CORE_FEATURE_CODER_VARINT
#include "coder_varint.hpp"
#else
#include <cstring>
#include <limits>
#include <utility>

#include "msg_wrapper.hpp"

namespace rpc_core {
namespace detail {

class coder {
 public:
  static std::pair<bool, std::string> serialize(const msg_wrapper& msg) {
    if (msg.cmd.size() > (std::numeric_limits<uint16_t>::max)()) {
      return {false, {}};
    }
    std::string payload;
    payload.reserve(PayloadMinLen + msg.cmd.size() + msg.data.size());
    payload.append((char*)&msg.seq, 4);
    auto cmd_len = (uint16_t)msg.cmd.length();
    payload.append((char*)&cmd_len, 2);
    payload.append((char*)msg.cmd.data(), cmd_len);
    payload.append((char*)&msg.type, 1);
    if (msg.request_payload) {
      payload.append(*msg.request_payload);
    } else {
      payload.append(msg.data);
    }
    return {true, std::move(payload)};
  }

  static std::pair<bool, msg_wrapper> deserialize(const std::string& payload) {
    msg_wrapper msg;
    if (payload.size() < PayloadMinLen) {
      return {false, {}};
    }
    const char* p = payload.data();
    const char* pend = payload.data() + payload.size();
    std::memcpy(&msg.seq, p, 4);
    p += 4;
    uint16_t cmd_len;
    std::memcpy(&cmd_len, p, 2);
    p += 2;
    if (static_cast<size_t>(pend - p) - 1 < cmd_len) {
      return {false, {}};
    }
    msg.cmd.assign(p, cmd_len);
    p += cmd_len;
    msg.type = static_cast<msg_wrapper::msg_type>(static_cast<uint8_t>(*p));
    p += 1;
    msg.data.assign(p, pend - p);
    return {true, std::move(msg)};
  }

 private:
  static const uint8_t PayloadMinLen = 4 /*seq*/ + 2 /*cmd_len*/ + 1 /*type*/;
};

}  // namespace detail
}  // namespace rpc_core
#endif
