//
// Created by willie on 2018/9/10.
//

#ifndef VERA_CPU_TIMER_H
#define VERA_CPU_TIMER_H

#include <cstdint>
#include <inttypes.h>

namespace Vera {
    class CpuTimer {
    public:
        static CpuTimer *getInstance();

        uint64_t getNanoTimestamp();

        void sleep(uint64_t sleepTimeNano);

    private:
        CpuTimer() {}

        ~CpuTimer() {}
    };

}


#endif //VERA_CPU_TIMER_H
