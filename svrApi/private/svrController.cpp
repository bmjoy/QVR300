//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include "svrApi.h"
#include "ControllerManager.h"
#include "svrApiCore.h"

#include <android/log.h>

using namespace Svr;

//TODO: move this 
//-----------------------------------------------------------------------------
static void ResetControllerState(svrControllerState* state)
//-----------------------------------------------------------------------------
{
    ::memset(state, 0, sizeof(svrControllerState));
    state->rotation.w = 1;
}

//-----------------------------------------------------------------------------
static void TransformControllerState(svrControllerState* state)
//-----------------------------------------------------------------------------
{
    svrHeadPoseState head = svrGetPredictedHeadPose(0);
    glm::vec3 headPos = glm::vec3(head.pose.position.x, head.pose.position.y, head.pose.position.z);
    glm::fquat headRot = glm::fquat(head.pose.rotation.w, head.pose.rotation.x, head.pose.rotation.y, head.pose.rotation.z);
    glm::vec3 ctrlPos = glm::vec3(state->position.x, state->position.y, state->position.z);
    glm::fquat ctrlRot = glm::fquat(state->rotation.w, state->rotation.x, state->rotation.y, state->rotation.z);

    ctrlPos = ctrlPos * headRot + headPos;
    ctrlRot = ctrlRot * headRot;

    state->rotation.x = ctrlRot.x;
    state->rotation.y = ctrlRot.y;
    state->rotation.z = ctrlRot.z;
    state->rotation.w = ctrlRot.w;

    state->position.x = ctrlPos.x;
    state->position.y = ctrlPos.y;
    state->position.z = ctrlPos.z;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT int svrControllerStartTracking(const char* controllerDesc)
//-----------------------------------------------------------------------------
{
    int controllerHandle = -1;
    
    if( gAppContext != 0 )
    {
        if( gAppContext->controllerManager != 0 )
        {
            //TODO: pass qvr fd and fdSize
            controllerHandle = gAppContext->controllerManager->ControllerStart(controllerDesc);
        }
    }
    
    return controllerHandle;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT svrControllerState svrControllerGetState(int controllerHandle, int controllerSpace)
//-----------------------------------------------------------------------------
{
    svrControllerState controllerState;
    
    if( (gAppContext != 0) && (gAppContext->controllerManager != 0) )
    {
        if( false == gAppContext->controllerManager->ControllerGetState(controllerHandle, &controllerState) )
        {
            ResetControllerState(&controllerState);
        }
    }
    else
    {
        ResetControllerState(&controllerState);
    }

    if (controllerSpace != 0)
        TransformControllerState(&controllerState);
    
    return controllerState;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT void svrControllerStopTracking(int controllerHandle)
//-----------------------------------------------------------------------------
{
    if( (gAppContext != 0) && (gAppContext->controllerManager != 0) )
    {
        gAppContext->controllerManager->ControllerStop(controllerHandle);
    }
}

//-----------------------------------------------------------------------------
SVRP_EXPORT void svrControllerSendMessage(int controllerHandle, int what, int arg1, int arg2)
//-----------------------------------------------------------------------------
{
    if( (gAppContext != 0) && (gAppContext->controllerManager != 0) )
    {
        gAppContext->controllerManager->ControllerSendMessage(controllerHandle, what, arg1, arg2);
    }
}

//-----------------------------------------------------------------------------
SVRP_EXPORT int svrControllerQuery(int controllerHandle, int what, void* memory, unsigned int memorySize)
//-----------------------------------------------------------------------------
{
    int result = 0;
    
    if( (gAppContext != 0) && (gAppContext->controllerManager != 0) )
    {
        result = gAppContext->controllerManager->ControllerQuery(controllerHandle, what, memory, memorySize);
    }
    
    return result;
}
