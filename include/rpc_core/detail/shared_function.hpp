#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace rpc_core {
namespace detail {

// Copies retain the callable's state. Invocation keeps it alive even if user
// code replaces the callback or destroys the object containing it.
template <typename Signature>
class shared_function;

template <typename R, typename... Args>
class shared_function<R(Args...)> {
 public:
  using function_type = std::function<R(Args...)>;

  shared_function() = default;
  shared_function(std::nullptr_t) {}
  shared_function(function_type fn) { *this = std::move(fn); }

  shared_function& operator=(function_type fn) {
    fn_ = fn ? std::make_shared<function_type>(std::move(fn)) : nullptr;
    return *this;
  }

  shared_function& operator=(std::nullptr_t) {
    fn_.reset();
    return *this;
  }

  explicit operator bool() const { return bool(fn_); }
  bool operator==(std::nullptr_t) const { return !fn_; }
  bool operator!=(std::nullptr_t) const { return bool(fn_); }

  R operator()(Args... args) const {
    auto keeper = fn_;
    if (keeper) return (*keeper)(std::forward<Args>(args)...);
    // Preserve std::function's empty-call behavior, including no-exception builds.
    return function_type{}(std::forward<Args>(args)...);
  }

 private:
  std::shared_ptr<function_type> fn_;
};

}  // namespace detail
}  // namespace rpc_core
