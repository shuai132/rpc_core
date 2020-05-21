#pragma once

#include <string>

#include "base/copyable.hpp"

#define RpcCore_ENSURE_TYPE_IS_MESSAGE(T)   typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0

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
    explicit Value(T v) : value(v) {}

    std::string serialize() const override {
        // 因为有虚函数 要取value的地址
        return std::string((char*)&value, sizeof(T));
    };

    bool deSerialize(const std::string& data) override {
        return memcpy((void*)&value, data.data(), sizeof(T));
    };
};

using VoidValue = Value<char>;

struct StringValue : public Message {
    std::string value;

    std::string serialize() const override {
        return value;
    }
    bool deSerialize(const std::string& data) override {
        value = data;
        return true;
    }
};

}
