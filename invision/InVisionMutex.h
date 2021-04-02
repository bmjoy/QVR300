#ifndef SC_MUTEX_H
#define SC_MUTEX_H

#include <pthread.h>
#include <errno.h>

namespace ShadowCreator {
    class InVisionMutex final {
    public:
        InVisionMutex(bool bRecursive = false);

        ~InVisionMutex();

        bool lock(bool bBlocking = true);

        void unlock();

        pthread_mutex_t *getPThreadMutex();

        class AutoLock {
        public:
            inline AutoLock(InVisionMutex &mutex) : mLock(mutex) { mLock.lock(); }

            inline AutoLock(InVisionMutex *mutex) : mLock(*mutex) { mLock.lock(); }

            inline ~AutoLock() { mLock.unlock(); }

        private:
            InVisionMutex &mLock;
        };

    private:
        pthread_mutex_t mMutex;
    };
}

#endif