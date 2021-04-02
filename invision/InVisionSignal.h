#ifndef SC_SIGNAL_H
#define SC_SIGNAL_H

#include <Common.h>
#include "InVisionMutex.h"

namespace ShadowCreator {
    class InVisionSignal final {
    public:
        InVisionSignal(bool bAutoReset);

        ~InVisionSignal();

        bool isSignaled();

        bool waitSignal(Nanoseconds timeoutNanoseconds);

        void raiseSignal();

        void clearSignal();

    public:
        static const Nanoseconds SIGNAL_TIMEOUT_INFINITE = 0xFFFFFFFFFFFFFFFFULL;

    private:
        InVisionMutex mInVisionMutex;
        pthread_cond_t mCondition;
        int mThreadWaitingCount;
        bool mBAutoReset;
        bool mBSignaled;
    };
}

#endif