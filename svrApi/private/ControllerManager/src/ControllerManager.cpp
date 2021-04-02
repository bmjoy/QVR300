//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include "ControllerManager.h"
#include "private/JNIHelper.h"
#include "ControllerManagerImpl.h"

//-----------------------------------------------------------------------------
ControllerManager::ControllerManager(JavaVM* javaVM, jobject context, SvrServiceClient* svrServiceClient, Svr::svrEventManager* eventManager, int qvrFd, int qvrFdSize)
{
    this->javaVM = javaVM;
    this->qvrFd = qvrFd;
    this->qvrFdSize = qvrFdSize;
    impl = new ControllerManagerImpl(javaVM, context, (jlong)this, svrServiceClient->GetJavaObject());
    this->eventManager = eventManager;
    Clear();
}

//-----------------------------------------------------------------------------
bool ControllerManager::Initialize( const char* defaultControllerService, int controllerRingBufferSize  )
{
    impl->Initialize(defaultControllerService, controllerRingBufferSize);
    return true;
}

//-----------------------------------------------------------------------------
void ControllerManager::Clear()
{
    //Set to default state
    for(int i=0;i<MAX_CONTROLLERS;i++)
    {
        listOfControllers[i] = 0;
    }
}

//-----------------------------------------------------------------------------
void ControllerManager::Shutdown()
{
    this->eventManager = 0;
    
    Clear();
    
    if( impl != 0 )
    {
        delete impl;
    }
    impl = 0;
    
    javaVM = 0;
}

//-----------------------------------------------------------------------------
ControllerManager::~ControllerManager()
{
    Shutdown();
}

//-----------------------------------------------------------------------------
int ControllerManager::ControllerQuery(int handle, int what, void* memory, unsigned int memorySize)
{
    int bytesWritten = 0;
    
    if( (memory != 0) )
    {    
        jobject byteBuffer = impl->Query(handle, what);
        if( byteBuffer != 0 )
        {
            JNIEnv* jniEnv = JNIHelper::GetJNIEnv(javaVM);
            void* bufferPtr = jniEnv->GetDirectBufferAddress(byteBuffer);
            unsigned int bufferSize  = jniEnv->GetDirectBufferCapacity(byteBuffer);
            
            if( bufferSize <= memorySize )
            {
                switch(what)
                {
                    //Battery
                    case svrControllerQueryType::kControllerQueryBatteryRemaining:
                    {
                        ::memcpy(memory, bufferPtr, 4);
                        bytesWritten = 4;
                    }
                    break;
                    case svrControllerQueryType::kControllerQueryControllerCaps:
                    {
                        ::memcpy(memory, bufferPtr, 148);
                        bytesWritten = 148;
                    }
                    break;
                }
            }
        }
        else {
            bytesWritten = 0;
        }
    }
    
    return bytesWritten;
}

//-----------------------------------------------------------------------------
int ControllerManager::ControllerStart(const char* desc)
{
    int handle = impl->Start(desc, qvrFd, qvrFdSize);

    return handle;
}

//-----------------------------------------------------------------------------
void ControllerManager::ControllerStop(int handle)
{
    if( IsValid(handle) )
    {
        listOfControllers[handle] = 0;
        impl->Stop(handle);
    }
}

//-----------------------------------------------------------------------------
void ControllerManager::ControllerSendMessage(int handle, int what, int arg1, int arg2)
{
    impl->SendMessage(handle, what, arg1, arg2);
}

//-----------------------------------------------------------------------------
bool ControllerManager::ControllerGetState(int handle, svrControllerState* data)
{
    bool result = false;

    if( IsValid(handle) )
    {
        RingBuffer<svrControllerState>* ringBuffer = listOfControllers[handle];
        if( ringBuffer != 0 )
        {
            svrControllerState* currentData = ringBuffer->get();
            if( currentData != 0x00 )
            {
                ::memcpy(data, currentData, sizeof(svrControllerState));
                result = true;
            }
        }
    }
    
    return result;
}

//-----------------------------------------------------------------------------
bool ControllerManager::IsValid(int index)
{
    bool bResult = false;
    if( (index >= 0) && (index < MAX_CONTROLLERS) )
    {
        bResult = true;    
    }
    
    return bResult;
}

//-----------------------------------------------------------------------------
void ControllerManager::CachePtr(int handle, RingBuffer<svrControllerState>* ptr)
{
    if( IsValid(handle) )
    {
        listOfControllers[handle] = ptr;
    }
}

//-----------------------------------------------------------------------------
void ControllerManager::OnControllerCallback(int what, long arg1, long arg2, jobject dataBundle)
{
    Svr::svrEventQueue* pQueue = eventManager->AcquireEventQueue();
    svrEventData data;
    data.data[0] = arg2;
    switch(what)
    {
        case CONTROLLER_MEMORY_ALLOCATED:
            CachePtr(arg1, (RingBuffer<svrControllerState>*)(arg2));
            break;
        case CONTROLLER_CONNECTED:
            pQueue->SubmitEvent(kEventControllerConnected, data);
            break;
        case CONTROLLER_CONNECTING:
            pQueue->SubmitEvent(kEventControllerConnecting, data);
            break;
        case CONTROLLER_DISCONNECTED:
            pQueue->SubmitEvent(kEventControllerDisconnected, data);
            break;
        case CONTROLLER_ERROR:
            pQueue->SubmitEvent(kEventControllerDisconnected, data);
            break;
    }
}