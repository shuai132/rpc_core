#pragma once

// config
#include "config.hpp"

// type traits
#include "detail/type_traits.hpp"

// serialize type
#include "serialize_type.hpp"

// raw type
#include "serialize/type_raw.hpp"

// other types
#include "detail/string_view.hpp"
#include "serialize/std_array.hpp"
#include "serialize/std_basic_string.hpp"
#include "serialize/std_bitset.hpp"
#include "serialize/std_chrono.hpp"
#include "serialize/std_complex.hpp"
#include "serialize/std_forward_list.hpp"
#include "serialize/std_shared_ptr.hpp"
#include "serialize/std_unique_ptr.hpp"
#include "serialize/type_enum.hpp"
#include "serialize/type_ptr.hpp"
#include "serialize/type_struct.hpp"
#include "serialize/type_void.hpp"

// container interface
#include "serialize/std_list_like.hpp"
#include "serialize/std_map.hpp"
#include "serialize/std_pair.hpp"
#include "serialize/std_set.hpp"
#include "serialize/std_tuple.hpp"

// container implementation
#include "serialize/std_list_like_impl.hpp"
#include "serialize/std_map_impl.hpp"
#include "serialize/std_pair_impl.hpp"
#include "serialize/std_set_impl.hpp"
#include "serialize/std_tuple_impl.hpp"
#include "serialize/type_struct_impl.hpp"

// container adaptors
#include "serialize/std_container_adaptors.hpp"
