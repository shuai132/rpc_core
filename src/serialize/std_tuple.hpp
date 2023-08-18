#pragma once

#include <tuple>
#include <type_traits>

namespace RpcCore {

namespace detail {

template <std::size_t I, class T>
using tuple_element_t = typename std::tuple_element<I, typename std::remove_cv<typename std::remove_reference<T>::type>::type>::type;

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

struct tuple_meta {
  char* data_de_serialize_ptr = nullptr;
  std::string data_serialize;
};

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl {
  static void serialize(Tuple& t, tuple_meta& meta) {
    auto payload = RpcCore::serialize<tuple_element_t<I, Tuple>>(std::get<I>(t));
    uint32_t payload_size = payload.size();
    meta.data_serialize.append((char*)&payload_size, sizeof(payload_size));
    meta.data_serialize.append(std::move(payload));
  }
};

template <typename Tuple, std::size_t N>
struct tuple_serialize_helper {
  static void serialize(Tuple& t, tuple_meta& meta) {
    tuple_serialize_helper<Tuple, N - 1>::serialize(t, meta);
    tuple_serialize_helper_impl<Tuple, N - 1>::serialize(t, meta);
  }
};

template <typename Tuple>
struct tuple_serialize_helper<Tuple, 1> {
  static void serialize(Tuple& t, tuple_meta& meta) {
    tuple_serialize_helper_impl<Tuple, 0>::serialize(t, meta);
  }
};

template <typename... Args>
void tuple_serialize(const std::tuple<Args...>& t, tuple_meta& meta) {
  tuple_serialize_helper<decltype(t), sizeof...(Args)>::serialize(t, meta);
}

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    auto& p = meta.data_de_serialize_ptr;
    uint32_t payload_size = *(uint32_t*)(p);
    p += sizeof(payload_size);
    bool ret = deserialize<tuple_element_t<I, Tuple>>(detail::string_view(p, payload_size), std::get<I>(t));
    p += payload_size;
    return ret;
  }
};

template <typename Tuple, std::size_t N>
struct tuple_de_serialize_helper {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    bool ret = tuple_de_serialize_helper<Tuple, N - 1>::de_serialize(t, meta);
    if (!ret) return false;
    return tuple_de_serialize_helper_impl<Tuple, N - 1>::de_serialize(t, meta);
  }
};

template <typename Tuple>
struct tuple_de_serialize_helper<Tuple, 1> {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    return tuple_de_serialize_helper_impl<Tuple, 0>::de_serialize(t, meta);
  }
};

template <typename... Args>
bool tuple_de_serialize(std::tuple<Args...>& t, tuple_meta& meta) {
  return tuple_de_serialize_helper<decltype(t), sizeof...(Args)>::de_serialize(t, meta);
}

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
inline std::string serialize(const T& t) {
  detail::tuple_meta meta;
  detail::tuple_serialize(t, meta);
  return std::move(meta.data_serialize);
}

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
inline bool deserialize(const detail::string_view& data, T& t) {
  detail::tuple_meta meta;
  meta.data_de_serialize_ptr = (char*)data.data();
  return detail::tuple_de_serialize(t, meta);
}

}  // namespace RpcCore
