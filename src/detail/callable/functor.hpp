#pragma once

#include "function.hpp"
#include "helpers.hpp"
#include "member_function.hpp"

namespace rpc_core {

namespace detail {

template <typename Class>
using call_operator_traits = member_function_traits<decltype(&Class::operator())>;

// classes with operator()
template <typename Class>
struct functor_traits : function_traits<typename call_operator_traits<Class>::function_type> {
  typedef call_operator_traits<Class> call_operator;
};

}  // namespace detail

template <typename Class>
struct functor_traits : detail::functor_traits<detail::remove_cvref_t<Class>> {};

}  // namespace rpc_core
