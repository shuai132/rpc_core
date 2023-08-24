#pragma once

namespace RPC_CORE_NAMESPACE {
namespace detail {

class copyable {
 public:
  copyable(const copyable&) = default;
  copyable& operator=(const copyable&) = default;

 protected:
  copyable() = default;
  ~copyable() = default;
};

}  // namespace detail
}  // namespace RPC_CORE_NAMESPACE
