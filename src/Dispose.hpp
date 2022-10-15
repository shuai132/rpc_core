#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//#define RpcCore_LOG_SHOW_VERBOSE

#include "Request.hpp"
#include "detail/noncopyable.hpp"

namespace RpcCore {

/**
 * 用于自动取消Request
 */
class Dispose : noncopyable, public Request::DisposeProto {
 public:
  explicit Dispose(std::string name = "") : name_(std::move(name)) {
    RpcCore_LOGD("new Dispose: %p %s", this, name_.c_str());
  }
  static std::shared_ptr<Dispose> create(const std::string& name = "") {
    return std::make_shared<Dispose>(name);
  }

 public:
  void add(const SRequest& request) override {
    RpcCore_LOGV("add: ptr:%p", request.get());
    requests_.push_back(WRequest{request});
  }

  void remove(const SRequest& request) override {
    RpcCore_LOGV("remove: ptr:%p", request.get());
    auto iter = std::remove_if(requests_.begin(), requests_.end(), [&](WRequest& param) {
      auto r = param.lock();
      if (!r) return true;
      return r == request;
    });
    requests_.erase(iter);
  }

  void dispose() {
    for (const auto& item : requests_) {
      auto r = item.lock();
      if (r) {
        r->cancel();
      }
    }
    requests_.clear();
  }

  ~Dispose() override {
    RpcCore_LOGD("~Dispose: size:%zu \t%s", requests_.size(), name_.c_str());
    dispose();
  }

 private:
  std::vector<WRequest> requests_;
  std::string name_;
};

using SDispose = std::shared_ptr<Dispose>;

}  // namespace RpcCore
