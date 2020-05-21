#pragma once

#include <string>
#include <cstdint>

#include "Message.hpp"

#define ENSURE_TYPE_IS_MESSAGE(T) \
        typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0

namespace RpcCore {

template <typename T, ENSURE_TYPE_IS_MESSAGE(T)>
struct RspType {
    T message;
    bool success;

    RspType(T m, bool s) : message(std::move(m)), success(s) {}

    RspType(bool s) : message(T()), success(s) {} // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    RspType(T m) : message(std::move(m)), success(true) {} // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
};

template <typename T, ENSURE_TYPE_IS_MESSAGE(T)>
inline RspType<T> R(T msg, bool success) {
    return RspType<T>(std::move(msg), success);
}

}
