#pragma once

#include <mutex>

namespace Vera {
    class VeraSignal final {
    public:
        VeraSignal(bool bAutoReset);

        ~VeraSignal() = default;

        bool waitSignal(uint64_t timeoutNanoseconds);

        void raiseSignal();

        void clearSignal();

        bool isSignaled();

    public:
        static const uint64_t SIGNAL_TIMEOUT_INFINITE = 0xFFFFFFFFFFFFFFFFULL;

    private:
        std::mutex mMutex;
        bool mBAutoReset;
        bool mBSignaled;
        std::condition_variable mCondition;
    };

}
