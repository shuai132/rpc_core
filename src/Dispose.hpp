#pragma once

#include "base/noncopyable.hpp"
#include "Request.hpp"

namespace RpcCore {

class Dispose : noncopyable {
    std::map<void*, std::vector<SRequest>> targetRequestMap;

public:
    void addRequest(SRequest request) {
        targetRequestMap[request->target()].emplace_back(std::move(request));
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
        for(const auto& m : targetRequestMap) {
            for (const auto& request : m.second) {
                request->cancel();
            }
        }
    }

    // RAII
    ~Dispose() {
        cancelAll();
    }
};

}
