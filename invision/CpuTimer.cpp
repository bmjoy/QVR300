//
// Created by willie on 2018/9/10.
//

#include "CpuTimer.h"

namespace ShadowCreator {
    CpuTimer *CpuTimer::getInstance() {
        static CpuTimer cpuTimer;
        return &cpuTimer;
    }

    Nanoseconds CpuTimer::getNanoTimestamp() {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (Nanoseconds) now.tv_sec * 1000ULL * 1000ULL * 1000ULL + now.tv_nsec;
    }

    void CpuTimer::sleep(Nanoseconds sleepTimeNano) {
        timespec t, rem;
        t.tv_sec = 0;
        t.tv_nsec = sleepTimeNano;
        nanosleep(&t, &rem);
    }
}