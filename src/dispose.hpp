#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// #define RPC_CORE_LOG_SHOW_VERBOSE

// config
#include "config.hpp"

// include
#include "detail/noncopyable.hpp"
#include "request.hpp"

namespace RPC_CORE_NAMESPACE {

class dispose : detail::noncopyable, public request::dispose_proto {
 public:
  explicit dispose(std::string name = "") : name_(std::move(name)) {
    RPC_CORE_LOGD("new dispose: %p %s", this, name_.c_str());
  }
  static std::shared_ptr<dispose> create(const std::string& name = "") {
    return std::make_shared<dispose>(name);
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
    RPC_CORE_LOGD("~dispose: size:%zu \t%s", requests_.size(), name_.c_str());
    dismiss();
  }

 private:
  std::vector<request_w> requests_;
  std::string name_;
};

using dispose_s = std::shared_ptr<dispose>;

}  // namespace RPC_CORE_NAMESPACE
