#pragma once

#include <map>
#include <set>
#include <memory>
#include <string>
#include <utility>

#include "base/noncopyable.hpp"
#include "Request.hpp"
#include "MakeEvent.hpp"

namespace RpcCore {

/**
 * 用于管理取消Request
 */
class Dispose : noncopyable, public Request::DisposeProto {
    std::map<void*, std::set<SRequest>> targetRequestMap;
    MAKE_EVENT(Destroy);
public:
    std::string name_;

public:
    explicit Dispose(std::string name = "") : name_(std::move(name)) {
        LOGD("new Dispose: %p %s", this, name_.c_str());
    }
    static std::shared_ptr<Dispose> create(const std::string& name = "") {
        return std::make_shared<Dispose>(name);
    }

public:
    SRequest addRequest(SRequest request) override {
        LOGV("addRequest: ptr:%p target:%p", request.get(), request->target());
        targetRequestMap[request->target()].insert(std::move(request));
        return request;
    }

    SRequest rmRequest(SRequest request) override {
        LOGV("rmRequest: ptr:%p target:%p", request.get(), request->target());
        auto it = targetRequestMap.find(request->target());
        if (it == targetRequestMap.cend()) return request;
        auto& rs = it->second;
        it->second.erase(rs.find(request));
        return request;
    }

    void cancelTarget(void* target) {
        auto it = targetRequestMap.find(target);
        if (it == targetRequestMap.cend()) return;
        for (const auto& request : it->second) {
            if (request->target() == target) {
                request->cancel(true);
            }
        }
        targetRequestMap.erase(it);
    }

    void cancelAll() {
        for(auto& m : targetRequestMap) {
            for (const auto& request : m.second) {
                request->cancel(true);
            }
            m.second.clear();
        }
        targetRequestMap.clear();
    }

    size_t getRequestSize() {
        size_t sum = 0;
        for(const auto& m : targetRequestMap) {
            sum += m.second.size();
        }
        return sum;
    }

    // RAII
    ~Dispose() override {
        LOGD("~Dispose: size:%zu \t%s", getRequestSize(), name_.c_str());
        cancelAll();
        onDestroy();
    }
};

using SDispose = std::shared_ptr<Dispose>;

}
