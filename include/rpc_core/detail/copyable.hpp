#pragma once

namespace rpc_core {
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
}  // namespace rpc_core
