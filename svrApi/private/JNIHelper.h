//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once
#include<jni.h>

namespace JNIHelper {
    JNIEnv* GetJNIEnv(JavaVM* javaVM);
    jclass LoadClass(JavaVM* javaVM, jobject context, const char* className);
}