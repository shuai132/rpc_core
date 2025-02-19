#pragma once

// config
#include "config.hpp"

#if defined(RPC_CORE_SERIALIZE_USE_CUSTOM)

#include RPC_CORE_SERIALIZE_USE_CUSTOM

#elif defined(RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON)

#include "serialize_nlohmann_json.hpp"

#else

// type traits
#include "detail/type_traits.hpp"

// serialize api
#include "serialize_api.hpp"

// raw type
#include "serialize/type_raw.hpp"

// other types
#include "serialize/binary_wrap.hpp"
#include "serialize/std_array.hpp"
#include "serialize/std_basic_string.hpp"
#include "serialize/std_bitset.hpp"
#include "serialize/std_chrono.hpp"
#include "serialize/std_complex.hpp"
#include "serialize/std_container_adaptors.hpp"
#include "serialize/std_forward_list.hpp"
#include "serialize/std_list_like.hpp"
#include "serialize/std_map.hpp"
#include "serialize/std_pair.hpp"
#include "serialize/std_set.hpp"
#include "serialize/std_shared_ptr.hpp"
#include "serialize/std_tuple.hpp"
#include "serialize/std_unique_ptr.hpp"
#include "serialize/type_enum.hpp"
#include "serialize/type_ptr.hpp"
#include "serialize/type_struct.hpp"
#include "serialize/type_void.hpp"

#endif
