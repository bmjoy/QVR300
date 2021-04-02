//
// Created by willie on 2018/9/10.
//

#include "CpuTimer.h"
#include <time.h>

namespace Vera {
    CpuTimer *CpuTimer::getInstance() {
        static CpuTimer cpuTimer;
        return &cpuTimer;
    }

    uint64_t CpuTimer::getNanoTimestamp() {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (uint64_t) now.tv_sec * 1000ULL * 1000ULL * 1000ULL + now.tv_nsec;
    }

    void CpuTimer::sleep(uint64_t sleepTimeNano) {
        timespec t, rem;
        t.tv_sec = 0;
        t.tv_nsec = sleepTimeNano;
        nanosleep(&t, &rem);
    }
}