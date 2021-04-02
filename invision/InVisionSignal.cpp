#include "InVisionSignal.h"

namespace ShadowCreator {
    InVisionSignal::InVisionSignal(bool bAutoReset) {
        pthread_cond_init(&mCondition, NULL);
        mThreadWaitingCount = 0;
        mBAutoReset = bAutoReset;
        mBSignaled = false;
    }

    InVisionSignal::~InVisionSignal() {
        pthread_cond_destroy(&mCondition);
    }

    bool InVisionSignal::isSignaled() {
        return mBSignaled;
    }

    bool InVisionSignal::waitSignal(Nanoseconds timeoutNanoseconds) {
        bool released = false;
        {
            InVisionMutex::AutoLock autoLock(mInVisionMutex);
            if (mBSignaled) {
                released = true;
            } else {
                mThreadWaitingCount++;
                if (timeoutNanoseconds == SIGNAL_TIMEOUT_INFINITE) {
                    do {
                        pthread_cond_wait(&mCondition, mInVisionMutex.getPThreadMutex());
                        // Must re-check condition because pthread_cond_wait may spuriously wake up.
                    } while (mBSignaled == false);
                } else if (timeoutNanoseconds > 0) {
                    struct timeval tp;
                    gettimeofday(&tp, NULL);
                    struct timespec ts;
                    ts.tv_sec = (time_t) (tp.tv_sec + timeoutNanoseconds / (1000 * 1000 * 1000));
                    ts.tv_nsec = (long) (tp.tv_usec + (timeoutNanoseconds % (1000 * 1000 * 1000)));
                    do {
                        if (ETIMEDOUT ==
                            pthread_cond_timedwait(&mCondition, mInVisionMutex.getPThreadMutex(),
                                                   &ts)) {
                            break;
                        }
                        // Must re-check condition because pthread_cond_timedwait may spuriously wake up.
                    } while (mBSignaled == false);
                }
                released = mBSignaled;
                mThreadWaitingCount--;
            }
            if (released && mBAutoReset) {
                mBSignaled = false;
            }
        }

        return released;
    }

    void InVisionSignal::raiseSignal() {
        InVisionMutex::AutoLock autoLock(mInVisionMutex);
        mBSignaled = true;
        if (mThreadWaitingCount > 0) {
            pthread_cond_broadcast(&mCondition);
        }
    }

    void InVisionSignal::clearSignal() {
        InVisionMutex::AutoLock autoLock(mInVisionMutex);
        mBSignaled = false;
    }
}