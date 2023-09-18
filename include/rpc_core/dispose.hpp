#pragma once

#include <algorithm>
#include <memory>
#include <vector>

// #define RPC_CORE_LOG_SHOW_VERBOSE

// config
#include "config.hpp"

// include
#include "detail/noncopyable.hpp"
#include "request.hpp"

namespace rpc_core {

class dispose : detail::noncopyable, public request::dispose_proto {
 public:
  static std::shared_ptr<dispose> create() {
    return std::make_shared<dispose>();
  }

 public:
  void add(const request_s& request) override {
    RPC_CORE_LOGV("add: ptr:%p", request.get());
    requests_.push_back(request_w{request});
  }

  void remove(const request_s& request) override {
    RPC_CORE_LOGV("remove: ptr:%p", request.get());
    auto iter = std::remove_if(requests_.begin(), requests_.end(), [&](request_w& param) {
      auto r = param.lock();
      if (!r) return true;
      return r == request;
    });
    requests_.erase(iter);
  }

  void dismiss() {
    for (const auto& item : requests_) {
      auto r = item.lock();
      if (r) {
        r->cancel();
      }
    }
    requests_.clear();
  }

  ~dispose() override {
    RPC_CORE_LOGD("~dispose: size:%zu", requests_.size());
    dismiss();
  }

 private:
  std::vector<request_w> requests_;
};

using dispose_s = std::shared_ptr<dispose>;

}  // namespace rpc_core
