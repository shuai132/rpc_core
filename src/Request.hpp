#pragma once

#include <memory>
#include <utility>

#ifdef RpcCore_THREAD_SUPPORT
#include <mutex>
#endif

#include "base/noncopyable.hpp"

namespace RpcCore {

#define RpcCore_Request_MAKE_PROP(type, name) \
        struct {}; /* NOLINT */ \
        private: \
        type name##_; \
        public: \
        type name() { \
            return name##_; \
        } \
        std::shared_ptr<Request> name(type name) { \
            name##_ = std::move(name); \
            return shared_from_this(); \
        }

struct Request : noncopyable, public std::enable_shared_from_this<Request> {
    using SRequest = std::shared_ptr<Request>;

    class RpcProto {
    public:
        virtual SeqType makeSeq() {
            assert(false);
            return SeqType{};
        };
        virtual void sendRequest(const std::shared_ptr<Request>&) {
            assert(false);
        }
    };

    enum class FinishType {
        NORMAL,
        TIMEOUT,
        CANCELED,
    };


    RpcCore_Request_MAKE_PROP(std::shared_ptr<RpcProto>, rpc);
    RpcCore_Request_MAKE_PROP(CmdType, cmd);
    RpcCore_Request_MAKE_PROP(void*, target);
    RpcCore_Request_MAKE_PROP(SeqType, seq);
    RpcCore_Request_MAKE_PROP(MsgDispatcher::RspHandle, rspHandle);
    RpcCore_Request_MAKE_PROP(uint32_t, timeoutMs);
    RpcCore_Request_MAKE_PROP(bool, canceled);
    RpcCore_Request_MAKE_PROP(std::function<void(FinishType)>, finishCb);
    RpcCore_Request_MAKE_PROP(std::string, payload);

public:

    std::function<void()> timeoutCb_;
    std::shared_ptr<Request> timeoutCb(std::function<void()> timeoutCb) {
#if _LIBCPP_STD_VER >= 14
        timeoutCb_ = [this, timeoutCb = std::move(timeoutCb)]{
            if (timeoutCb) {
                timeoutCb();
            }
            onFinish(FinishType::TIMEOUT);
        };
#else
        timeoutCb_ = std::bind([this](const std::function<void()>& timeoutCb){
            if (timeoutCb) {
                timeoutCb();
            }
            onFinish(FinishType::TIMEOUT);
        }, std::move(timeoutCb));
#endif
        return shared_from_this();
    }

private:
    FinishType finishType_;
    void onFinish(FinishType type) {
        finishType_ = type;
        if (finishCb_) {
            finishCb_(finishType_);
        }
        notify();
    }

    inline void notify() {
#ifdef RpcCore_THREAD_SUPPORT
        cond_.notify_one();
#endif
    }
public:
    Request() = default;
    Request(std::shared_ptr<RpcProto> rpc) : rpc_(std::move(rpc)) {}

public:
    void init() {
        timeoutMs(3000);
        target(nullptr);
        canceled(false);
        timeoutCb(nullptr);
        inited_ = true;
    }

    SRequest call(std::shared_ptr<RpcProto> rpc = nullptr) {
        assert(inited_);
        if (rpc) rpc_ = std::move(rpc);
        seq_ = rpc_->makeSeq();
        rpc_->sendRequest(shared_from_this());
        return shared_from_this();
    }

    SRequest cancel() {
        assert(inited_);
        canceled(true);
        onFinish(FinishType::CANCELED);
        return shared_from_this();
    }

    template<typename T>
    SRequest setCb(std::function<void(T&&)> cb) {
        this->rspHandle_ = [this, cb](MsgWrapper msg){
            if (canceled()) {
                return true;
            }

            auto rsp = msg.unpackAs<T>();
            if (rsp.first) {
                cb(std::forward<T>(rsp.second));
                onFinish(FinishType::NORMAL);
                return true;
            }
            return false;
        };
        return shared_from_this();
    }

    SRequest setCb(std::function<void()> cb) {
        this->rspHandle_ = [this, cb](const MsgWrapper&){
            cb();
            notify();
            return true;
        };
        return shared_from_this();
    }
    SRequest setMessage(const Message& message) {
        this->payload(message.serialize());
        return shared_from_this();
    }

    static SRequest MakeRequest(std::shared_ptr<Request::RpcProto> rpc) {
        auto request = std::make_shared<Request>(rpc);
        request->init();
        return request;
    }

private:
    bool inited_ = false;

#ifdef RpcCore_THREAD_SUPPORT
private:
    std::mutex mutex_;
    std::condition_variable cond_;
public:
    void wait() {
        std::unique_lock<std::mutex> guard(mutex_);
        cond_.wait(guard);
    }
#endif
};
using SMessage = std::shared_ptr<Message>;
using SRequest = std::shared_ptr<Request>;
using FinishType = Request::FinishType;

}
