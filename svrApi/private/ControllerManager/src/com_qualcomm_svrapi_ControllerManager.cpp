//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include <jni.h>
#include "ControllerManager.h"

//-----------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL Java_com_qualcomm_svrapi_controllers_ControllerManager_OnControllerCallback(JNIEnv *, jobject, jlong nativeHandle, jint what, jlong arg1, jlong arg2, jobject msg)
//-----------------------------------------------------------------------------
{
    ControllerManager* svrServiceClient = (ControllerManager*)(nativeHandle);
    if( svrServiceClient != 0 )
    {
        svrServiceClient->OnControllerCallback(what, arg1, arg2, msg);
    }
}
