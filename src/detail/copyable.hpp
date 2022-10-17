#pragma once

namespace RpcCore {
namespace internal {

class copyable {
 public:
  copyable(const copyable&) = default;
  copyable& operator=(const copyable&) = default;

 protected:
  copyable() = default;
  ~copyable() = default;
};

}  // namespace internal
}  // namespace RpcCore
