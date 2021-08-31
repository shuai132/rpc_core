#pragma once

#include "Message.hpp"

namespace RpcCore {

class Coder {
public:
    static std::string serialize(const MsgWrapper& msg)
    {
        std::string payload;
#ifdef RpcCore_USE_INT_CMD_TYPE
        payload.reserve(4+2+1+4 + msg.data.size());
        payload.append((char*)&msg.seq, sizeof(SeqType));
        payload.append((char*)&msg.cmd, sizeof(CmdType));
#else
        payload.reserve(4+2+1+4 + msg.cmd.size() + msg.data.size());
        payload.append((char*)&msg.seq, 4);
        uint16_t cmdLen = msg.cmd.length();
        payload.append((char*)&cmdLen, 2);
        payload.append((char*)msg.cmd.data(), cmdLen);
#endif
        payload.append((char*)&msg.type, sizeof(MsgWrapper::MsgType));
        uint32_t dataLen = msg.data.length();
        payload.append((char*)&dataLen, 4);
        payload.append((char*)msg.data.data(), dataLen);
        return payload;
    }

    static MsgWrapper unserialize(const std::string& payload, bool& ok)
    {
        MsgWrapper msg;
        char* p = (char*)payload.data();
        msg.seq = *(SeqType*)p;
        p+=sizeof(msg.seq);
#ifdef RpcCore_USE_INT_CMD_TYPE
        msg.cmd = *(CmdType*)p;
        p+=sizeof(msg.cmd);
#else
        uint16_t cmdLen = *(uint16_t*)p;
        p+=sizeof(cmdLen);
        msg.cmd.append(p, cmdLen);
        p+=cmdLen;
#endif
        msg.type = *(MsgWrapper::MsgType*)(p);
        p+=sizeof(MsgWrapper::MsgType);
        uint32_t dataLen = *(uint32_t*)p;
        p+=sizeof(uint32_t);
        msg.data.append(p, dataLen);
        ok = true;
        return msg;
    }
};

}
