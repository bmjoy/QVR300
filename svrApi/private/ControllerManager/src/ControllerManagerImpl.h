//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once
#include <jni.h>

class ControllerManagerImpl
{
    private:
        static jclass clazz;
        static jmethodID methods[7];
        static bool initialized;
        
    private:
        jobject thiz;
        JavaVM* javaVM;
        
        void Initialize(JavaVM* javaVM, jobject);
        
        enum {
            CONTROLLER_MANAGER_INIT = 0,
            CONTROLLER_MANAGER_INITIALIZE = 1,
            CONTROLLER_MANAGER_START = 2,
            CONTROLLER_MANAGER_STOP = 3,
            CONTROLLER_MANAGER_QUERY = 4,
            CONTROLLER_MANAGER_SENDMESSAGE = 5,
            CONTROLLER_MANAGER_SHUTDOWN = 6
        };
        
    public:
        ControllerManagerImpl(JavaVM* javaVM, jobject context, jlong listener, jobject svrServiceClient);
        ~ControllerManagerImpl();
        void Initialize(const char* defaultControllerService, int size);
        int Start(const char* desc, int, int);
        void Stop(int);
        jobject Query(int, int);
        void SendMessage(int, int, int, int);
};
