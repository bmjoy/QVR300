//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include <jni.h>
#include "SvrServiceClient.h"
#include <android/log.h>

//-----------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL Java_com_qualcomm_svrapi_SvrServiceClient_OnServiceCallback(JNIEnv *, jobject, jlong nativeHandle, int what, int arg1, int arg2, jobject msg)
//-----------------------------------------------------------------------------
{
    SvrServiceClient* svrServiceClient = (SvrServiceClient*)(nativeHandle);
    if( svrServiceClient != 0 )
    {
        svrServiceClient->OnServiceEvent(what, arg1, arg2, msg);
    }
}
