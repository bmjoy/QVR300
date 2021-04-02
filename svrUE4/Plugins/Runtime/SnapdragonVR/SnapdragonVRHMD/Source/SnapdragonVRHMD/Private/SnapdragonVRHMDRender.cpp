//=============================================================================
// FILE: SnapdragonVRRender.cpp
//
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================

//#include "HMDPrivatePCH.h"
#include "Runtime/Core/Public/Misc/ScopeLock.h"
#include "SnapdragonVRHMD.h"
#include "RHIStaticStates.h"

#if SNAPDRAGONVR_SUPPORTED_PLATFORMS

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#endif // PLATFORM_ANDROID


#include <android_native_app_glue.h>


#endif // SNAPDRAGONVR_SUPPORTED_PLATFORMS



#if SNAPDRAGONVR_SUPPORTED_PLATFORMS

extern struct android_app* GNativeAndroidApp;

//-----------------------------------------------------------------------------
// FSnapdragonVRHMDCustomPresent, IStereoRendering Implementation
//-----------------------------------------------------------------------------
void FSnapdragonVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const
{

}


//-----------------------------------------------------------------------------
// FSnapdragonVRHMDCustomPresent, FRHICustomPresent Implementation
//-----------------------------------------------------------------------------

void FSnapdragonVRHMDCustomPresent::OnBackBufferResize()
{

}

bool FSnapdragonVRHMDCustomPresent::Present(int32& InOutSyncInterval)
{
    check(IsInRenderingThread());

    FinishRendering();

    return false;
}

FSnapdragonVRHMDTextureSet::FSnapdragonVRHMDTextureSet(
    class FOpenGLDynamicRHI* InGLRHI,
    GLuint InResource,
    GLenum InTarget,
    GLenum InAttachment,
    uint32 InSizeX,
    uint32 InSizeY,
    uint32 InSizeZ,
    uint32 InNumMips,
    uint32 InNumSamples,
	uint32 InNumSamplesTileMem,
    uint32 InArraySize,
    EPixelFormat InFormat,
    bool bInCubemap,
    bool bInAllocatedStorage,
    uint32 InFlags,
    uint8* InTextureRange,
	uint32 InTargetableTextureFlags
):  FOpenGLTexture2D(
        InGLRHI,
        InResource,
        InTarget,
        InAttachment,
        InSizeX,
        InSizeY,
        InSizeZ,
        InNumMips,
        InNumSamples,
		InNumSamplesTileMem,
        InArraySize,
        InFormat,
        bInCubemap,
        bInAllocatedStorage,
        InFlags,
        InTextureRange,
        FClearValueBinding::Black)
{
    FRHIResourceCreateInfo CreateInfo;
    for (int i = 0; i < mkRenderTargetTextureNum; ++i)
    {
        mRenderTargetTexture2DHIRef[i] = InGLRHI->RHICreateTexture2D(InSizeX, InSizeY, InFormat, InNumMips, InNumSamples, InTargetableTextureFlags, CreateInfo);
    }

    mRenderTargetTexture2DHIRefIndex = 0;
    SetIndexAsCurrentRenderTarget();
}
FSnapdragonVRHMDTextureSet* FSnapdragonVRHMDTextureSet::CreateTexture2DSet(
    FOpenGLDynamicRHI* InGLRHI,
    uint32 SizeX, 
    uint32 SizeY,
    uint32 InNumSamples,
	uint32 InNumSamplesTileMem,
    EPixelFormat InFormat,
    uint32 InFlags,
	uint32 InTargetableTextureFlags)
{
    const GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    return new FSnapdragonVRHMDTextureSet(InGLRHI, 0, Target, GL_NONE, SizeX, SizeY, 0, 1, InNumSamples, InNumSamplesTileMem, 1, InFormat, false, false, InFlags, nullptr, InTargetableTextureFlags);
}
FSnapdragonVRHMDTextureSet::~FSnapdragonVRHMDTextureSet() {}

void FSnapdragonVRHMDTextureSet::SwitchToNextElement()
{
    mRenderTargetTexture2DHIRefIndex = ((mRenderTargetTexture2DHIRefIndex + 1) % mkRenderTargetTextureNum);
    SetIndexAsCurrentRenderTarget();
}
void FSnapdragonVRHMDTextureSet::SetIndexAsCurrentRenderTarget()
{
    Resource = GetRenderTargetResource();
}
GLuint FSnapdragonVRHMDTextureSet::GetRenderTargetResource()
{
    return *(GLuint*)mRenderTargetTexture2DHIRef[mRenderTargetTexture2DHIRefIndex]->GetNativeResource();
}

//-----------------------------------------------------------------------------
// FSnapdragonVRHMDCustomPresent Implementation
//-----------------------------------------------------------------------------
FSnapdragonVRHMDCustomPresent::FSnapdragonVRHMDCustomPresent(FSnapdragonVRHMD* pHMD)
    : FRHICustomPresent(0),
    bInVRMode(false),
    bContextInitialized(false),
	PoseHistory(kMaxPoseHistoryLength)
{
	//glGenQueriesEXT(kMaxQueryCount, QueryResources);
    //memset(&mPoseLastGeneratedGameThread, 0, sizeof(mPoseLastGeneratedGameThread));
}

void FSnapdragonVRHMDCustomPresent::BeginRendering(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
    bContextInitialized = true;
}

bool FSnapdragonVRHMD::AllocateRenderTargetTexture(
    uint32 Index, 
    uint32 SizeX, 
    uint32 SizeY, 
    uint8 Format, 
    uint32 NumMips, 
    uint32 Flags, 
    uint32 TargetableTextureFlags, 
    FTexture2DRHIRef& OutTargetableTexture, 
    FTexture2DRHIRef& OutShaderResourceTexture, 
    uint32 NumSamples)
{
    return pSnapdragonVRBridge->AllocateRenderTargetTexture(
        SizeX, 
        SizeY, 
        Format, 
        NumMips, 
        Flags, 
        TargetableTextureFlags, 
        OutTargetableTexture, 
        OutShaderResourceTexture, 
        NumSamples);
}

bool FSnapdragonVRHMDCustomPresent::AllocateRenderTargetTexture(
    uint32 SizeX, 
    uint32 SizeY, 
    uint8 Format, 
    uint32 NumMips, 
    uint32 Flags, 
    uint32 TargetableTextureFlags, 
    FTexture2DRHIRef& OutTargetableTexture, 
    FTexture2DRHIRef& OutShaderResourceTexture, 
    uint32 NumSamples)
{
    auto OpenGLDynamicRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
    check(OpenGLDynamicRHI);
    mTextureSet = TRefCountPtr<FSnapdragonVRHMDTextureSet>(FSnapdragonVRHMDTextureSet::CreateTexture2DSet(OpenGLDynamicRHI, SizeX, SizeY, 1, NumSamples, EPixelFormat(Format), Flags, TargetableTextureFlags));
    OutTargetableTexture = OutShaderResourceTexture = mTextureSet->GetTexture2D();
    return true;
}

void FSnapdragonVRHMDCustomPresent::FinishRendering()
{
    check(IsInRenderingThread());

    if(bInVRMode)
    {
        //const bool emptyQueue = mPoseQueueSvrBasis.IsEmpty();
		const bool PoseHistoryEmpty = PoseHistory.IsEmpty();
        if (PoseHistoryEmpty)
        {
            UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMDCustomPresent::FinishRendering() found no pose!"));
        }
        else
        {
            FPoseStateFrame poseStateDebug;
            PoseHistory.Dequeue(poseStateDebug);

            svrFrameParams frameParams;
            memset(&frameParams, 0, sizeof(frameParams));
            frameParams.frameIndex = poseStateDebug.FrameNumber;
            frameParams.eyeLayers[0].imageHandle = mTextureSet->GetRenderTargetResource();
			frameParams.eyeLayers[0].imageType = kTypeTexture;
			frameParams.eyeLayers[0].eyeMask = kEyeMaskBoth;
            frameParams.eyeBufferType = kEyeBufferStereoSingle;
            frameParams.headPoseState = poseStateDebug.Pose;
            frameParams.minVsyncs = 1;

            //UE_LOG(LogSVR, Error, TEXT("    FSnapdragonVRHMDCustomPresent::FinishRendering() => svrSubmitFrame()"));
            svrSubmitFrame(&frameParams);
            mTextureSet->SwitchToNextElement();
        }
    }
}

bool FSnapdragonVRHMDCustomPresent::PoseIsEmpty()
{
    return PoseHistory.IsEmpty();
}

#if 0
bool FSnapdragonVRHMDCustomPresent::PosePeek(FPoseStateFrame& OutTailItem)
{
	
    if (PoseIsEmpty())
    {
        return false;
    }

    //TQueue::Peek() seems broken (eg always returns a default-constructed item); workaround with this value, which should always be equal to the tail of mPoseQueueSvrBasis
    OutTailItem = mPoseLastGeneratedGameThread;
    return true;
}
bool FSnapdragonVRHMDCustomPresent::PoseDequeue(FPoseStateFrame& OutItem)
{
    check(IsInRenderingThread());
    return mPoseQueueSvrBasis.Dequeue(OutItem);
}
bool FSnapdragonVRHMDCustomPresent::PoseEnqueue(const FPoseStateFrame& InItem)
{
    check(IsInGameThread());
    mPoseLastGeneratedGameThread = InItem;
    return mPoseQueueSvrBasis.Enqueue(InItem);
}
#endif

void FSnapdragonVRHMDCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
    check(IsInGameThread());
    check(InViewportRHI);

    InViewportRHI->SetCustomPresent(this);
}

void FSnapdragonVRHMDCustomPresent::DoBeginVR()
{
    //UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMDCustomPresent::DoBeginVR()"));

    check(IsInRenderingThread());
    check(!IsInGameThread());

    if(!bInVRMode)
    {
        bInVRMode = true;

		//svr performance levels can be manipulated by render or game thread, so prevent race conditions
		FScopeLock ScopeLock(&PerfLevelCriticalSection);
		svrBeginParams beginParams;
// CTORNE ->
        if (FSnapdragonVRHMD::PerfLevelCpuLastSet != kNumPerfLevels && FSnapdragonVRHMD::PerfLevelGpuLastSet != kNumPerfLevels)
        {
            beginParams.cpuPerfLevel = FSnapdragonVRHMD::PerfLevelCpuLastSet;
            beginParams.gpuPerfLevel = FSnapdragonVRHMD::PerfLevelGpuLastSet;
        }
        else
        {
		const svrPerfLevel PerfLevelDefault = FSnapdragonVRHMD::GetCVarSnapdragonVrPerfLevelDefault();
		FSnapdragonVRHMD::PerfLevelsLastSetByCvarRead(&beginParams.cpuPerfLevel, &beginParams.gpuPerfLevel, PerfLevelDefault, PerfLevelDefault);
        }
// <- CTORNE
		beginParams.nativeWindow = GNativeAndroidApp->window;
		beginParams.mainThreadId = gettid();

		FSnapdragonVRHMD::PerfLevelCpuWrite(beginParams.cpuPerfLevel); 
		FSnapdragonVRHMD::PerfLevelGpuWrite(beginParams.gpuPerfLevel);
		FSnapdragonVRHMD::PerfLevelLog(TEXT("svrBeginVr"), FSnapdragonVRHMD::PerfLevelCpuLastSet, FSnapdragonVRHMD::PerfLevelGpuLastSet);
        svrBeginVr(&beginParams);
    }
}

void FSnapdragonVRHMDCustomPresent::DoEndVR()
{
    UE_LOG(LogSVR, Log, TEXT("FSnapdragonVRHMDCustomPresent::DoEndVR()"));
}

#endif //SNAPDRAGONVR_SUPPORTED_PLATFORMS