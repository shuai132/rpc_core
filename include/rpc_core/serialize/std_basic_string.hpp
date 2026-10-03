#pragma once

#include <string>
#include <cstring>
#include <functional>

namespace rpc_core {

namespace detail {

template <typename T>
struct is_std_basic_string : std::false_type {};

template <typename... Args>
struct is_std_basic_string<std::basic_string<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  using VT = typename T::value_type;
  oa.data.append((char*)t.data(), t.size() * sizeof(VT));
  return oa;
}

inline serialize_oarchive& operator>>(std::string&& t, serialize_oarchive& oa) {
  if (oa.data.empty()) oa.data = std::move(t);
  else oa.data.append(t);
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_basic_string<T>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using VT = typename T::value_type;
  if (!ia.require(ia.size) || ia.size % sizeof(VT) != 0) {
    ia.error = true;
    return ia;
  }
  if (ia.size != 0) {
    const auto* begin = reinterpret_cast<const char*>(t.data());
    const auto* end = begin + (t.size() + 1) * sizeof(VT);
    const std::less<const char*> less;
    if (less(ia.data, end) && less(begin, ia.data + ia.size)) {
      // Preserve aliased input before resize can overwrite it or release its storage.
      T decoded(t.get_allocator());
      decoded.resize(ia.size / sizeof(VT));
      std::memcpy(&decoded[0], ia.data, ia.size);
      t.swap(decoded);
      return ia;
    }
  }
  t.resize(ia.size / sizeof(VT));
  if (!t.empty()) std::memcpy(&t[0], ia.data, t.size() * sizeof(VT));
  return ia;
}

}  // namespace rpc_core
