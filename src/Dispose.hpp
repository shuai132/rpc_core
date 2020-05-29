#pragma once

#include "base/noncopyable.hpp"
#include "Request.hpp"
#include "MakeEvent.hpp"

namespace RpcCore {

class Dispose : noncopyable, public Request::DisposeProto {
    std::map<void*, std::vector<SRequest>> targetRequestMap;
    MAKE_EVENT(Destroy);

public:
    static std::shared_ptr<Dispose> create() {
        return std::make_shared<Dispose>();
    }

public:
    SRequest addRequest(SRequest request) override {
        targetRequestMap[request->target()].emplace_back(std::move(request));
        return request;
    }

    SRequest rmRequest(SRequest request) override {
        auto it = targetRequestMap.find(request->target());
        if (it == targetRequestMap.cend()) return request;
        auto& vector = it->second;
        for (auto r = vector.begin(); r != vector.cend();) {
            if (*r == request) {
                vector.erase(r);
            } else {
                r++;
            }
        }
        return request;
    }

    void cancelTarget(void* target) {
        auto it = targetRequestMap.find(target);
        if (it == targetRequestMap.cend()) return;
        for (const auto& request : it->second) {
            if (request->target() == target) {
                request->cancel();
            }
        }
        targetRequestMap.erase(it);
    }

    void cancelAll() {
        for(auto& m : targetRequestMap) {
            for (const auto& request : m.second) {
                request->cancel();
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
        // cancelAll(); // 自动就析构了
        onDestroy();
    }
};

using SDispose = std::shared_ptr<Dispose>;

}
