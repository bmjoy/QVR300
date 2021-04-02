#include "InVisionThread.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

namespace ShadowCreator {
    InVisionThread::~InVisionThread() {
        if (NULL != mPWorkDoneSignal) {
            delete mPWorkDoneSignal;
            mPWorkDoneSignal = NULL;
        }
        if (NULL != mPWorkAvailableSignal) {
            delete mPWorkAvailableSignal;
            mPWorkAvailableSignal = NULL;
        }
    }

    bool InVisionThread::createThreadWithPriority(std::string threadName, int priority,
                                                  ThreadFunction threadFunction, void *threadData) {
        mThreadName = threadName;
        mThreadFunction = threadFunction;
        mThreadData = threadData;
        mPWorkDoneSignal = new InVisionSignal(false);
        mPWorkAvailableSignal = new InVisionSignal(true);
        mBThreadTerminated = false;

        pthread_attr_t attr;
        sched_param param;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
        pthread_attr_getschedparam(&attr, &param);
        param.sched_priority = priority;
        pthread_attr_setschedparam(&attr, &param);
        int res = pthread_create(&mThreadHandle, &attr, threadFunctionInternal, this);
        if (0 != res) {
            WLOGE("InVisionThread::createThread create pthread %s failed res = %d",
                 mThreadName.c_str(), res);
            return false;
        }
        pthread_attr_destroy(&attr);
        mPWorkDoneSignal->waitSignal(InVisionSignal::SIGNAL_TIMEOUT_INFINITE);
        WLOGD("InVisionThread::createThread %s done", threadName.c_str());
        return true;
    }


    void *InVisionThread::threadFunctionInternal(void *data) {
        InVisionThread *thread = (InVisionThread *) data;
        thread->setThreadName(thread->mThreadName.c_str());
        while (true) {
            if (!thread->mPWorkAvailableSignal->waitSignal(0)) {
                thread->mPWorkDoneSignal->raiseSignal();
                WLOGD("InVisionThread::threadFunctionInternal_before_wait mPWorkAvailableSignal");
                thread->mPWorkAvailableSignal->waitSignal(InVisionSignal::SIGNAL_TIMEOUT_INFINITE);
                WLOGD("InVisionThread::threadFunctionInternal_after_wait mPWorkAvailableSignal");
            }
            WLOGD("InVisionThread::threadFunctionInternal_inside_while_mBThreadTerminated=%d",
                 thread->mBThreadTerminated);
            if (thread->mBThreadTerminated) {
                WLOGD("InVisionThread::threadFunctionInternal_before_raise_mPWorkDoneSignal");
                thread->mPWorkDoneSignal->raiseSignal();
                break;
            }
            WLOGD("InVisionThread::threadFunctionInternal_before_call_mThreadFunction");
            thread->mThreadFunction(thread->mThreadData);
        }
        return NULL;
    }

    void InVisionThread::setThreadName(const char *threadName) {
        WLOGD("InVisionThread::setThreadName_start threadName=%s", threadName);
        prctl(PR_SET_NAME, (long) threadName, 0, 0, 0);
    }

    void InVisionThread::signalThread() {
        WLOGD("InVisionThread::signalThread_start");
        mPWorkDoneSignal->clearSignal();
        mPWorkAvailableSignal->raiseSignal();
    }

    void InVisionThread::destroyThread() {
        WLOGD("InVisionThread::destroyThread_start");
        {
            mPWorkDoneSignal->clearSignal();
            mBThreadTerminated = true;
            mPWorkAvailableSignal->raiseSignal();
        }
        mPWorkDoneSignal->waitSignal(InVisionSignal::SIGNAL_TIMEOUT_INFINITE);
        WLOGD("InVisionThread::destroyThread_after mPWorkDoneSignal waitSignal");
        delete mPWorkDoneSignal;
        delete mPWorkAvailableSignal;
        mPWorkDoneSignal = NULL;
        mPWorkAvailableSignal = NULL;
        pthread_join(mThreadHandle, NULL);
        WLOGD("InVisionThread::destroyThread_done");
    }

    bool InVisionThread::isThreadRunning() {
        return !mBThreadTerminated;
    }

    pid_t InVisionThread::getThreadID() {
        return mThreadID;
    }

    void InVisionThread::setThreadID(int threadID) {
        mThreadID = threadID;
    }

    void InVisionThread::setCurrThreadAffinity(int mask) {
        if (THREAD_AFFINITY_BIG_CORES == mask) {
            mask = 0;
            unsigned int bestFrequency = 0;
            for (int i = 0; i < 16; i++) {
                unsigned int maxFrequency = 0;
                const char *files[] =
                        {
                                "scaling_available_frequencies",    // not available on all devices
                                "scaling_max_freq",                    // no user read permission on all devices
                                "cpuinfo_max_freq",                    // could be set lower than the actual max, but better than nothing
                        };
                for (int j = 0; j < ARRAY_SIZE(files); j++) {
                    char fileName[1024];
                    sprintf(fileName, "/sys/devices/system/cpu/cpu%d/cpufreq/%s", i, files[j]);
                    FILE *fp = fopen(fileName, "r");
                    if (fp == NULL) {
                        continue;
                    }
                    char buffer[1024];
                    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
                        fclose(fp);
                        continue;
                    }
                    for (int index = 0; buffer[index] != '\0';) {
                        const unsigned int frequency = atoi(buffer + index);
                        if (frequency > maxFrequency) {
                            maxFrequency = frequency;
                        }
                        while (isspace(buffer[index])) { index++; }
                        while (isdigit(buffer[index])) { index++; }
                    }
                    fclose(fp);
                    break;
                }
                if (maxFrequency == 0) {
                    break;
                }

                if (maxFrequency == bestFrequency) {
                    mask |= (1 << i);
                } else if (maxFrequency > bestFrequency) {
                    mask = (1 << i);
                    bestFrequency = maxFrequency;
                }
            }

            if (mask == 0) {
                return;
            }
        }
        // Set the thread affinity.
        pid_t pid = gettid();
        int res = syscall(__NR_sched_setaffinity, pid, sizeof(mask), &mask);
        if (res) {
            int err = errno;
            WLOGE("InVisionThread::setCurrThreadAffinity(%d) meet Error : thread=(%d) mask=0x%X err=%s(%d)\n",
                 __NR_sched_setaffinity, pid, mask, strerror(err), err);
        } else {
            WLOGD("InVisionThread::setCurrThreadAffinity successfully : Thread %d affinity 0x%02X\n",
                 pid,
                 mask);
        }
    }

}