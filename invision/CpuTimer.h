//
// Created by willie on 2018/9/10.
//

#ifndef SC_CPU_TIMER_H
#define SC_CPU_TIMER_H

#include <Common.h>

namespace ShadowCreator {
    class CpuTimer {
    public:
        static CpuTimer *getInstance();

        Nanoseconds getNanoTimestamp();

        void sleep(Nanoseconds sleepTimeNano);

    private:
        CpuTimer() {}

        ~CpuTimer() {}
    };

    class AutoCpuTimer {
    public:
        inline AutoCpuTimer(const char *name, CpuTimer *cpuTimer) : mName(name),
                                                                    mCpuTimer(cpuTimer) {
            mStartNanos = mCpuTimer->getNanoTimestamp();
        }

        inline ~AutoCpuTimer() {
            Nanoseconds endNanos = mCpuTimer->getNanoTimestamp();
            Nanoseconds duration = endNanos - mStartNanos;
            WLOGD("%s duration = %"
                         PRIu64
                         "", mName, duration);
        }

    private:
        const char *mName;
        CpuTimer *mCpuTimer;
        Nanoseconds mStartNanos;
    };

}


#endif //SC_CPU_TIMER_H
