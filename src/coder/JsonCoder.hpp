#pragma once

#include <algorithm>
#include <cassert>

#include "Coder.hpp"
#include "ArduinoJson.h"

namespace RpcCore {
namespace coder {

class JsonCoder : public Coder {
public:
    explicit JsonCoder(size_t cap = 1024 * 4)
    : cap_(std::max(cap, 256LU))
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
                doc["ok"] = (int)msg.success;   // for small size
                break;
            default:
                LOGE("unknown type:%d", msg.type);
                assert(false);
        }
        if (not msg.data.empty()) {
            doc["data"] = msg.data; // todo: base64
        }

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
        msg.cmd = doc["cmd"];
        msg.type = doc["type"];
        msg.success = doc["ok"];
        msg.data = doc["data"].as<std::string>();   // todo: base64
        LOGD("unserialize: raw:%s, to:%s", payload.c_str(), msg.dump().c_str());
        ok = true;
        return msg;
    }

    size_t cap_;
};

}
}
