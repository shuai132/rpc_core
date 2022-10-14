#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

//#define RpcCore_LOG_SHOW_VERBOSE

#include "Request.hpp"
#include "detail/noncopyable.hpp"

namespace RpcCore {

/**
 * 用于管理取消Request
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
  void addRequest(SRequest request) override {
    RpcCore_LOGV("addRequest: ptr:%p", request.get());
    requests_.insert(std::move(request));
  }

  void rmRequest(SRequest request) override {
    RpcCore_LOGV("rmRequest: ptr:%p", request.get());
    requests_.erase(request);
  }

  void dispose() {
    for (const auto& item : requests_) {
      item->cancel();
    }
    requests_.clear();
  }

  // RAII
  ~Dispose() override {
    RpcCore_LOGD("~Dispose: size:%zu \t%s", requests_.size(), name_.c_str());
    dispose();
  }

 private:
  std::set<SRequest> requests_;
  std::string name_;
};

using SDispose = std::shared_ptr<Dispose>;

}  // namespace RpcCore
