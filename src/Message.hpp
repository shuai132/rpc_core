#pragma once

#include <string>
#include <cstdint>

#include "base/copyable.hpp"

namespace RpcCore {

/**
 * 数据序列化接口
 * 用户消息需要继承它
 * std::string作为数据载体 并不要求可读
 */
struct Message : copyable {
    virtual std::string serialize() const = 0;
    virtual bool deSerialize(const std::string& data) = 0;
};

/**
 * 值类型模板
 * NOTICE: 请谨慎使用!!!
 * todo: 保证/验证字节对齐
 */
template <typename T>
struct Value : Message {
    T value;

    Value() = default;
    Value(T v) : value(v) {}

    std::string serialize() const override {
        return std::string((char*)&value, sizeof(T));
    };
    bool deSerialize(const std::string& data) override {
        return memcpy((void*)&value, data.data(), sizeof(T));
    };
};

using Void = Value<uint8_t>;
const Void VOID{};

struct String : Message, public std::string {
    using std::string::string;
    String(std::string str) {
        this->swap(str);
    }
    std::string serialize() const override {
        return *this;
    }
    bool deSerialize(const std::string& data) override {
        *this = data;
        return true;
    }
};

}
