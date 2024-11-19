#pragma once

#include "msg_wrapper.hpp"
#include "varint.hpp"

namespace rpc_core {
namespace detail {

class coder {
 public:
  static std::string serialize(const msg_wrapper& msg) {
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
    return payload;
  }

  static msg_wrapper deserialize(const std::string& payload, bool& ok) {
    msg_wrapper msg;
    char* p = (char*)payload.data();
    const char* pend = payload.data() + payload.size();
    uint8_t bytes;
    msg.seq = from_varint(p, &bytes);
    p += bytes;
    uint16_t cmd_len = from_varint(p, &bytes);
    p += bytes;
    if (p + cmd_len + sizeof(msg.type) > pend) {
      ok = false;
      return msg;
    }
    msg.cmd.assign(p, cmd_len);
    p += cmd_len;
    msg.type = *(msg_wrapper::msg_type*)(p);
    p += sizeof(msg.type);
    msg.data.assign(p, pend - p);
    ok = true;
    return msg;
  }
};

}  // namespace detail
}  // namespace rpc_core
