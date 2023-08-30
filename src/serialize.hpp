#pragma once

// config
#include "config.hpp"

// type traits
#include "detail/type_traits.hpp"

// serialize type
#include "serialize_type.hpp"

// types
#include "detail/string_view.hpp"
#include "serialize/define_type.hpp"
#include "serialize/ptr_type.hpp"
#include "serialize/raw_type.hpp"
#include "serialize/std_array.hpp"
#include "serialize/std_basic_string.hpp"
#include "serialize/std_bitset.hpp"
#include "serialize/std_chrono.hpp"
#include "serialize/std_complex.hpp"
#include "serialize/std_forward_list.hpp"
#include "serialize/std_shared_ptr.hpp"
#include "serialize/std_unique_ptr.hpp"
#include "serialize/void.hpp"

// container interface
#include "serialize/std_list_like.hpp"
#include "serialize/std_map.hpp"
#include "serialize/std_pair.hpp"
#include "serialize/std_set.hpp"
#include "serialize/std_tuple.hpp"

// container implementation
#include "serialize/define_type_impl.hpp"
#include "serialize/std_list_like_impl.hpp"
#include "serialize/std_map_impl.hpp"
#include "serialize/std_pair_impl.hpp"
#include "serialize/std_set_impl.hpp"
#include "serialize/std_tuple_impl.hpp"
