#pragma once

namespace RPC_CORE_NAMESPACE {
namespace detail {

class noncopyable {
 public:
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace detail
}  // namespace RPC_CORE_NAMESPACE
