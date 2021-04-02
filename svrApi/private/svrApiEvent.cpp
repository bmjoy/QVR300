//=============================================================================
// FILE: svrApiEvent.h
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#include <algorithm>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#include "svrConfig.h"
#include "svrUtil.h"

#include "private/svrApiEvent.h"

VAR(bool, gLogEvents, false, kVariableNonpersistent);               //Enables logcat logging of events

namespace Svr
{
    //-----------------------------------------------------------------------------
    float GetEventTimeStamp(timespec& baseTime)
    //-----------------------------------------------------------------------------
    {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        
        float retVal = ((float)(spec.tv_sec - baseTime.tv_sec) + ( (float)(spec.tv_nsec - baseTime.tv_nsec) * 1e-9));
        return retVal;

    }

    //-----------------------------------------------------------------------------
    bool CompareEvents(const svrEvent& lhs, const svrEvent& rhs)
    //-----------------------------------------------------------------------------
    {
        return lhs.eventTimeStamp < rhs.eventTimeStamp;
    }

    //-----------------------------------------------------------------------------
    svrEventQueue::svrEventQueue(size_t capacity, timespec baseTime)
    //-----------------------------------------------------------------------------
    {
        mpEventQueue = new spsc_bounded_queue_t<svrEvent>(capacity);
        mThreadId = gettid();
        mBaseTime = baseTime;
    }

    //-----------------------------------------------------------------------------
    svrEventQueue::~svrEventQueue()
    //-----------------------------------------------------------------------------
    {
        delete mpEventQueue;
        mThreadId = -1;
    }

    //-----------------------------------------------------------------------------
    void svrEventQueue::SubmitEvent(svrEventType eventType, svrEventData& eventData, unsigned int deviceId)
    //-----------------------------------------------------------------------------
    {
        svrEvent evt;
        evt.eventType = eventType;
        evt.eventData = eventData;
        evt.deviceId = deviceId;
        evt.eventTimeStamp = GetEventTimeStamp(mBaseTime);
        if (!mpEventQueue->enqueue(evt))
        {
            LOGE("Event enqueue failed, out of space. (thread = %d)", mThreadId);
        }
    }

    //-----------------------------------------------------------------------------
    svrEventManager::svrEventManager()
    //-----------------------------------------------------------------------------
    {
        for (int i = 0; i < MAX_STAGING_EVENT_QUEUES; i++)
        {
            mpStagingEventQueues[i] = NULL;
        }

        clock_gettime(CLOCK_REALTIME, &mBaseTime);
        mAppEventQueue = new svrEventQueue(MAX_STAGING_EVENT_QUEUES * MAX_EVENTS_PER_QUEUE, mBaseTime);
    }

    //-----------------------------------------------------------------------------
    svrEventManager::~svrEventManager()
    //-----------------------------------------------------------------------------
    {
        for (int i = 0; i < MAX_STAGING_EVENT_QUEUES; i++)
        {
            if (mpStagingEventQueues[i] != NULL)
            {
                delete mpStagingEventQueues[i];
                mpStagingEventQueues[i] = NULL;
            }
        }

        delete mAppEventQueue;
    }

    //-----------------------------------------------------------------------------
    svrEventQueue* svrEventManager::AcquireEventQueue()
    //-----------------------------------------------------------------------------
    {
        pid_t threadId = gettid();
        
        svrEventQueue* pRetQueue = FindStagingQueueByThreadId(threadId);
        if (pRetQueue == NULL)
        {
            for (int i = 0; i < MAX_STAGING_EVENT_QUEUES; i++)
            {
                if (mpStagingEventQueues[i] == NULL)
                {
                    LOGI("Creating EVENT QUEUE for thread : %d", (unsigned int)threadId);

                    mpStagingEventQueues[i] = new svrEventQueue(MAX_EVENTS_PER_QUEUE, mBaseTime);
                    mpStagingEventQueues[i]->mThreadId = threadId;

                    pRetQueue = mpStagingEventQueues[i];
                    break;
                }
            }
        }
        return pRetQueue;
    }

    //-----------------------------------------------------------------------------
    void svrEventManager::ReleaseEventQueue(svrEventQueue* pEventQueue)
    //-----------------------------------------------------------------------------
    {
        pid_t threadId = gettid();

        for (int i = 0; i < MAX_STAGING_EVENT_QUEUES; i++)
        {
            if (mpStagingEventQueues[i] == pEventQueue)
            {
                if (mpStagingEventQueues[i]->mThreadId != threadId)
                {
                    LOGE("Releasing event queue for thread %d from thread %d", mpStagingEventQueues[i]->mThreadId, threadId);
                }

                delete mpStagingEventQueues[i];
                mpStagingEventQueues[i] = NULL;
            }
        }
        
    }

    //-----------------------------------------------------------------------------
    bool svrEventManager::PollEvent(svrEvent& outEvent)
    //-----------------------------------------------------------------------------
    {
        return mAppEventQueue->mpEventQueue->dequeue(outEvent);
    }

    //-----------------------------------------------------------------------------
    void svrEventManager::ProcessEvents()
    //-----------------------------------------------------------------------------
    {
        static svrEvent frameEvents[MAX_EVENTS_PER_QUEUE * MAX_STAGING_EVENT_QUEUES];

        memset(&frameEvents[0], 0, sizeof(svrEvent) * MAX_EVENTS_PER_QUEUE * MAX_STAGING_EVENT_QUEUES);

        unsigned int evtIdx = 0;
        for (int i = 0; i < MAX_STAGING_EVENT_QUEUES; i++)
        {
            if (mpStagingEventQueues[i] != NULL)
            {
                while (mpStagingEventQueues[i]->mpEventQueue->dequeue(frameEvents[evtIdx]))
                {
                    evtIdx++;
                }
            }
        }
        if (evtIdx > 0)
        {
            std::sort(frameEvents, frameEvents + evtIdx, CompareEvents);
            for (unsigned int i = 0; i < evtIdx; i++)
            {
                mAppEventQueue->mpEventQueue->enqueue(frameEvents[i]);
                if (gLogEvents)
                {
                    LOGI("Event Type[%d], Time[%f]", frameEvents[i].eventType, frameEvents[i].eventTimeStamp);
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    svrEventQueue*  svrEventManager::FindStagingQueueByThreadId(pid_t threadId)
    //-----------------------------------------------------------------------------
    {
        for (int i = 0; i < MAX_STAGING_EVENT_QUEUES; i++)
        {
            if (mpStagingEventQueues[i] != NULL && mpStagingEventQueues[i]->mThreadId == threadId)
            {
                //Only allow one event queue per thread - if the client attempts to acquire one again then just return the existing queue
                return mpStagingEventQueues[i];
            }
        }

        return NULL;
    }

}//End namespace Svr