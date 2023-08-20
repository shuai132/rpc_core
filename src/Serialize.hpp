#pragma once

#include <type_traits>

#include "Type.hpp"
#include "detail/string_view.hpp"

// clang-format off
/// The include order cannot be changed!!!
#include "serialize/void.hpp"

#include "serialize/trivial_type.hpp"

#include "serialize/std_string.hpp"

// container interface
#include "serialize/list_like.hpp"
#include "serialize/std_tuple.hpp"

// container implementation
#include "serialize/list_like_impl.hpp"
#include "serialize/std_tuple_impl.hpp"
