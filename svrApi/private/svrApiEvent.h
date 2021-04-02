//=============================================================================
// FILE: svrApiEvent.h
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#ifndef _SVR_API_EVENT_H_
#define _SVR_API_EVENT_H_

#include "svrApi.h"
#include "private/svrApiSpscQueue.h"

#define MAX_STAGING_EVENT_QUEUES    16
#define MAX_EVENTS_PER_QUEUE        32

namespace Svr
{
    class svrEventManager;

    //-----------------------------------------------------------------------------
    class svrEventQueue
    {
        friend class svrEventManager;

    public:
        svrEventQueue(size_t capacity, timespec baseTime);
        ~svrEventQueue();

        void SubmitEvent(svrEventType eventType, svrEventData& eventData, unsigned int deviceId = 0);

    private:
        pid_t                               mThreadId;
        spsc_bounded_queue_t<svrEvent>*     mpEventQueue;
        timespec                            mBaseTime;
    };

    //-----------------------------------------------------------------------------
    class svrEventManager
    {
    public:
        svrEventManager();
        ~svrEventManager();

    public:
        svrEventQueue*  AcquireEventQueue();
        void            ReleaseEventQueue(svrEventQueue* pEventQueue);

        bool            PollEvent(svrEvent& outEvent);

        void            ProcessEvents();

    private:
        svrEventQueue*  FindStagingQueueByThreadId(pid_t threadId);

    private:
        svrEventQueue*  mAppEventQueue;
        svrEventQueue*  mpStagingEventQueues[MAX_STAGING_EVENT_QUEUES];
        timespec        mBaseTime;
    };

}//End namespace Svr


#endif //_SVR_API_EVENT_H_

