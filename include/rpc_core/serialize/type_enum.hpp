#pragma once

#include <limits>
#include <type_traits>

namespace rpc_core {

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::auto_uintmax impl((uintmax_t)t);
  impl >> oa;
  return oa;
}

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::auto_uintmax impl;
  impl << ia;
  if (ia.error) return ia;
  using underlying = typename std::underlying_type<T>::type;
  const auto max = static_cast<uintmax_t>((std::numeric_limits<underlying>::max)());
  if (impl.value <= max) {
    t = static_cast<T>(static_cast<underlying>(impl.value));
  } else if (std::is_signed<underlying>::value &&
             impl.value >= static_cast<uintmax_t>((std::numeric_limits<underlying>::min)())) {
    // Negative enums historically use the uintmax_t conversion of their value.
    // Decode that representation without narrowing an out-of-range integer.
    const auto magnitude_minus_one = (std::numeric_limits<uintmax_t>::max)() - impl.value;
    t = static_cast<T>(static_cast<underlying>(-static_cast<intmax_t>(magnitude_minus_one) - 1));
  } else {
    ia.error = true;
  }
  return ia;
}

}  // namespace rpc_core
