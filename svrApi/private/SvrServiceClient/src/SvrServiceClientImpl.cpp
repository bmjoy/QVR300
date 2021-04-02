//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include "SvrServiceClientImpl.h"
#include "private/JNIHelper.h"

jclass SvrServiceClientImpl::clazz;
jmethodID SvrServiceClientImpl::methods[3];
bool SvrServiceClientImpl::initialized = false;

//-----------------------------------------------------------------------------
void SvrServiceClientImpl::Initialize(JavaVM* javaVM, jobject context)
//-----------------------------------------------------------------------------
{
    this->javaVM = javaVM;
    if( initialized == false )
    {
        JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
        clazz = JNIHelper::LoadClass(javaVM, context, "com/qualcomm/svrapi/SvrServiceClient");        
        methods[SVRSERVICE_CLIENT_INIT] = jniEnv->GetMethodID(clazz, "<init>", "(Landroid/content/Context;J)V");
        methods[SVRSERVICE_CLIENT_CONNECT] = jniEnv->GetMethodID(clazz, "Connect", "()V");
        methods[SVRSERVICE_CLIENT_DISCONNECT] = jniEnv->GetMethodID(clazz, "Disconnect", "()V");
        initialized = true;
    }
}

//-----------------------------------------------------------------------------
SvrServiceClientImpl::SvrServiceClientImpl(JavaVM* javaVM, jobject context, jlong nativeHandle)
//-----------------------------------------------------------------------------
{
    Initialize(javaVM, context);
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    thiz = jniEnv->NewGlobalRef(jniEnv->NewObject(clazz, methods[SVRSERVICE_CLIENT_INIT], context, nativeHandle));
}

//-----------------------------------------------------------------------------
SvrServiceClientImpl::~SvrServiceClientImpl()
//-----------------------------------------------------------------------------
{
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jniEnv->DeleteGlobalRef(thiz);
}

//-----------------------------------------------------------------------------
jobject SvrServiceClientImpl::GetJavaObject()
//-----------------------------------------------------------------------------
{
    return thiz;
}

//-----------------------------------------------------------------------------
void SvrServiceClientImpl::Connect()
//-----------------------------------------------------------------------------
{
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jniEnv->CallVoidMethod(thiz, methods[SVRSERVICE_CLIENT_CONNECT]);
}

//-----------------------------------------------------------------------------
void SvrServiceClientImpl::Disconnect()
//-----------------------------------------------------------------------------
{
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jniEnv->CallVoidMethod(thiz, methods[SVRSERVICE_CLIENT_DISCONNECT]);
}
