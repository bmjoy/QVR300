//=============================================================================
// FILE: SnapdragonVRHMD.cpp
//
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================

#include "SnapdragonVRHMD.h"
#include "../Public/SnapdragonVRHMDFunctionLibrary.h"
#include "../Classes/SnapdragonVRHMDEventManager.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneViewport.h"
#include "PostProcess/PostProcessHMD.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"

#include "RunnableThread.h"

#if PLATFORM_ANDROID
/*
It appears that we must ensure that svrEndVr() (which is always called on the CPU thread) never overlaps with
svrSubmitFrame() (which is always called on the render thread) by mutexing bInVRMode in those cases.  Otherwise, we can
get mysterious deadlocks during suspend/resume (seemingly only if the phone is not plugged into USB!  Plugged in USB
phones never reproduced this deadlock).

It is equally important that we do *not* try to use this thread synchronization construct to guard against other times
bInVRMode is used (for example, at startup), or else a different mysterious deadlock on app launch occurs.  @todo NTF:
understand what's really going on here
*/
FCriticalSection FSnapdragonVRHMDCustomPresent::InVRModeCriticalSection;
FCriticalSection FSnapdragonVRHMDCustomPresent::PerfLevelCriticalSection;

#include <android_native_app_glue.h>

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif // PLATFORM_ANDROID

#include "RHIStaticStates.h"
#include "SceneViewport.h"

#include "DrawDebugHelpers.h"

#if SNAPDRAGONVR_SUPPORTED_PLATFORMS
static const TCHAR* const svrPerfLevelSystemCStr = TEXT("sys");
static const TCHAR* const svrPerfLevelMinimumCStr = TEXT("min");
static const TCHAR* const svrPerfLevelMediumCStr = TEXT("med");
static const TCHAR* const svrPerfLevelMaximumCStr = TEXT("max");

//keep these two lines in sync (eg pass the string that corresponds to the enum)
static const TCHAR* const svrPerfLevelDefaultCStr = svrPerfLevelSystemCStr;
/*static*/ enum svrPerfLevel FSnapdragonVRHMD::GetCVarSnapdragonVrPerfLevelDefault() { return kPerfSystem; }

//these variables default values can be overridden by adding a line like this to Engine\Config\ConsoleVariables.ini: svr.perfCpu=max
static TAutoConsoleVariable<FString> CVarPerfLevelCpu(TEXT("svr.perfCpu"), svrPerfLevelDefaultCStr, TEXT("Set SnapdragonVr CPU performance consumption to one of the following: sys, min, med, max.  Note that if this value is set by Blueprint, the cvar will continue to report the last value it was set to, and will not reflect the value set by Blueprint"));
static TAutoConsoleVariable<FString> CVarPerfLevelGpu(TEXT("svr.perfGpu"), svrPerfLevelDefaultCStr, TEXT("Set SnapdragonVr GPU performance consumption to one of the following: sys, min, med, max.  Note that if this value is set by Blueprint, the cvar will continue to report the last value it was set to, and will not reflect the value set by Blueprint"));

//BEG_Q3D_AUDIO_HACK
#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarQ3D_DrawSound(TEXT("q3d.drawSound"), 0, TEXT("Draws red tetrahedrons centered at each spatialized sound's position (hurts performance significantly)"));
static TAutoConsoleVariable<int32> CVarQ3D_DrawSoundName(TEXT("q3d.drawSoundName"), 0, TEXT("Draws red textual names of spatialized sounds near their positions (hurts performance dramatically)"));
static TAutoConsoleVariable<int32> CVarQ3D_LogSoundName(TEXT("q3d.logSoundName"), 0, TEXT("Logs sound names and positions of spatialized sounds each frame"));
static TAutoConsoleVariable<int32> CVarQ3D_LogSoundNumber(TEXT("q3d.logSoundNumber"), 0, TEXT("Logs the number of spatialized sounds playing on a frame"));
#endif//#if !UE_BUILD_SHIPPING
//END_Q3D_AUDIO_HACK

static bool FStringToSvrPerfLevel(enum svrPerfLevel* OutPerfLevel, const FString& InValueReceived)
{
    if (InValueReceived.Compare(FString(svrPerfLevelSystemCStr, ESearchCase::IgnoreCase)) == 0)
    {
        *OutPerfLevel = kPerfSystem;
        return true;
    }
    else if (InValueReceived.Compare(FString(svrPerfLevelMinimumCStr, ESearchCase::IgnoreCase)) == 0)
    {
        *OutPerfLevel = kPerfMinimum;
        return true;
    }
    else if (InValueReceived.Compare(FString(svrPerfLevelMediumCStr, ESearchCase::IgnoreCase)) == 0)
    {
        *OutPerfLevel = kPerfMedium;
        return true;
    }
    else if (InValueReceived.Compare(FString(svrPerfLevelMaximumCStr, ESearchCase::IgnoreCase)) == 0)
    {
        *OutPerfLevel = kPerfMaximum;
        return true;
    }
    else
    {
        return false;
    }
}
static bool SvrPerfLevelToFString(FString* const OutPerfLevelFString, const enum svrPerfLevel InPerfLevel)
{
    switch (InPerfLevel)
    {
    case kPerfSystem:
    {
        *OutPerfLevelFString = FString(svrPerfLevelSystemCStr);
        return true;
    }
    case kPerfMinimum:
    {
        *OutPerfLevelFString = FString(svrPerfLevelMinimumCStr);
        return true;
    }
    case kPerfMedium:
    {
        *OutPerfLevelFString = FString(svrPerfLevelMediumCStr);
        return true;
    }
    case kPerfMaximum:
    {
        *OutPerfLevelFString = FString(svrPerfLevelMaximumCStr);
        return true;
    }
    default:
    {
        return false;
    }
    }
}

static bool IsPerfLevelValid(const enum svrPerfLevel InPerfLevel)
{
    return InPerfLevel >= kPerfSystem && InPerfLevel <= kPerfMaximum;
}

// CTORNE ->
/*static*/ enum svrPerfLevel FSnapdragonVRHMD::PerfLevelCpuLastSet = kNumPerfLevels, FSnapdragonVRHMD::PerfLevelGpuLastSet = kNumPerfLevels;
// <- CTORNE
/*static*/ void PerfLevelLastSet(enum svrPerfLevel* const OutPerfLevel, const enum svrPerfLevel InPerfLevel)
{
	check(IsPerfLevelValid(InPerfLevel));
	*OutPerfLevel = InPerfLevel;
}
/*static*/ void FSnapdragonVRHMD::PerfLevelCpuWrite(const enum svrPerfLevel InPerfLevel)
{
	PerfLevelLastSet(&PerfLevelCpuLastSet, InPerfLevel);
}
/*static*/ void FSnapdragonVRHMD::PerfLevelGpuWrite(const enum svrPerfLevel InPerfLevel)
{
	PerfLevelLastSet(&PerfLevelGpuLastSet, InPerfLevel);
}
/*static*/ void FSnapdragonVRHMD::PerfLevelLog(const TCHAR* const InPrefix,enum svrPerfLevel InPerfLevelCpu, enum svrPerfLevel InPerfLevelGpu)
{
#if !UE_BUILD_SHIPPING
    FString PerfLevelCpuLastSetFString, PerfLevelGpuLastSetFString;
    SvrPerfLevelToFString(&PerfLevelCpuLastSetFString, InPerfLevelCpu);
    SvrPerfLevelToFString(&PerfLevelGpuLastSetFString, InPerfLevelGpu);
    UE_LOG(LogSVR, Log, TEXT("%s:CPU = %s; GPU = %s"), InPrefix, *PerfLevelCpuLastSetFString, *PerfLevelGpuLastSetFString);
#endif//#if !UE_BUILD_SHIPPING
}

static bool PerfLevelLastSetByCvarRead(
	enum svrPerfLevel* OutPerfLevel,
	const TAutoConsoleVariable<FString>& InCVar,
	const enum svrPerfLevel InPerfLevelDefault)
{
	const bool bReadSucceeded = FStringToSvrPerfLevel(OutPerfLevel, InCVar.GetValueOnAnyThread());
	if (!bReadSucceeded)
	{
		*OutPerfLevel = InPerfLevelDefault;
	}

	return bReadSucceeded;
}

/*static*/ bool FSnapdragonVRHMD::PerfLevelsLastSetByCvarRead(
	enum svrPerfLevel* OutPerfLevelCpuCurrent,
	enum svrPerfLevel* OutPerfLevelGpuCurrent,
	const enum svrPerfLevel InPerfLevelCpuDefault,
	const enum svrPerfLevel InPerfLevelGpuDefault)
{
	bool bValid = PerfLevelLastSetByCvarRead(OutPerfLevelCpuCurrent, CVarPerfLevelCpu, InPerfLevelCpuDefault);
	bValid &= PerfLevelLastSetByCvarRead(OutPerfLevelGpuCurrent, CVarPerfLevelGpu, InPerfLevelGpuDefault);
	return bValid;
}

static void PerfLevelOnChangeByCvar(enum svrPerfLevel* const OutPerfLevelToSet, const IConsoleVariable* const InConsoleVar, const TCHAR* const InLogPrefix)
{
	//svr performance levels can be manipulated by render or game thread, so prevent race conditions
	FScopeLock ScopeLock(&FSnapdragonVRHMDCustomPresent::PerfLevelCriticalSection);

	enum svrPerfLevel PerfLevel;
	const bool bReadSucceeded = FStringToSvrPerfLevel(&PerfLevel, InConsoleVar->GetString());
	if (bReadSucceeded)
	{
		PerfLevelLastSet(OutPerfLevelToSet, PerfLevel);
		FSnapdragonVRHMD::PerfLevelLog(InLogPrefix, FSnapdragonVRHMD::PerfLevelCpuLastSet, FSnapdragonVRHMD::PerfLevelGpuLastSet);
		svrSetPerformanceLevels(FSnapdragonVRHMD::PerfLevelCpuLastSet, FSnapdragonVRHMD::PerfLevelGpuLastSet);
	}
}
static void PerfLevelCpuOnChangeByCvar(IConsoleVariable* InVar)
{
	PerfLevelOnChangeByCvar(&FSnapdragonVRHMD::PerfLevelCpuLastSet, InVar, TEXT("svr.perfCpu cvar"));
}
static void PerfLevelGpuOnChangeByCvar(IConsoleVariable* InVar)
{
	PerfLevelOnChangeByCvar(&FSnapdragonVRHMD::PerfLevelGpuLastSet, InVar, TEXT("svr.perfGpu cvar"));
}
#endif//#if SNAPDRAGONVR_SUPPORTED_PLATFORMS

//-----------------------------------------------------------------------------
// FSnapdragonVRHMDPlugin Implementation
//-----------------------------------------------------------------------------

class FSnapdragonVRHMDPlugin : public ISnapdragonVRHMDPlugin
{
    /** IHeadMountedDisplayModule implementation */
    virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

    FString GetModulePriorityKeyName() const
    {
        return FString(TEXT("SnapdragonVR"));
    }

	FString GetModuleKeyName() const
	{
		return FString(TEXT("SnapdragonVR"));
	}

    TSharedPtr< IHeadMountedDisplay, ESPMode::ThreadSafe > SnapdragonVRHMD;//maintain singleton instance, since for Android platforms, UEngine::PreExit() is not called, which means the destructor for FSnapdragonVRHMD is never called.  Nonetheless, CreateHeadMountedDisplay() can still be called an arbitrary number of times -- June 13, 2016
};

IMPLEMENT_MODULE(FSnapdragonVRHMDPlugin, SnapdragonVRHMD)

TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > FSnapdragonVRHMDPlugin::CreateHeadMountedDisplay()
{
#if SNAPDRAGONVR_SUPPORTED_PLATFORMS
    if (!SnapdragonVRHMD.IsValid())
    {
		SnapdragonVRHMD = TSharedPtr< FSnapdragonVRHMD, ESPMode::ThreadSafe >(new FSnapdragonVRHMD());
    }
    if (static_cast<FSnapdragonVRHMD*>(&*SnapdragonVRHMD)->IsInitialized())
    {
        UE_LOG(LogSVR, Log, TEXT("SnapdragonVR->IsInitialized() == True"));
        return SnapdragonVRHMD;
    }
    else
    {
        SnapdragonVRHMD.Reset();//failed to initialize; free useless instance of SnapdragonVR
    }
#endif//SNAPDRAGONVR_SUPPORTED_PLATFORMS
    return nullptr;
}

//-----------------------------------------------------------------------------
// FSnapdragonVRHMD, IHeadMountedDisplay Implementation
//-----------------------------------------------------------------------------

#if SNAPDRAGONVR_SUPPORTED_PLATFORMS

bool FSnapdragonVRHMD::IsHMDEnabled() const
{
    return true;
}

FString FSnapdragonVRHMD::GetVersionString() const
{
	FString s = FString::Printf(TEXT("SnapdragonVR - %s, built %s, %s"),
		*FEngineVersion::Current().ToString(), UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}

void FSnapdragonVRHMD::EnableHMD(bool allow)
{
	EnableStereo(allow);
}

EHMDDeviceType::Type FSnapdragonVRHMD::GetHMDDeviceType() const
{
    return EHMDDeviceType::DT_SnapdragonVR;
}

bool FSnapdragonVRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
    if (IsInitialized())
    {
        MonitorDesc.MonitorName = "";
        MonitorDesc.MonitorId = 0;
        MonitorDesc.DesktopX = MonitorDesc.DesktopY = 0;
        MonitorDesc.ResolutionX = RenderTargetSize.X;
        MonitorDesc.ResolutionY = RenderTargetSize.Y;

        return true;
    }
    else
    {
		MonitorDesc.MonitorName = "";
        MonitorDesc.MonitorId = 0;
        MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
    }

    return false;
}

void FSnapdragonVRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
    svrDeviceInfo di = svrGetDeviceInfo();
    OutHFOVInDegrees = FMath::RadiansToDegrees(di.targetFovXRad);
    OutVFOVInDegrees = FMath::RadiansToDegrees(di.targetFovYRad);
}

bool FSnapdragonVRHMD::DoesSupportPositionalTracking() const
{
    svrDeviceInfo di = svrGetDeviceInfo();
    const unsigned int supportedTrackingModes = svrGetSupportedTrackingModes();

    const bool bTrackingPosition = supportedTrackingModes & kTrackingPosition;
    return bTrackingPosition;
}

bool FSnapdragonVRHMD::HasValidTrackingPosition()
{
    return IsPositionalTrackingEnabled();
}

void FSnapdragonVRHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
    //Do Nothing
}

void FSnapdragonVRHMD::RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const
{
    //Do Nothing
}

void FSnapdragonVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
    //Do Nothing
}

float FSnapdragonVRHMD::GetInterpupillaryDistance() const
{
    return 0.064f;
}

void FSnapdragonVRHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	FSnapdragonVRHMDCustomPresent::FPoseStateFrame CurrentPose;
	pSnapdragonVRBridge->PoseHistory.Current(CurrentPose);
	PoseToOrientationAndPosition(CurrentPose.Pose.pose, CurrentOrientation, CurrentPosition, GWorld->GetWorldSettings()->WorldToMeters);
    CurHmdOrientation = LastHmdOrientation = CurrentOrientation;
    CurrentPosition = CurHmdPosition;
}

TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> FSnapdragonVRHMD::GetViewExtension()
{
    TSharedPtr<FSnapdragonVRHMD, ESPMode::ThreadSafe> ptr(AsShared());
    return StaticCastSharedPtr<ISceneViewExtension>(ptr);
}

void FSnapdragonVRHMD::ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation)
{
    ViewRotation.Normalize();

	FSnapdragonVRHMDCustomPresent::FPoseStateFrame CurrentPose;
	pSnapdragonVRBridge->PoseHistory.Current(CurrentPose);
	PoseToOrientationAndPosition(CurrentPose.Pose.pose, CurHmdOrientation, CurHmdPosition, GWorld->GetWorldSettings()->WorldToMeters);
    LastHmdOrientation = CurHmdOrientation;
    LastHmdPosition = CurHmdPosition;

    const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
    DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

    // Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
    // Same with roll.
    DeltaControlRotation.Pitch = 0;
    DeltaControlRotation.Roll = 0;
    DeltaControlOrientation = DeltaControlRotation.Quaternion();

    ViewRotation = FRotator(DeltaControlOrientation * CurHmdOrientation);
}

bool FSnapdragonVRHMD::UpdatePlayerCamera(FQuat& OutCurrentOrientation, FVector& OutCurrentPosition)
{
	FSnapdragonVRHMDCustomPresent::FPoseStateFrame *CurrentPose;
	CurrentPose = pSnapdragonVRBridge->PoseHistory.CurrentPtr();

	svrHeadPoseState HeadPoseState;
	GetHeadPoseState(&HeadPoseState);

	CurrentPose->Pose = HeadPoseState;

	PoseToOrientationAndPosition(CurrentPose->Pose.pose, CurHmdOrientation, CurHmdPosition, GWorld->GetWorldSettings()->WorldToMeters);
    LastHmdOrientation = CurHmdOrientation;
    LastHmdPosition = CurHmdPosition;

    OutCurrentOrientation = CurHmdOrientation;
    OutCurrentPosition = CurHmdPosition;
    return true;

}

bool FSnapdragonVRHMD::IsChromaAbCorrectionEnabled() const
{
    return true;
}

bool FSnapdragonVRHMD::IsPositionalTrackingEnabled() const
{
    return DoesSupportPositionalTracking();
}

bool FSnapdragonVRHMD::IsHeadTrackingAllowed() const
{
    return true;
}

void FSnapdragonVRHMD::ResetOrientationAndPosition(float yaw)
{
    ResetOrientation(yaw);
    ResetPosition();
}

void FSnapdragonVRHMD::ResetOrientation(float yaw)
{
    //Do Nothing
}

void FSnapdragonVRHMD::ResetPosition()
{
    //Do Nothing
}

void FSnapdragonVRHMD::SetClippingPlanes(float NCP, float FCP)
{
    //Do Nothing
}

void FSnapdragonVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
    //Do Nothing
}

FRotator FSnapdragonVRHMD::GetBaseRotation() const
{
    return FRotator::ZeroRotator;
}

void FSnapdragonVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
    //Do Nothing
}

FQuat FSnapdragonVRHMD::GetBaseOrientation() const
{
    return FQuat::Identity;
}

void FSnapdragonVRHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
    //Do Nothing
}

void FSnapdragonVRHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
    //Do Nothing
}

void FSnapdragonVRHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
    //Do Nothing
}

void FSnapdragonVRHMD::OnBeginPlay(FWorldContext& InWorldContext)
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::OnBeginPlay"));
}

void FSnapdragonVRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::OnEndPlay"));
}

bool FSnapdragonVRHMD::OnStartGameFrame( FWorldContext& WorldContext )
{
    check(pSnapdragonVRBridge)

    //if sensor fusion is inactive, but the app has been resumed and initialized on both the main thread and graphics thread, then start sensor fusion
    if (bResumed && 
        pSnapdragonVRBridge->bContextInitialized && 
        !pSnapdragonVRBridge->bInVRMode &&
        bInitialized)
    {
        BeginVRMode();
    }

	bool ReturnVal = true;

    if (pSnapdragonVRBridge->bInVRMode)
    {
		svrHeadPoseState HeadPoseState;
		ReturnVal = GetHeadPoseState(&HeadPoseState);

        //const float predictedTimeMs = svrGetPredictedDisplayTime();
        //const svrHeadPoseState poseStateSvrBasis = svrGetPredictedHeadPose(predictedTimeMs);

        FSnapdragonVRHMDCustomPresent::FPoseStateFrame svrHeadPose;
        svrHeadPose.Pose = HeadPoseState;
        svrHeadPose.FrameNumber = GFrameCounter;

		pSnapdragonVRBridge->PoseHistory.Enqueue(svrHeadPose);
    }

    return ReturnVal;
}

bool FSnapdragonVRHMD::OnEndGameFrame( FWorldContext& WorldContext )
{
	SendEvents();

    return true;
}

bool FSnapdragonVRHMD::GetHeadPoseState(svrHeadPoseState* HeadPoseState)
{
	float PredictedTime = 0.f;
	const double Now = FPlatformTime::Seconds();
	if (IsInGameThread())
	{
		static double Prev;
		const double RawDelta = Now - Prev;
		Prev = Now;
		const float ClampedPrediction = FMath::Min(0.1, RawDelta * 2.f);
		// convert Seconds to MS
		PredictedTime = ClampedPrediction * 1000.f;
		
	} 
	else if (IsInRenderingThread())
	{
		PredictedTime = svrGetPredictedDisplayTime();
	}

	svrHeadPoseState PredictedHeadPose = svrGetPredictedHeadPose(PredictedTime);

	*HeadPoseState = PredictedHeadPose;

	return true;
}

void FSnapdragonVRHMD::SendEvents()
{
	svrEvent SvrEvent;
	while (svrPollEvent(&SvrEvent))
	{
		switch (SvrEvent.eventType)
		{
		case svrEventType::kEventSdkServiceStarting:
			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRSdkServiceDelegate.Broadcast(ESnapdragonVRSdkServiceEventType::EventSdkServiceStarting);
			break;
		case svrEventType::kEventSdkServiceStarted:
			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRSdkServiceDelegate.Broadcast(ESnapdragonVRSdkServiceEventType::EventSdkServiceStarted);
			break;
		case svrEventType::kEventSdkServiceStopped:
			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRSdkServiceDelegate.Broadcast(ESnapdragonVRSdkServiceEventType::EventSdkServiceStopped);
			break;
		case svrEventType::kEventThermal:
		{
			ESnapdragonVRThermalZone ThermalZone = static_cast<ESnapdragonVRThermalZone>(SvrEvent.eventData.thermal.zone);
			ESnapdragonVRThermalLevel ThermalLevel = static_cast<ESnapdragonVRThermalLevel>(SvrEvent.eventData.thermal.level);

			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRThermalDelegate.Broadcast(ThermalZone, ThermalLevel);
			break;
		}
		case svrEventType::kEventVrModeStarted:
			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRModeDelegate.Broadcast(ESnapdragonVRModeEventType::EventVrModeStarted);
			break;
		case svrEventType::kEventVrModeStopping:
			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRModeDelegate.Broadcast(ESnapdragonVRModeEventType::EventVrModeStopping);
			break;
		case svrEventType::kEventVrModeStopped:
			USnapdragonVRHMDFunctionLibrary::GetSnapdragonVRHMDEventManager()->OnSnapdragonVRModeDelegate.Broadcast(ESnapdragonVRModeEventType::EventVrModeStopped);
			break;

		default:
			break;
		}
	}
}

void FSnapdragonVRHMD::DrawDebug(UCanvas* Canvas)
{
    Canvas->DrawText(GEngine->GetLargeFont(), FText::Format( FText::FromString("Debug Draw {0}"), FText::AsNumber(GFrameCounter)), 10.f, 10.f);

	DrawDebugTrackingCameraFrustum(GWorld, Canvas->SceneView->ViewRotation, Canvas->SceneView->ViewLocation);
}

void FSnapdragonVRHMD::DrawDebugTrackingCameraFrustum(UWorld* World, const FRotator& ViewRotation, const FVector& ViewLocation)
{
#if 0
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FSnapdragonVRHMD::DrawDebugTrackingCameraFrustum"));

	const FColor c = (HasValidTrackingPosition() ? FColor::Green : FColor::Red);
	FVector origin;
	FQuat orient;
	float lfovDeg, rfovDeg, tfovDeg, bfovDeg, nearPlane, farPlane, cameraDist;
	uint32 nSensors = GetNumOfTrackingSensors();
	for (uint8 sensorIndex = 0; sensorIndex < nSensors; ++sensorIndex)
	{

		GetTrackingSensorProperties(sensorIndex, origin, orient, lfovDeg, rfovDeg, tfovDeg, bfovDeg, cameraDist, nearPlane, farPlane);

		FVector HeadPosition;
		FQuat HeadOrient;
		GetCurrentOrientationAndPosition(HeadOrient, HeadPosition);
		const FQuat DeltaControlOrientation = ViewRotation.Quaternion() * HeadOrient.Inverse();

		orient = DeltaControlOrientation * orient;

		origin = DeltaControlOrientation.RotateVector(origin);

		const float lfov = FMath::DegreesToRadians(lfovDeg);
		const float rfov = FMath::DegreesToRadians(rfovDeg);
		const float tfov = FMath::DegreesToRadians(tfovDeg);
		const float bfov = FMath::DegreesToRadians(bfovDeg);
		FVector coneTop(0, 0, 0);
		FVector coneBase1(-farPlane, farPlane * FMath::Tan(rfov), farPlane * FMath::Tan(tfov));
		FVector coneBase2(-farPlane, -farPlane * FMath::Tan(lfov), farPlane * FMath::Tan(tfov));
		FVector coneBase3(-farPlane, -farPlane * FMath::Tan(lfov), -farPlane * FMath::Tan(bfov));
		FVector coneBase4(-farPlane, farPlane * FMath::Tan(rfov), -farPlane * FMath::Tan(bfov));
		FMatrix m(FMatrix::Identity);
		m = orient * m;
		//m *= FScaleMatrix(frame->CameraScale3D);
		m *= FTranslationMatrix(origin);
		m *= FTranslationMatrix(ViewLocation); // to location of pawn
		coneTop = m.TransformPosition(coneTop);
		coneBase1 = m.TransformPosition(coneBase1);
		coneBase2 = m.TransformPosition(coneBase2);
		coneBase3 = m.TransformPosition(coneBase3);
		coneBase4 = m.TransformPosition(coneBase4);

		// draw a point at the camera pos
		DrawDebugPoint(World, coneTop, 5, c);

		// draw main pyramid, from top to base
		DrawDebugLine(World, coneTop, coneBase1, c);
		DrawDebugLine(World, coneTop, coneBase2, c);
		DrawDebugLine(World, coneTop, coneBase3, c);
		DrawDebugLine(World, coneTop, coneBase4, c);

		// draw base (far plane)				  
		DrawDebugLine(World, coneBase1, coneBase2, c);
		DrawDebugLine(World, coneBase2, coneBase3, c);
		DrawDebugLine(World, coneBase3, coneBase4, c);
		DrawDebugLine(World, coneBase4, coneBase1, c);

		// draw near plane
		FVector coneNear1(-nearPlane, nearPlane * FMath::Tan(rfov), nearPlane * FMath::Tan(tfov));
		FVector coneNear2(-nearPlane, -nearPlane * FMath::Tan(lfov), nearPlane * FMath::Tan(tfov));
		FVector coneNear3(-nearPlane, -nearPlane * FMath::Tan(lfov), -nearPlane * FMath::Tan(bfov));
		FVector coneNear4(-nearPlane, nearPlane * FMath::Tan(rfov), -nearPlane * FMath::Tan(bfov));
		coneNear1 = m.TransformPosition(coneNear1);
		coneNear2 = m.TransformPosition(coneNear2);
		coneNear3 = m.TransformPosition(coneNear3);
		coneNear4 = m.TransformPosition(coneNear4);
		DrawDebugLine(World, coneNear1, coneNear2, c);
		DrawDebugLine(World, coneNear2, coneNear3, c);
		DrawDebugLine(World, coneNear3, coneNear4, c);
		DrawDebugLine(World, coneNear4, coneNear1, c);

		// center line
		FVector centerLine(-cameraDist, 0, 0);
		centerLine = m.TransformPosition(centerLine);
		DrawDebugLine(World, coneTop, centerLine, FColor::Yellow);
		DrawDebugPoint(World, centerLine, 5, FColor::Yellow);
	}
#endif
}


//-----------------------------------------------------------------------------
// FSnapdragonVRHMD, IStereoRendering Implementation
//-----------------------------------------------------------------------------
bool FSnapdragonVRHMD::IsStereoEnabled() const
{
    return true;
}

bool FSnapdragonVRHMD::EnableStereo(bool stereo)
{

	GEngine->bForceDisableFrameRateSmoothing = stereo;
    return true;
}

void FSnapdragonVRHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
    //UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::AdjustViewRect, x:%d, y:%d, sx:%d, sy%d"), X, Y, SizeX, SizeY);
    SizeX = SizeX / 2;

    if( StereoPass == eSSP_RIGHT_EYE )
    {
        X += SizeX;
    }
}

void FSnapdragonVRHMD::CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
    if( StereoPassType != eSSP_FULL)
    {
        const float EyeOffset = (GetInterpupillaryDistance() * 0.5f) * WorldToMeters;
        const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? -EyeOffset : EyeOffset;
        ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
    }
}

FMatrix FSnapdragonVRHMD::GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOVDegrees) const
{
    svrDeviceInfo di = svrGetDeviceInfo();

    const float ProjectionCenterOffset = 0.151976421f;
    const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;
	
	const float HalfFov = FMath::DegreesToRadians(FOVDegrees) / 2.f;//don't directly query svrDeviceInfo here; trust Unreal to pass the same FOV svrGetDeviceInfo reported in FSnapdragonVRHMD::GetFieldOfView()
    const float InWidth = di.targetEyeWidthPixels;
    const float InHeight = di.targetEyeHeightPixels;
    const float XS = 1.0f / tan(HalfFov);
    const float YS = InWidth / tan(HalfFov) / InHeight;

    const float InNearZ = GNearClippingPlane;
    return FMatrix(
        FPlane(XS, 0.0f, 0.0f, 0.0f),
        FPlane(0.0f, YS, 0.0f, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f),
        FPlane(0.0f, 0.0f, InNearZ, 0.0f))

        * FTranslationMatrix(FVector(PassProjectionOffset,0,0));
}

void FSnapdragonVRHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
    //Do Nothing
}

void FSnapdragonVRHMD::GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
    EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
    EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
}

void FSnapdragonVRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
    check(IsInGameThread());

    svrDeviceInfo di = svrGetDeviceInfo();

    InOutSizeX = di.targetEyeWidthPixels * 2;
    InOutSizeY = di.targetEyeHeightPixels;
}

bool FSnapdragonVRHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
    check(IsInGameThread());

    const uint32 InSizeX = Viewport.GetSizeXY().X;
    const uint32 InSizeY = Viewport.GetSizeXY().Y;

    FIntPoint RTSize;
	RTSize.X = Viewport.GetRenderTargetTexture()->GetSizeX();
	RTSize.Y = Viewport.GetRenderTargetTexture()->GetSizeY();

    uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
    CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
    if (NewSizeX != RTSize.X || NewSizeY != RTSize.Y)
    {
        return true;
    }
    
    return false;
}

bool FSnapdragonVRHMD::ShouldUseSeparateRenderTarget() const
{
    check(IsInGameThread());
    return IsStereoEnabled();
}

void FSnapdragonVRHMD::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
{
    check(IsInGameThread());
    FRHIViewport* const ViewportRHI = InViewport.GetViewportRHI().GetReference();

    if (!IsStereoEnabled())
    {
        if (!bUseSeparateRenderTarget)
        {
            ViewportRHI->SetCustomPresent(nullptr);
        }
        return;
    }

    pSnapdragonVRBridge->UpdateViewport(InViewport, ViewportRHI);
}

FRHICustomPresent* FSnapdragonVRHMD::GetCustomPresent()
{
    return pSnapdragonVRBridge;
}


//-----------------------------------------------------------------------------
// FSnapdragonVRHMD, ISceneViewExtension Implementation
//-----------------------------------------------------------------------------
void FSnapdragonVRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily) 
{
    InViewFamily.EngineShowFlags.MotionBlur = 0;
    InViewFamily.EngineShowFlags.HMDDistortion = false;
    InViewFamily.EngineShowFlags.ScreenPercentage = false;
    InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FSnapdragonVRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
    InView.BaseHmdOrientation = LastHmdOrientation;
    InView.BaseHmdLocation = LastHmdPosition;
    InViewFamily.bUseSeparateRenderTarget = true;

	const int EyeIndex = (InView.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	InView.ViewRect = EyeRenderViewport[EyeIndex];
}

void FSnapdragonVRHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
    //Do Nothing
}

void FSnapdragonVRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
    check(IsInRenderingThread());
}

void FSnapdragonVRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
    check(IsInRenderingThread());


    pSnapdragonVRBridge->BeginRendering(RHICmdList, InViewFamily);

	const FSceneView* MainView = InViewFamily.Views[0];
	const float WorldToMetersScale = MainView->WorldToMetersScale;

	FSnapdragonVRHMDCustomPresent::FPoseStateFrame* PoseState = pSnapdragonVRBridge->PoseHistory.Get();
	if (!PoseState)
	{
		return;
	}

	FQuat OldOrientation;
	FVector OldPosition;
	PoseToOrientationAndPosition(PoseState->Pose.pose, OldOrientation, OldPosition, WorldToMetersScale);
	const FTransform OldRelativeTransform(OldOrientation, OldPosition);

	svrHeadPoseState HeadPoseState;
	GetHeadPoseState(&HeadPoseState);

	const svrHeadPose Pose = HeadPoseState.pose;

	PoseState->Pose = HeadPoseState;

	FQuat NewOrientation;
	FVector NewPosition;
	PoseToOrientationAndPosition(Pose, NewOrientation, NewPosition, WorldToMetersScale);
	const FTransform NewRelativeTransform(NewOrientation, NewPosition);

	ApplyLateUpdate(InViewFamily.Scene, OldRelativeTransform, NewRelativeTransform);
}


//-----------------------------------------------------------------------------
// FSnapdragonVRHMD Implementation
//-----------------------------------------------------------------------------

FSnapdragonVRHMD::FSnapdragonVRHMD()
    :bInitialized(false),
    bResumed(false),
    CurHmdOrientation(FQuat::Identity),
    LastHmdOrientation(FQuat::Identity),
    DeltaControlRotation(FRotator::ZeroRotator),
    DeltaControlOrientation(FQuat::Identity),
    CurHmdPosition(FVector::ZeroVector),
    LastHmdPosition(FVector::ZeroVector)
{
    Startup();
}

FSnapdragonVRHMD::~FSnapdragonVRHMD()
{
    UE_LOG(LogSVR, Log, TEXT("~FSnapdragonVRHMD()"));
}


bool FSnapdragonVRHMD::IsInitialized() const
{
    return bInitialized;
}

bool FSnapdragonVRHMD::InitializeExternalResources()
{
	CVarPerfLevelCpu.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&PerfLevelCpuOnChangeByCvar));
	CVarPerfLevelGpu.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&PerfLevelGpuOnChangeByCvar));

    extern struct android_app* GNativeAndroidApp;

    svrInitParams initParams;

    initParams.javaVm = GJavaVM;
    initParams.javaEnv = FAndroidApplication::GetJavaEnv();
    initParams.javaActivityObject = GNativeAndroidApp->activity->clazz;

    if (svrInitialize(&initParams) != SVR_ERROR_NONE)
    {
        UE_LOG(LogSVR, Error, TEXT("svrInitialize failed"));
        return false;
    }
    else
    {
        UE_LOG(LogSVR, Log, TEXT("svrInitialize succeeeded"));

        svrSetTrackingMode(kTrackingRotation | kTrackingPosition);
        return true;
    }
}

void FSnapdragonVRHMD::Startup()
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::Startup(); this=%x"), this);

    bInitialized = InitializeExternalResources();
    if (!bInitialized)
    {
        return;
    }

    pSnapdragonVRBridge = new FSnapdragonVRHMDCustomPresent(this);

    //Register life cycle delegates
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FSnapdragonVRHMD::ApplicationWillEnterBackgroundDelegate);
    FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FSnapdragonVRHMD::ApplicationWillDeactivateDelegate);//calls to this delegate are often (always?) paired with a call to ApplicationWillEnterBackgroundDelegate(), but cover the possibility that only this delegate is called
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FSnapdragonVRHMD::ApplicationHasEnteredForegroundDelegate);
    FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FSnapdragonVRHMD::ApplicationHasReactivatedDelegate);//calls to this delegate are often (always?) paired with a call to ApplicationHasEnteredForegroundDelegate(), but cover the possibility that only this delegate is called

                                                                                                                         //don't bother with ApplicationWillTerminateDelegate() -- "There is no guarantee that this will ever be called on a mobile device, save state when ApplicationWillEnterBackgroundDelegate is called instead." -- https://docs.unrealengine.com/latest/INT/API/Runtime/Core/Misc/FCoreDelegates/ApplicationWillTerminateDelegate/index.html --June 10, 2016
                                                                                                                         //OnExit() and OnPreExit() are not documented as being called on mobile -- https://docs.unrealengine.com/latest/INT/API/Runtime/Core/Misc/FCoreDelegates/OnExit/index.html and https://docs.unrealengine.com/latest/INT/API/Runtime/Core/Misc/FCoreDelegates/OnPreExit/index.html --June 10, 2016
	svrDeviceInfo DeviceInfo = svrGetDeviceInfo();
	RenderTargetSize.X = DeviceInfo.targetEyeWidthPixels * 2;
	RenderTargetSize.Y = DeviceInfo.targetEyeHeightPixels;

	const int32 RTSizeX = RenderTargetSize.X;
	const int32 RTSizeY = RenderTargetSize.Y;
	EyeRenderViewport[0] = FIntRect(0, 0, RTSizeX / 2 - 1, RTSizeY - 1);
	EyeRenderViewport[1] = FIntRect(RTSizeX / 2 + 1, 1, RTSizeX - 1, RTSizeY - 1);

	// disable vsync
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVSyncVar->Set(false);

	// enforce finishcurrentframe
	static IConsoleVariable* CFCFVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	CFCFVar->Set(false);
}

void FSnapdragonVRHMD::ApplicationWillEnterBackgroundDelegate()
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::ApplicationWillEnterBackgroundDelegate"));
    CleanupIfNecessary();
}

void FSnapdragonVRHMD::CleanupIfNecessary()
{
    bResumed = false;

    if (pSnapdragonVRBridge->bInVRMode)
    {
        UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::CleanupIfNecessary(): svrEndVr() svrShutdown()"));

        //stop consuming resources for VR until the app is resumed
        svrEndVr();
        svrShutdown();
        pSnapdragonVRBridge->bInVRMode = false;
        bInitialized = false;
    }
}

void FSnapdragonVRHMD::ApplicationWillDeactivateDelegate()
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::ApplicationWillDeactivateDelegate"));
    CleanupIfNecessary();
}

void FSnapdragonVRHMD::ApplicationHasReactivatedDelegate()
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::ApplicationHasReactivatedDelegate"));
    InitializeIfNecessaryOnResume();
}

void FSnapdragonVRHMD::ApplicationHasEnteredForegroundDelegate()
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMD::ApplicationHasEnteredForegroundDelegate"));
    InitializeIfNecessaryOnResume();
}

void FSnapdragonVRHMD::InitializeIfNecessaryOnResume()
{
    if (!bInitialized)//Upon application launch, FSnapdragonVRHMD::Startup() must initialize immediately, but Android lifecycle "resume" delegates will also be called -- don't double-initialize
    {
        bInitialized = InitializeExternalResources();
    }
    bResumed = true;
}

void FSnapdragonVRHMD::SetCPUAndGPULevels(const int32 InCPULevel, const int32 InGPULevel) const
{
	const svrPerfLevel SvrPerfLevelCPU = static_cast<const svrPerfLevel>(InCPULevel);
	const svrPerfLevel SvrPerfLevelGPU = static_cast<const svrPerfLevel>(InGPULevel);
	check(IsPerfLevelValid(SvrPerfLevelCPU));
	check(IsPerfLevelValid(SvrPerfLevelGPU));

	//svr performance levels can be manipulated by render or game thread, so prevent race conditions
	FScopeLock ScopeLock(&pSnapdragonVRBridge->PerfLevelCriticalSection);
	PerfLevelCpuLastSet = SvrPerfLevelCPU;
	PerfLevelGpuLastSet = SvrPerfLevelGPU;
// CTORNE ->
	if (pSnapdragonVRBridge->bInVRMode)
	{
		svrSetPerformanceLevels(PerfLevelCpuLastSet, PerfLevelGpuLastSet);
		FSnapdragonVRHMD::PerfLevelLog(TEXT("SetCPUAndGPULevels"), PerfLevelCpuLastSet, PerfLevelGpuLastSet);
	}
// <- CTORNE
}

void FSnapdragonVRHMD::PoseToOrientationAndPosition(const svrHeadPose& Pose, FQuat& OutCurrentOrientation, FVector& OutCurrentPosition, const float WorldToMetersScale)
{
    if (pSnapdragonVRBridge->bInVRMode)
    {
		OutCurrentOrientation = FQuat(-Pose.rotation.z, Pose.rotation.x, Pose.rotation.y, Pose.rotation.w);
		OutCurrentPosition = FVector(Pose.position.z * WorldToMetersScale, -Pose.position.x * WorldToMetersScale, -Pose.position.y * WorldToMetersScale);
    }
    else
    {
        OutCurrentOrientation = FQuat(FRotator(0.0f, 0.0f, 0.0f));
        OutCurrentPosition = FVector(0.0f, 0.0f, 0.0f);
    }
}

void FSnapdragonVRHMD::BeginVRMode()
{
    check(pSnapdragonVRBridge);
    if (IsInRenderingThread())
    {
        pSnapdragonVRBridge->DoBeginVR();
    }
    else
    {
        ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(BeginVRMode,
            FSnapdragonVRHMDCustomPresent*, pSnapdragonVRBridge, pSnapdragonVRBridge,
            {
                pSnapdragonVRBridge->DoBeginVR();
            });
        FlushRenderingCommands();
    }
}

void FSnapdragonVRHMD::EndVRMode()
{
}

uint32 FSnapdragonVRHMD::GetNumberOfBufferedFrames() const
{
    return 1;
}

#endif //SNAPDRAGONVR_SUPPORTED_PLATFORMS