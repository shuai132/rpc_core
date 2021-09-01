#pragma once

#include <map>
#include <set>
#include <memory>
#include <string>
#include <utility>

#include "base/noncopyable.hpp"
#include "Request.hpp"

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
        RpcCore_LOGV("addRequest: ptr:%p target:%p", request.get(), request->target());
        targetRequestMap_[request->target()].insert(std::move(request));
    }

    void rmRequest(SRequest request) override {
        RpcCore_LOGV("rmRequest: ptr:%p target:%p", request.get(), request->target());
        auto it = targetRequestMap_.find(request->target());
        if (it == targetRequestMap_.cend()) return;
        auto& rs = it->second;
        it->second.erase(rs.find(request));
    }

    void cancelTarget(void* target) {
        auto it = targetRequestMap_.find(target);
        if (it == targetRequestMap_.cend()) return;
        for (const auto& request : it->second) {
            if (request->target() == target) {
                request->cancel(true);
            }
        }
        targetRequestMap_.erase(it);
    }

    void cancelAll() {
        for(auto& m : targetRequestMap_) {
            for (const auto& request : m.second) {
                request->cancel(true);
            }
            m.second.clear();
        }
        targetRequestMap_.clear();
    }

    size_t getRequestSize() {
        size_t sum = 0;
        for(const auto& m : targetRequestMap_) {
            sum += m.second.size();
        }
        return sum;
    }

    // RAII
    ~Dispose() override {
        RpcCore_LOGD("~Dispose: size:%zu \t%s", getRequestSize(), name_.c_str());
        cancelAll();
    }

private:
  std::map<void*, std::set<SRequest>> targetRequestMap_;
  std::string name_;
};

using SDispose = std::shared_ptr<Dispose>;

}
