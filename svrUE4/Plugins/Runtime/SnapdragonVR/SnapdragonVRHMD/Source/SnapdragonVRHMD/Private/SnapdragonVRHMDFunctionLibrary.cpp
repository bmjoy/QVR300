//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#include "../Public/SnapdragonVRHMDFunctionLibrary.h"
#include "HMDPrivatePCH.h"
#include "Engine.h"   //PMILI: Apparently GEngine is located in Engine.h... Not sure how we can avoid receiving the MONOLITHIC warning
#include "SnapdragonVRHMD.h"

USnapdragonVRHMDFunctionLibrary::USnapdragonVRHMDFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

#if SNAPDRAGONVR_SUPPORTED_PLATFORMS
FSnapdragonVRHMD* GetSnapdragonHMD()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->GetVersionString().Contains(TEXT("SnapdragonVR")))
	{
		return static_cast<FSnapdragonVRHMD*>(GEngine->HMDDevice.Get());
	}

	return nullptr;
}
#endif//#if SNAPDRAGONVR_SUPPORTED_PLATFORMS

void USnapdragonVRHMDFunctionLibrary::SetCpuAndGpuLevelsSVR(const int32 CpuLevel, const int32 GpuLevel)
{
#if SNAPDRAGONVR_SUPPORTED_PLATFORMS
	FSnapdragonVRHMD* HMD = GetSnapdragonHMD();
	if (HMD)
	{
		HMD->SetCPUAndGPULevels(CpuLevel, GpuLevel);
	}
#endif//#if SNAPDRAGONVR_SUPPORTED_PLATFORMS
}

USnapdragonVRHMDEventManager* USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()
{
	return USnapdragonVRHMDEventManager::GetInstance();
}