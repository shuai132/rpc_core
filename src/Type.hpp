#pragma once

#include <string>
#include <cstdint>

#include "Message.hpp"

#define ENSURE_TYPE_IS_MESSAGE(T) \
        typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0
