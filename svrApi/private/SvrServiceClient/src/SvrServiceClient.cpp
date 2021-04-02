//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include "SvrServiceClient.h"
#include <jni.h>
#include "SvrServiceClientImpl.h"

//-----------------------------------------------------------------------------
SvrServiceClient::SvrServiceClient(JavaVM* javaVM, jobject context)
//-----------------------------------------------------------------------------
{
    impl = new SvrServiceClientImpl(javaVM, context, (jlong)(this));
}

//-----------------------------------------------------------------------------
SvrServiceClient::~SvrServiceClient()
//-----------------------------------------------------------------------------
{
    if( impl != 0 )
    {
        delete impl;
        impl = 0;
    }
}

//-----------------------------------------------------------------------------
jobject SvrServiceClient::GetJavaObject()
//-----------------------------------------------------------------------------
{
    return impl->GetJavaObject();
}

// Connect/Disconnect
//-----------------------------------------------------------------------------
void SvrServiceClient::Connect()
//-----------------------------------------------------------------------------
{
    impl->Connect();
}

//-----------------------------------------------------------------------------
void SvrServiceClient::Disconnect()
//-----------------------------------------------------------------------------
{
    impl->Disconnect();
}

//-----------------------------------------------------------------------------
void SvrServiceClient::OnServiceEvent(int what, int arg1, int arg2, jobject bundle)
//-----------------------------------------------------------------------------
{
    //TODO: add events to svr eventManager
    switch(what)
    {
        case SVRSERVICE_EVENT_CONNECTED:
            break;
        case SVRSERVICE_EVENT_DISCONNECTED:
            break;
    }
}
