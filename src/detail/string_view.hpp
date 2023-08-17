#pragma once

#include <cstddef>
#include <string>

namespace RpcCore {
namespace detail {

class string_view {
 public:
  string_view(const char* data, size_t size) : data_(data), size_(size) {}
  string_view(const std::string& data) : data_(data.data()), size_(data.size()) {}  // NOLINT
  const char* data() const {
    return data_;
  }
  size_t size() const {
    return size_;
  }

 private:
  const char* data_;
  size_t size_;
};

}  // namespace detail
}  // namespace RpcCore
