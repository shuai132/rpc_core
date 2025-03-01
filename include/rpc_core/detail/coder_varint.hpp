#pragma once

#include <limits>
#include <utility>

#include "msg_wrapper.hpp"
#include "varint.hpp"

namespace rpc_core {
namespace detail {

class coder {
 public:
  static std::pair<bool, std::string> serialize(const msg_wrapper& msg) {
    if (msg.cmd.size() > (std::numeric_limits<uint16_t>::max)()) {
      return {false, {}};
    }
    std::string payload;
    std::string v_seq = to_varint(msg.seq);
    std::string v_cmd_len = to_varint(msg.cmd.length());
    payload.reserve(v_seq.size() + v_cmd_len.size() + sizeof(msg.type) + msg.cmd.size() + msg.data.size());
    payload.append(v_seq);
    payload.append(v_cmd_len);
    payload.append(msg.cmd);
    payload.append((char*)&msg.type, sizeof(msg.type));
    if (msg.request_payload) {
      payload.append(*msg.request_payload);
    } else {
      payload.append(msg.data);
    }
    return {true, std::move(payload)};
  }

  static std::pair<bool, msg_wrapper> deserialize(const std::string& payload) {
    msg_wrapper msg;
    const char* p = payload.data();
    size_t remaining = payload.size();
    size_t bytes = 0;
    uint32_t seq = 0;
    if (!from_varint(p, remaining, seq, bytes)) {
      return {false, {}};
    }
    msg.seq = seq;
    p += bytes;
    remaining -= bytes;
    uint32_t cmd_len = 0;
    if (!from_varint(p, remaining, cmd_len, bytes) || cmd_len > (std::numeric_limits<uint16_t>::max)()) {
      return {false, {}};
    }
    p += bytes;
    remaining -= bytes;
    if (remaining < sizeof(msg.type) || cmd_len > remaining - sizeof(msg.type)) {
      return {false, {}};
    }
    msg.cmd.assign(p, cmd_len);
    p += cmd_len;
    remaining -= cmd_len;
    msg.type = static_cast<msg_wrapper::msg_type>(static_cast<uint8_t>(*p));
    p += sizeof(msg.type);
    remaining -= sizeof(msg.type);
    msg.data.assign(p, remaining);
    return {true, std::move(msg)};
  }
};

}  // namespace detail
}  // namespace rpc_core
