//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once

#include <jni.h>

/**
  *
  *
  */
class SvrServiceClientImpl {
    private:
        static jclass clazz;
        static jmethodID methods[3];
        static bool initialized;
    
    private:    
        JavaVM* javaVM;
        jobject thiz;
    
    private:
        void Initialize(JavaVM* javaVM, jobject context);
        
        enum {
            SVRSERVICE_CLIENT_INIT = 0,
            SVRSERVICE_CLIENT_CONNECT = 1,
            SVRSERVICE_CLIENT_DISCONNECT = 2
        };
    public:
        jobject GetJavaObject();
        
    public:
        SvrServiceClientImpl(JavaVM* javaVM, jobject context, jlong nativeHandle);
        ~SvrServiceClientImpl();
        void Connect();
        void Disconnect();
};
