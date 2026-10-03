#pragma once

#include <algorithm>
#include <exception>
#include <memory>
#include <unordered_set>
#include <vector>

// #define RPC_CORE_LOG_SHOW_VERBOSE

// config
#include "config.hpp"

// include
#include "detail/noncopyable.hpp"
#include "request.hpp"

namespace rpc_core {

class dispose : detail::noncopyable {
 public:
  static std::shared_ptr<dispose> create() {
    return std::make_shared<dispose>();
  }

 public:
  void add(const request_s& request) {
    RPC_CORE_LOGV("add: ptr:%p", request.get());
    requests_.push_back(request_w{request});
  }

  void remove(const request_s& request) {
    RPC_CORE_LOGV("remove: ptr:%p", request.get());
    auto iter = std::remove_if(requests_.begin(), requests_.end(), [&](request_w& param) {
      auto r = param.lock();
      if (!r) return true;
      return r == request;
    });
    requests_.erase(iter, requests_.end());
  }

  void dismiss() {
    // Cancel callbacks may remove requests, add new ones or dismiss again.
    std::vector<request_w> pending;
    pending.swap(requests_);
    std::unordered_set<const request*> canceled;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    std::exception_ptr failure;
#endif
    for (const auto& item : pending) {
      auto r = item.lock();
      if (r && canceled.insert(r.get()).second) {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
          r->cancel();
        } catch (...) {
          // Finish the detached batch before propagating a user callback failure.
          if (!failure) failure = std::current_exception();
        }
#else
        r->cancel();
#endif
      }
    }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    if (failure) std::rethrow_exception(failure);
#endif
  }

  ~dispose() {
    RPC_CORE_LOGD("~dispose: size:%zu", requests_.size());
    dismiss();
  }

 private:
  std::vector<request_w> requests_;
};

using dispose_s = std::shared_ptr<dispose>;

}  // namespace rpc_core
