#pragma once

#include <algorithm>
#include <cassert>

#include "Coder.hpp"
#include "ArduinoJson.h"
#include "../../modules/cryptor/cryptor.hpp"

namespace RpcCore {
namespace coder {

class JsonCoder : public Coder {
public:
    explicit JsonCoder(size_t cap = 1024 * 4)
    : cap_(std::max(cap, size_t{256}))
    {}

public:
    std::string serialize(const MsgWrapper& msg) override
    {
        ArduinoJson::DynamicJsonDocument doc(cap_);
        doc["seq"] = msg.seq;
        doc["type"] = msg.type;
        switch (msg.type) {
            case MsgWrapper::COMMAND:
                doc["cmd"] = msg.cmd;
                break;
            case MsgWrapper::RESPONSE:
                break;
            default:
                LOGE("unknown type:%d", msg.type);
                assert(false);
        }
        doc["data"] = cryptor::encrypt(msg.data);

        std::string payload;
        serializeJson(doc, payload);
        LOGD("serialize:   raw:%s", payload.c_str());
        return payload;
    }

    MsgWrapper unserialize(const std::string& payload, bool& ok) override
    {
        MsgWrapper msg;
        ArduinoJson::DynamicJsonDocument doc(cap_);
        auto error = deserializeJson(doc, payload);
        if (error) {
            LOGE("reason: %s raw: %s", error.c_str(), payload.c_str());
            ok = false;
            return msg;
        }
        msg.seq = doc["seq"];
#ifdef RpcCore_USE_INT_CMD_TYPE
        msg.cmd = doc["cmd"];
#else
        msg.cmd = doc["cmd"].as<std::string>();
#endif
        msg.type = doc["type"];
        msg.data = cryptor::decrypt(doc["data"].as<std::string>());
        LOGD("unserialize: raw:%s, to:%s", payload.c_str(), msg.dump().c_str());
        ok = true;
        return msg;
    }

    size_t cap_;
};

}
}
