#pragma once

#include <map>
#include <set>
#include <memory>

#include "base/noncopyable.hpp"
#include "Request.hpp"
#include "MakeEvent.hpp"

namespace RpcCore {

class Dispose : noncopyable, public Request::DisposeProto {
    std::map<void*, std::set<SRequest>> targetRequestMap;
    MAKE_EVENT(Destroy);

public:
    static std::shared_ptr<Dispose> create() {
        return std::make_shared<Dispose>();
    }

public:
    SRequest addRequest(SRequest request) override {
        targetRequestMap[request->target()].insert(std::move(request));
        return request;
    }

    SRequest rmRequest(SRequest request) override {
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
        LOGD("~Dispose: will cancel: %zu", getRequestSize());
        cancelAll();
        onDestroy();
    }
};

using SDispose = std::shared_ptr<Dispose>;

}
