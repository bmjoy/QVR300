//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#include "Classes/SnapdragonVRControllerEventManager.h"
#include "SnapdragonVRController.h"
#include "SnapdragonVRControllerPrivate.h"

static USnapdragonVRControllerEventManager* Singleton = nullptr;

USnapdragonVRControllerEventManager::USnapdragonVRControllerEventManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USnapdragonVRControllerEventManager* USnapdragonVRControllerEventManager::GetInstance()
{
	if (!Singleton)
	{
		Singleton = NewObject<USnapdragonVRControllerEventManager>();
		Singleton->AddToRoot();
	}
	return Singleton;
}