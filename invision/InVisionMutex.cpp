#include "InVisionMutex.h"

namespace ShadowCreator {
    InVisionMutex::InVisionMutex(bool bRecursive) {
        if (bRecursive) {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&mMutex, &attr);
        } else {
            pthread_mutex_init(&mMutex, NULL);
        }
    }

    InVisionMutex::~InVisionMutex() {
        pthread_mutex_destroy(&mMutex);
    }

    bool InVisionMutex::lock(bool bBlocking) {
        if (EBUSY == pthread_mutex_trylock(&mMutex)) {
            if (!bBlocking) {
                return false;
            }
            pthread_mutex_lock(&mMutex);
        }
        return true;
    }

    void InVisionMutex::unlock() {
        pthread_mutex_unlock(&mMutex);
    }

    pthread_mutex_t *InVisionMutex::getPThreadMutex() {
        return &mMutex;
    }
}