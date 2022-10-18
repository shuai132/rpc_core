#pragma once

namespace RpcCore {

namespace FuncHelper {

#include <type_traits>

template <class FPtr>
struct function_traits;

template <class R, class C, class A1>
struct function_traits<R (C::*)(A1)> {  // non-const specialization
  typedef A1 arg_type;
  typedef R result_type;
  typedef R type(A1);
};

template <class R, class C, class A1>
struct function_traits<R (C::*)(A1) const> {  // const specialization
  typedef A1 arg_type;
  typedef R result_type;
  typedef R type(A1);
};

template <class T>
typename function_traits<T>::type* bar_helper(T);

template <typename Ret, typename Arg, typename... Rest>
Arg first_argument_helper(Ret (*)(Arg, Rest...));

template <typename Ret, typename F, typename Arg, typename... Rest>
Arg first_argument_helper(Ret (F::*)(Arg, Rest...));

template <typename Ret, typename F, typename Arg, typename... Rest>
Arg first_argument_helper(Ret (F::*)(Arg, Rest...) const);

template <typename F>
decltype(first_argument_helper(&F::operator())) first_argument_helper(F);

template <typename T>
using first_argument = decltype(first_argument_helper(std::declval<T>()));

template <class F>
struct FirstParamType {
  typedef decltype(bar_helper(&F::operator())) fptr;
  typedef typename std::remove_pointer<fptr>::type signature;
  using type = FuncHelper::first_argument<signature>;
  using type_bare = typename std::remove_cv<typename std::remove_reference<type>::type>::type;
};

}  // namespace FuncHelper

}  // namespace RpcCore
