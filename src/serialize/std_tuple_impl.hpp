#pragma once

namespace RpcCore {

namespace detail {

struct tuple_meta {
  char* data_de_serialize_ptr = nullptr;
  std::string data_serialize;
};

enum class tuple_serialize_type {
  Ignore,
  RawType,
  Normal,
};

template <typename T>
struct is_std_ignore {
  static const bool value = std::is_same<detail::remove_cvref_t<decltype(std::ignore)>, T>::value;
};

template <typename T>
struct tuple_serialize_type_check {
  static constexpr tuple_serialize_type value = std::is_fundamental<T>::value ? tuple_serialize_type::RawType
                                                : is_std_ignore<T>::value     ? tuple_serialize_type::Ignore
                                                                              : tuple_serialize_type::Normal;
};

template <typename Tuple, std::size_t I, tuple_serialize_type type>
struct tuple_serialize_helper_impl;

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl<Tuple, I, tuple_serialize_type::Normal> {
  static void serialize(const Tuple& t, tuple_meta& meta) {
    auto payload = RpcCore::serialize<detail::remove_cvref_t<tuple_element_t<I, Tuple>>>(std::get<I>(t));
    uint32_t payload_size = payload.size();
    meta.data_serialize.append((char*)&payload_size, sizeof(payload_size));
    meta.data_serialize.append(std::move(payload));
  }
};

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl<Tuple, I, tuple_serialize_type::RawType> {
  static void serialize(const Tuple& t, tuple_meta& meta) {
    auto payload = RpcCore::serialize<detail::remove_cvref_t<tuple_element_t<I, Tuple>>>(std::get<I>(t));
    meta.data_serialize.append(std::move(payload));
  }
};

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl<Tuple, I, tuple_serialize_type::Ignore> {
  static void serialize(const Tuple& t, tuple_meta& meta) {}
};

template <typename Tuple, std::size_t N>
struct tuple_serialize_helper {
  static void serialize(const Tuple& t, tuple_meta& meta) {
    tuple_serialize_helper<Tuple, N - 1>::serialize(t, meta);
    tuple_serialize_helper_impl<Tuple, N - 1, tuple_serialize_type_check<tuple_element_t<N - 1, Tuple>>::value>::serialize(t, meta);
  }
};

template <typename Tuple>
struct tuple_serialize_helper<Tuple, 1> {
  static void serialize(const Tuple& t, tuple_meta& meta) {
    tuple_serialize_helper_impl<Tuple, 0, tuple_serialize_type_check<tuple_element_t<0, Tuple>>::value>::serialize(t, meta);
  }
};

template <typename... Args>
void tuple_serialize(const std::tuple<Args...>& t, tuple_meta& meta) {
  tuple_serialize_helper<decltype(t), sizeof...(Args)>::serialize(t, meta);
}

template <typename Tuple, std::size_t I, tuple_serialize_type type>
struct tuple_de_serialize_helper_impl;

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl<Tuple, I, tuple_serialize_type::Normal> {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    auto& p = meta.data_de_serialize_ptr;
    uint32_t payload_size = *(uint32_t*)(p);
    p += sizeof(payload_size);
    bool ret = deserialize<tuple_element_t<I, Tuple>>(detail::string_view(p, payload_size), std::get<I>(t));
    p += payload_size;
    return ret;
  }
};

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl<Tuple, I, tuple_serialize_type::RawType> {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    auto& p = meta.data_de_serialize_ptr;
    size_t cost_size;
    bool ret = deserialize<tuple_element_t<I, Tuple>>(detail::string_view(p, 0), std::get<I>(t), &cost_size);
    p += cost_size;
    return ret;
  }
};

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl<Tuple, I, tuple_serialize_type::Ignore> {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    return true;
  }
};

template <typename Tuple, std::size_t N>
struct tuple_de_serialize_helper {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    bool ret = tuple_de_serialize_helper<Tuple, N - 1>::de_serialize(t, meta);
    if (!ret) return false;
    return tuple_de_serialize_helper_impl<Tuple, N - 1, tuple_serialize_type_check<tuple_element_t<N - 1, Tuple>>::value>::de_serialize(t, meta);
  }
};

template <typename Tuple>
struct tuple_de_serialize_helper<Tuple, 1> {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    return tuple_de_serialize_helper_impl<Tuple, 0, tuple_serialize_type_check<tuple_element_t<0, Tuple>>::value>::de_serialize(t, meta);
  }
};

template <typename... Args>
bool tuple_de_serialize(std::tuple<Args...>& t, tuple_meta& meta) {
  return tuple_de_serialize_helper<decltype(t), sizeof...(Args)>::de_serialize(t, meta);
}

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type>
inline std::string serialize(const T& t) {
  detail::tuple_meta meta;
  detail::tuple_serialize(t, meta);
  return std::move(meta.data_serialize);
}

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type>
inline bool deserialize(const detail::string_view& data, T& t) {
  detail::tuple_meta meta;
  meta.data_de_serialize_ptr = (char*)data.data();
  return detail::tuple_de_serialize(t, meta);
}

}  // namespace RpcCore
