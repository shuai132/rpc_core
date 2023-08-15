#pragma once

namespace RpcCore {

template <typename... Args>
struct Tuple;

namespace detail {

struct tuple_meta {
  const std::string* data_de_serialize = nullptr;
  std::string data_serialize;
  uint32_t pos = 0;
};

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl {
  static void serialize(Tuple& t, tuple_meta& meta) {
    auto payload = std::get<I>(t).serialize();
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
void tuple_serialize(const Tuple<Args...>& t, tuple_meta& meta) {
  tuple_serialize_helper<decltype(t), sizeof...(Args)>::serialize(t, meta);
}

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl {
  static bool de_serialize(Tuple& t, tuple_meta& meta) {
    auto p = meta.data_de_serialize->data() + meta.pos;
    uint32_t payload_size = *(uint32_t*)(p);
    meta.pos += sizeof(payload_size);
    p = meta.data_de_serialize->data() + meta.pos;
    bool ret = std::get<I>(t).deSerialize(std::string(p, payload_size));
    meta.pos += payload_size;
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
bool tuple_de_serialize(Tuple<Args...>& t, tuple_meta& meta) {
  return tuple_de_serialize_helper<decltype(t), sizeof...(Args)>::de_serialize(t, meta);
}

}  // namespace detail
}  // namespace RpcCore
