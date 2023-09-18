#pragma once

#include <cstdint>
#include <functional>
#include <string>

// #define RPC_CORE_LOG_SHOW_VERBOSE
#include "log.h"
#include "noncopyable.hpp"

namespace rpc_core {
namespace detail {

class data_packer : detail::noncopyable {
 public:
  explicit data_packer(uint32_t max_body_size = UINT32_MAX) : max_body_size_(max_body_size) {}

 public:
  bool pack(const void *data, size_t size, const std::function<bool(const void *data, size_t size)> &cb) const {
    if (size > max_body_size_) {
      return false;
    }
    auto ret = cb(&size, 4);
    if (!ret) return false;
    ret = cb(data, size);
    if (!ret) return false;

    return true;
  }

  std::string pack(const void *data, size_t size) const {
    std::string payload;
    if (size > max_body_size_) {
      RPC_CORE_LOGW("size > max_body_size: %zu > %u", size, max_body_size_);
      return payload;
    }
    payload.insert(0, (char *)&size, 4);
    payload.insert(payload.size(), (char *)data, size);
    return payload;
  }

  std::string pack(const std::string &data) const {
    return pack(data.data(), data.size());
  }

 public:
  bool feed(const void *data, size_t size) {  // NOLINT(misc-no-recursion)
    if (body_size_ != 0) {
      feed_body(data, size);
      return true;
    }

    /// wait header(4 bytes)
    if (header_len_now_ + size < 4) {
      buffer_.insert(buffer_.size(), (char *)data, size);
      header_len_now_ += size;
      return true;
    }

    /// herder data ready, start read body
    // 1. read header, aka: body size
    uint8_t header_len = 4 - header_len_now_;
    size_t body_len = size - header_len;
    for (int i = 0; i < header_len; ++i) {
      buffer_.push_back(((char *)data)[i]);
    }
    body_size_ = *(uint32_t *)(buffer_.data());
    buffer_.clear();
    RPC_CORE_LOGV("feed: wait body_size: %u", body_size_);
    if (body_size_ > max_body_size_) {
      RPC_CORE_LOGW("body_size > max_body_size: %u > %u", body_size_, max_body_size_);
      reset();
      return false;
    }

    // 2. feed body
    if (body_len != 0) {
      feed_body((char *)data + header_len, body_len);
    }
    return true;
  }

  void reset() {
    buffer_.clear();
    buffer_.shrink_to_fit();
    header_len_now_ = 0;
    body_size_ = 0;
  }

 private:
  void feed_body(const void *data, size_t size) {  // NOLINT(misc-no-recursion)
    if (buffer_.size() + size < body_size_) {
      buffer_.insert(buffer_.size(), (char *)data, size);
    } else {
      size_t body_need = body_size_ - buffer_.size();
      size_t body_left = size - body_need;
      buffer_.insert(buffer_.size(), (char *)data, body_need);
      if (on_data) on_data(std::move(buffer_));

      reset();

      if (body_left != 0) {
        feed((char *)data + body_need, body_left);
      }
    }
  }

 public:
  std::function<void(std::string)> on_data;

 private:
  uint32_t max_body_size_;
  std::string buffer_;

  uint32_t header_len_now_ = 0;
  uint32_t body_size_ = 0;
};

}  // namespace detail
}  // namespace rpc_core
