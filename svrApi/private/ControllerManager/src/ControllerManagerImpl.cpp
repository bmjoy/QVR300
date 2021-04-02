//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include "ControllerManagerImpl.h"
#include "private/JNIHelper.h"

jclass ControllerManagerImpl::clazz;
jmethodID ControllerManagerImpl::methods[7];
bool ControllerManagerImpl::initialized = false;
        
//-----------------------------------------------------------------------------
void ControllerManagerImpl::Initialize(JavaVM* javaVM, jobject context)
//-----------------------------------------------------------------------------
{
    this->javaVM = javaVM;
    if( initialized == false )
    {
        JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
        clazz = JNIHelper::LoadClass(javaVM, context, "com/qualcomm/svrapi/controllers/ControllerManager");        
        
        methods[CONTROLLER_MANAGER_INIT] = jniEnv->GetMethodID(clazz, "<init>", "(Landroid/content/Context;Lcom/qualcomm/svrapi/SvrServiceClient;J)V");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        methods[CONTROLLER_MANAGER_INITIALIZE] = jniEnv->GetMethodID(clazz, "Initialize", "(Ljava/lang/String;I)V");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        methods[CONTROLLER_MANAGER_START] = jniEnv->GetMethodID(clazz, "Start", "(Ljava/lang/String;II)I");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        methods[CONTROLLER_MANAGER_STOP] = jniEnv->GetMethodID(clazz, "Stop", "(I)V");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        methods[CONTROLLER_MANAGER_SENDMESSAGE] = jniEnv->GetMethodID(clazz, "SendMessage", "(IIII)V");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        methods[CONTROLLER_MANAGER_SHUTDOWN] = jniEnv->GetMethodID(clazz, "Shutdown", "()V");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        methods[CONTROLLER_MANAGER_QUERY] = jniEnv->GetMethodID(clazz, "Query", "(II)Ljava/nio/ByteBuffer;");
        //if( jniEnv->ExceptionOccurred() ) jniEnv->ExceptionClear();
        initialized = true;
    }
    
}

//-----------------------------------------------------------------------------
ControllerManagerImpl::ControllerManagerImpl(JavaVM* javaVM, jobject context, jlong listener, jobject svrServiceClient)
//-----------------------------------------------------------------------------
{
    Initialize(javaVM, context);
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    if( methods[CONTROLLER_MANAGER_INIT] != 0 )
    {
    thiz = jniEnv->NewGlobalRef(jniEnv->NewObject(clazz, methods[CONTROLLER_MANAGER_INIT], context, svrServiceClient, listener));
}
}

//-----------------------------------------------------------------------------
void ControllerManagerImpl::Initialize(const char* defaultControllerService, int size)
//-----------------------------------------------------------------------------
{
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    if( methods[CONTROLLER_MANAGER_INITIALIZE] != 0 )
    {
        jstring str = jniEnv->NewStringUTF(defaultControllerService);
        jniEnv->CallVoidMethod(thiz, methods[CONTROLLER_MANAGER_INITIALIZE], str, size);
    }
}

//-----------------------------------------------------------------------------
ControllerManagerImpl::~ControllerManagerImpl()
//-----------------------------------------------------------------------------
{
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    if( methods[CONTROLLER_MANAGER_SHUTDOWN] != 0 )
    {
    jniEnv->CallVoidMethod(thiz, methods[CONTROLLER_MANAGER_SHUTDOWN]);
    }
    
    if( thiz != 0 )
    {
    jniEnv->DeleteGlobalRef(thiz);
}
}

//-----------------------------------------------------------------------------
int ControllerManagerImpl::Start(const char* desc, int qvrFd, int qvrFdSize)
//-----------------------------------------------------------------------------
{
    if( methods[CONTROLLER_MANAGER_START] == 0 ) return -1;
    
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jstring description = jniEnv->NewStringUTF(desc);
    int handle = jniEnv->CallIntMethod(thiz, methods[CONTROLLER_MANAGER_START], description, qvrFd, qvrFdSize);
    return handle;
}

//-----------------------------------------------------------------------------
void ControllerManagerImpl::Stop(int controllerHandle)
//-----------------------------------------------------------------------------
{
    if( methods[CONTROLLER_MANAGER_STOP] == 0 ) return;
    
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jniEnv->CallVoidMethod(thiz, methods[CONTROLLER_MANAGER_STOP], controllerHandle);
}

//-----------------------------------------------------------------------------
void ControllerManagerImpl::SendMessage(int controllerHandle, int what, int arg1, int arg2)
//-----------------------------------------------------------------------------
{
    if( methods[CONTROLLER_MANAGER_SENDMESSAGE] == 0 ) return;
    
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jniEnv->CallVoidMethod(thiz, methods[CONTROLLER_MANAGER_SENDMESSAGE], controllerHandle, what, arg1, arg2);
}

//-----------------------------------------------------------------------------
jobject ControllerManagerImpl::Query(int controllerHandle, int what)
//-----------------------------------------------------------------------------
{
    if( methods[CONTROLLER_MANAGER_QUERY] == 0 ) return 0;
    
    JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
    jobject byteBuffer = jniEnv->CallObjectMethod(thiz, methods[CONTROLLER_MANAGER_QUERY], controllerHandle, what);
    return byteBuffer;
}
