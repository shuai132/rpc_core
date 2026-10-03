#pragma once

#include <algorithm>
#include <exception>
#include <memory>
#include <set>
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
    if (adds_until_prune_ == 0) {
      // Scan periodically, with the interval growing with the retained set.
      // An alias can share either its pointer or its owner with another request.
      // Only identical pointers with identical ownership are duplicates.
      using key = std::pair<const rpc_core::request*, request_w>;
      auto less = [](const key& a, const key& b) {
        return a.first != b.first ? std::less<const rpc_core::request*>{}(a.first, b.first)
                                  : std::owner_less<request_w>{}(a.second, b.second);
      };
      std::set<key, decltype(less)> seen(less);
      auto end = std::remove_if(requests_.begin(), requests_.end(), [&](const request_w& item) {
        auto owner = item.lock();
        return !owner || !seen.emplace(owner.get(), item).second;
      });
      requests_.erase(end, requests_.end());
      adds_until_prune_ = (std::max)(size_t{64}, requests_.size());
    }
    --adds_until_prune_;
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
    adds_until_prune_ = (std::max)(size_t{64}, requests_.size());
  }

  void dismiss() {
    // Cancel callbacks may remove requests, add new ones or dismiss again.
    std::vector<request_w> pending;
    pending.swap(requests_);
    adds_until_prune_ = 0;
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
  size_t adds_until_prune_ = 0;
};

using dispose_s = std::shared_ptr<dispose>;

}  // namespace rpc_core
