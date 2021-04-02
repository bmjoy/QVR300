#ifndef SC_THREAD_H
#define SC_THREAD_H

#include <string>
#include <pthread.h>
#include "InVisionMutex.h"
#include "InVisionSignal.h"


namespace ShadowCreator {
    static const int THREAD_AFFINITY_BIG_CORES = -1;

    typedef void (*ThreadFunction)(void *data);

    class InVisionThread final {
    public:
//        InVisionThread();
//
        ~InVisionThread();

        bool createThreadWithPriority(std::string threadName, int priority,
                                      ThreadFunction threadFunction, void *threadData);

        void setCurrThreadAffinity(int mask);

        void setThreadName(const char *threadName);

        void signalThread();

        void destroyThread();

        bool isThreadRunning();

        pid_t getThreadID();

        void setThreadID(int tid);


    private:
        static void *threadFunctionInternal(void *data);

    private:
        /**
         * set new create thread stack size 512KB
         */
        static const int THREAD_STACK_SIZE = 524288;

    private:
        pthread_t mThreadHandle;
        std::string mThreadName;
        void *mThreadData = NULL;
        ThreadFunction mThreadFunction;
        InVisionSignal *mPWorkDoneSignal = NULL;
        InVisionSignal *mPWorkAvailableSignal = NULL;
//        InVisionMutex *mPWorkMutex = NULL;
        volatile bool mBThreadTerminated = true;
        pid_t mThreadID;
    };
}

#endif