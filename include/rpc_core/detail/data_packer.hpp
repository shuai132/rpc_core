#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// #define RPC_CORE_LOG_SHOW_VERBOSE
#include "log.h"
#include "noncopyable.hpp"
#include "shared_function.hpp"

namespace rpc_core {
namespace detail {

class data_packer : detail::noncopyable {
 public:
  explicit data_packer(uint32_t max_body_size = UINT32_MAX) : max_body_size_(max_body_size) {}

 public:
  bool pack(const void *data, size_t size, const std::function<bool(const void *data, size_t size)> &cb) const {
    if (size > max_body_size_ || (size != 0 && data == nullptr)) {
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
    if (size != 0 && data == nullptr) return payload;
    if (size > max_body_size_) {
      RPC_CORE_LOGW("size > max_body_size: %zu > %u", size, max_body_size_);
      return payload;
    }
    payload.insert(0, (char *)&size, 4);
    if (size != 0) payload.insert(payload.size(), (char *)data, size);
    return payload;
  }

  std::string pack(const std::string &data) const {
    return pack(data.data(), data.size());
  }

 public:
  // After invalid framing, only reset() may start another stream.
  bool feed(const void *data, size_t size) {
    if (failed_) return false;
    if (size != 0 && data == nullptr) {
      failed_ = true;
      return false;
    }
    if (feeding_) {
      if (size != 0) deferred_.append(static_cast<const char *>(data), size);
      return true;
    }
    struct guard {
      bool &active;
      ~guard() { active = false; }
    } scope{feeding_};
    feeding_ = true;
    if (!feed_chunk(data, size)) return false;
    while (!deferred_.empty()) {
      std::string next;
      next.swap(deferred_);
      if (!feed_chunk(next.data(), next.size())) return false;
    }
    return true;
  }

  void reset() {
    buffer_.clear();
    deferred_.clear();
    discard_chunk_ = feeding_;
    header_len_now_ = 0;
    body_size_ = 0;
    failed_ = false;
  }

 private:
  bool feed_chunk(const void *data, size_t size) {
    discard_chunk_ = false;
    auto bytes = static_cast<const char *>(data);
    while (size != 0) {
      if (header_len_now_ < sizeof(body_size_)) {
        const auto count = std::min(size, sizeof(body_size_) - header_len_now_);
        buffer_.append(bytes, count);
        header_len_now_ += static_cast<uint32_t>(count);
        bytes += count;
        size -= count;
        if (header_len_now_ != sizeof(body_size_)) return true;
        std::memcpy(&body_size_, buffer_.data(), sizeof(body_size_));
        buffer_.clear();
        if (body_size_ > max_body_size_) {
          RPC_CORE_LOGW("body_size > max_body_size: %u > %u", body_size_, max_body_size_);
          failed_ = true;
          return false;
        }
      }

      const auto count = std::min(size, body_size_ - buffer_.size());
      buffer_.append(bytes, count);
      bytes += count;
      size -= count;
      if (buffer_.size() != body_size_) return true;

      // Finish this frame before user code runs; reentrant feeds follow this chunk.
      auto payload = std::move(buffer_);
      buffer_.clear();
      header_len_now_ = 0;
      body_size_ = 0;
      if (on_data) on_data(std::move(payload));
      if (failed_) return false;
      if (discard_chunk_) return true;
    }
    return true;
  }

 public:
  shared_function<void(std::string)> on_data;

 private:
  uint32_t max_body_size_;
  std::string buffer_;
  std::string deferred_;
  bool feeding_ = false;
  bool discard_chunk_ = false;

  uint32_t header_len_now_ = 0;
  uint32_t body_size_ = 0;
  bool failed_ = false;
};

}  // namespace detail
}  // namespace rpc_core
