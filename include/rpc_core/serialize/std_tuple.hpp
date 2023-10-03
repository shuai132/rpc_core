#pragma once

#include <tuple>

#include "../detail/callable/helpers.hpp"

namespace rpc_core {

namespace detail {

template <std::size_t I, class T>
using tuple_element_t = detail::remove_cvref_t<typename std::tuple_element<I, detail::remove_cvref_t<T>>::type>;

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

template <typename T>
struct is_std_ignore {
  static const bool value = std::is_same<detail::remove_cvref_t<decltype(std::ignore)>, T>::value;
};

enum class tuple_serialize_type {
  Ignore,
  RawType,
  Normal,
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
  static void serialize(const Tuple& t, serialize_oarchive& oa) {
    serialize_oarchive tmp;
    std::get<I>(t) >> tmp;
    tmp >> oa;
  }
};

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl<Tuple, I, tuple_serialize_type::RawType> {
  static void serialize(const Tuple& t, serialize_oarchive& oa) {
    std::get<I>(t) >> oa;
  }
};

template <typename Tuple, std::size_t I>
struct tuple_serialize_helper_impl<Tuple, I, tuple_serialize_type::Ignore> {
  static void serialize(const Tuple& t, serialize_oarchive& oa) {
    RPC_CORE_UNUSED(t);
    RPC_CORE_UNUSED(oa);
  }
};

template <typename Tuple, std::size_t N>
struct tuple_serialize_helper {
  static void serialize(const Tuple& t, serialize_oarchive& oa) {
    tuple_serialize_helper<Tuple, N - 1>::serialize(t, oa);
    tuple_serialize_helper_impl<Tuple, N - 1, tuple_serialize_type_check<tuple_element_t<N - 1, Tuple>>::value>::serialize(t, oa);
  }
};

template <typename Tuple>
struct tuple_serialize_helper<Tuple, 1> {
  static void serialize(const Tuple& t, serialize_oarchive& oa) {
    tuple_serialize_helper_impl<Tuple, 0, tuple_serialize_type_check<tuple_element_t<0, Tuple>>::value>::serialize(t, oa);
  }
};

template <typename... Args>
void tuple_serialize(const std::tuple<Args...>& t, serialize_oarchive& oa) {
  tuple_serialize_helper<decltype(t), sizeof...(Args)>::serialize(t, oa);
}

template <typename Tuple, std::size_t I, tuple_serialize_type type>
struct tuple_de_serialize_helper_impl;

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl<Tuple, I, tuple_serialize_type::Normal> {
  static void de_serialize(Tuple& t, serialize_iarchive& ia) {
    serialize_iarchive tmp;
    tmp << ia;
    std::get<I>(t) << tmp;
  }
};

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl<Tuple, I, tuple_serialize_type::RawType> {
  static void de_serialize(Tuple& t, serialize_iarchive& ia) {
    std::get<I>(t) << ia;
  }
};

template <typename Tuple, std::size_t I>
struct tuple_de_serialize_helper_impl<Tuple, I, tuple_serialize_type::Ignore> {
  static void de_serialize(Tuple& t, serialize_iarchive& ia) {
    RPC_CORE_UNUSED(t);
    RPC_CORE_UNUSED(ia);
  }
};

template <typename Tuple, std::size_t N>
struct tuple_de_serialize_helper {
  static void de_serialize(Tuple& t, serialize_iarchive& ia) {
    tuple_de_serialize_helper<Tuple, N - 1>::de_serialize(t, ia);
    if (ia.error) return;
    tuple_de_serialize_helper_impl<Tuple, N - 1, tuple_serialize_type_check<tuple_element_t<N - 1, Tuple>>::value>::de_serialize(t, ia);
  }
};

template <typename Tuple>
struct tuple_de_serialize_helper<Tuple, 1> {
  static void de_serialize(Tuple& t, serialize_iarchive& ia) {
    tuple_de_serialize_helper_impl<Tuple, 0, tuple_serialize_type_check<tuple_element_t<0, Tuple>>::value>::de_serialize(t, ia);
  }
};

template <typename... Args>
void tuple_de_serialize(std::tuple<Args...>& t, serialize_iarchive& ia) {
  tuple_de_serialize_helper<decltype(t), sizeof...(Args)>::de_serialize(t, ia);
}

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  detail::tuple_serialize(t, oa);
  return oa;
}

template <typename T, typename std::enable_if<detail::is_tuple<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  detail::tuple_de_serialize(t, ia);
  return ia;
}

}  // namespace rpc_core
