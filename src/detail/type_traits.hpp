#pragma once

#include <type_traits>

namespace RPC_CORE_NAMESPACE {
namespace detail {

template <typename Base, typename T, typename... Args>
struct all_base_of {
  static constexpr bool value = std::is_base_of<Base, T>::value && all_base_of<Base, Args...>::value;
};

template <typename Base, typename T>
struct all_base_of<Base, T> {
  static constexpr bool value = std::is_base_of<Base, T>::value;
};

}  // namespace detail
}  // namespace RPC_CORE_NAMESPACE
