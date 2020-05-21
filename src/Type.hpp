#pragma once

#include <type_traits>
#include <cstdint>

#include "Message.hpp"

#define RpcCore_ENSURE_TYPE_IS_MESSAGE(T) \
        typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0
