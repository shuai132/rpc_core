#pragma once

#include "../Message.hpp"

namespace RpcCore {
namespace detail {

class Coder {
 public:
  static std::string serialize(const MsgWrapper& msg) {
    std::string payload;
    payload.reserve(PayloadMinLen + msg.cmd.size() + msg.data.size());
    payload.append((char*)&msg.seq, 4);
    uint16_t cmdLen = msg.cmd.length();
    payload.append((char*)&cmdLen, 2);
    payload.append((char*)msg.cmd.data(), cmdLen);
    payload.append((char*)&msg.type, 1);
    payload.append(msg.data);
    return payload;
  }

  static MsgWrapper deserialize(const std::string& payload, bool& ok) {
    MsgWrapper msg;
    if (payload.size() < PayloadMinLen) {
      ok = false;
      return msg;
    }
    char* p = (char*)payload.data();
    const char* pend = payload.data() + payload.size();
    msg.seq = *(SeqType*)p;
    p += 4;
    uint16_t cmdLen = *(uint16_t*)p;
    p += 2;
    if (p + cmdLen + 1 > pend) {
      ok = false;
      return msg;
    }
    msg.cmd.append(p, cmdLen);
    p += cmdLen;
    msg.type = *(MsgWrapper::MsgType*)(p);
    p += 1;
    msg.data.append(p, pend - p);
    ok = true;
    return msg;
  }

 private:
  static const uint8_t PayloadMinLen = 4 /*seq*/ + 2 /*cmdLen*/ + 1 /*type*/;
};

}  // namespace detail
}  // namespace RpcCore
