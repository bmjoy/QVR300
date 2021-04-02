//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once
#include <jni.h>

#include "RingBuffer.h"
#include "SvrServiceClient.h"
#include "private/svrApiEvent.h"

#define MAX_CONTROLLERS 8

class ControllerManagerImpl;

class ControllerManager {
    private:
        enum {
            CONTROLLER_DISCONNECTED      = 0x601,
            CONTROLLER_CONNECTED         = 0x602,
            CONTROLLER_CONNECTING        = 0x603,
            CONTROLLER_ERROR             = 0x604,
            
            CONTROLLER_MEMORY_ALLOCATED  = 0x699
        };
        JavaVM* javaVM;
        int qvrFd;
        int qvrFdSize;
        ControllerManagerImpl* impl;
        RingBuffer<svrControllerState>* listOfControllers[MAX_CONTROLLERS];
        Svr::svrEventManager* eventManager;
        bool IsValid(int index);
        void CachePtr(int, RingBuffer<svrControllerState>* ptr);
        void Clear();
    public:
        ControllerManager(JavaVM* javaVM, jobject context, SvrServiceClient* svrServiceClient, Svr::svrEventManager* eventManager, int qvrFd, int qvrFdSize);
        bool Initialize( const char* defaultControllerService, int controllerRingBufferSize );
        void Shutdown();
        ~ControllerManager();
    public:
        int ControllerStart(const char*);
        void ControllerStop(int handle);
        int  ControllerQuery(int handle, int what, void* memory, unsigned int memorySize);
        bool ControllerGetState(int handle, svrControllerState* data);
        void ControllerSendMessage(int handle, int what, int arg1, int arg2);
    public:
        void OnControllerCallback(int what, long arg1, long arg2, jobject dataBundle);
};