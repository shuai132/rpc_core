#pragma once

#include <string>

#include "base/copyable.hpp"
#include "Type.hpp"
#include "log.h"

namespace RpcCore {

const VoidValue VOID{};

using CmdType = uint32_t;
using SeqType = uint32_t;

/**
 * 包装数据 用于协议分析
 */
struct MsgWrapper : copyable {
    enum MsgType {
        COMMAND,
        RESPONSE,
    };
    enum InnerCmd {
        PING,
        PONG,
    };

    SeqType seq;
    CmdType cmd;
    MsgType type;
    bool    success;
    std::string data;

    std::string dump() const {
        char tmp[100];
        snprintf(tmp, 100, "seq:%u, type:%u, cmd:%u, success:%u", seq, type, cmd, success);
        return tmp;
    }

    /**
     * 将data解析为指定类型的数据
     * T为Message类型
     */
    template<typename T, ENSURE_TYPE_IS_MESSAGE(T)>
    T unpackAs() const {
        T message;
        bool ok = message.deSerialize(data);
        if (not ok) {
            LOGE("serialize error, msg info:%s", dump().c_str());
        }
        return message;
    }

    /**
     * 创建Cmd消息
     * @param cmd
     * @param data
     * @return
     */
    static MsgWrapper MakeCmd(CmdType cmd, SeqType seq, const Message& data = VOID) {
        MsgWrapper msg;
        msg.type = MsgWrapper::COMMAND;
        msg.cmd = cmd;
        msg.seq = seq;
        if (&data != &VOID) {
            msg.data = data.serialize();
        }
        return msg;
    }

    /**
     * 创建Rsp消息
     * @param seq 对应Cmd的seq
     * @param message 将保存在Msg.data
     * @param success 成功/失败
     * @return
     */
    static MsgWrapper MakeRsp(SeqType seq, const Message& message = VOID, bool success = true) {
        MsgWrapper msg;
        msg.type = MsgWrapper::RESPONSE;
        msg.success = success;
        msg.seq = seq;
        if ((intptr_t*) &message != (intptr_t*) &VOID) {
            msg.data = message.serialize();
        }
        return msg;
    }
};

}
