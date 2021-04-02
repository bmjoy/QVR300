//
// Created by willie on 7/4/2019.
//

#include "VeraSignal.h"

namespace Vera {

    VeraSignal::VeraSignal(bool bAutoReset) : mBAutoReset(bAutoReset) {
        mBAutoReset = bAutoReset;
        mBSignaled = false;
    }

    bool VeraSignal::waitSignal(uint64_t timeoutNanoseconds) {
        bool released;
        std::unique_lock<std::mutex> autoLock(mMutex);
        if (mBSignaled) {
            released = true;
        } else {
            if (timeoutNanoseconds == SIGNAL_TIMEOUT_INFINITE) {
                mCondition.wait(autoLock, [this] { return mBSignaled; });
            } else if (timeoutNanoseconds > 0) {
                mCondition.wait_for(autoLock, std::chrono::nanoseconds(timeoutNanoseconds),
                                    [this] { return mBSignaled; });
            }
            released = mBSignaled;
        }
        if (released && mBAutoReset) {
            mBSignaled = false;
        }
        return released;
    }

    void VeraSignal::raiseSignal() {
        std::unique_lock<std::mutex> autoLock(mMutex);
        mBSignaled = true;
        mCondition.notify_one();
    }

    void VeraSignal::clearSignal() {
        std::unique_lock<std::mutex> autoLock(mMutex);
        mBSignaled = false;
    }

    bool VeraSignal::isSignaled() {
        std::unique_lock<std::mutex> autoLock(mMutex);
        return mBSignaled;
    }

}