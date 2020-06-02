#pragma once

#include "../base/noncopyable.hpp"
#include "../MsgWrapper.hpp"
#include "../Type.hpp"

namespace RpcCore {

class Coder : noncopyable {
public:
    virtual ~Coder() = default;

public:
    virtual std::string serialize(const MsgWrapper& msg) = 0;

    virtual MsgWrapper unserialize(const std::string& payload, bool& ok) = 0;
};

}
