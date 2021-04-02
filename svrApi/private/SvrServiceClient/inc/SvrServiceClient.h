//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once
#include <jni.h>

class SvrServiceClientImpl;

/**
  * SvrServiceClient
  *
  */
class SvrServiceClient {
    private:
        int versionNumber;
        SvrServiceClientImpl* impl;
    public:
        enum {
            SVRSERVICE_EVENT_CONNECTED       = 0x600,
            SVRSERVICE_EVENT_DISCONNECTED    = 0x601
        };
        SvrServiceClient(JavaVM*, jobject);
        ~SvrServiceClient();
    public:
        void Initialize();
        jobject GetJavaObject();
    public:
        void Connect();
        void Disconnect();
        void OnServiceEvent(int what, int arg1, int arg2, jobject bundle);
};