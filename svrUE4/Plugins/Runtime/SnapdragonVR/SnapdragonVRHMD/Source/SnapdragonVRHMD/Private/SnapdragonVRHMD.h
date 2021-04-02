//=============================================================================
// FILE: SnapdragonVRHMD.h
//
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================

#pragma once

#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayBase.h"
#include "ISnapdragonVRHMDPlugin.h"

#include "Queue.h"
#include "CircularBuffer.h"

#if SNAPDRAGONVR_SUPPORTED_PLATFORMS
#include "svrApi.h"
#include "SceneViewExtension.h"

#include "OpenGLDrvPrivate.h"//unfortunately, OpenGLResources.h does not stand alone, but requires files #included'd ahead of it
#include "OpenGLResources.h"


template<typename ElementType> class FCircularQueue
{
public:

	/**
	* Default constructor.
	*
	* @param Size The number of elements that the queue can hold (will be rounded up to the next power of 2).
	*/
	FCircularQueue(uint32 Size)
		: Buffer(Size)
		, Head(0)
		, Tail(0)
	{ }

	/** Virtual destructor. */
	virtual ~FCircularQueue() { }

public:

	/**
	* Gets the number of elements in the queue.
	*
	* @return Number of queued elements.
	*/
	uint32 Count() const
	{
		int32 Count = Tail - Head;

		if (Count < 0)
		{
			Count += Buffer.Capacity();
		}

		return Count;
	}

	/**
	* Removes an item from the front of the queue.
	*
	* @param OutElement Will contain the element if the queue is not empty.
	* @return true if an element has been returned, false if the queue was empty.
	*/
	bool Dequeue(ElementType& OutElement)
	{
		if (Head != Tail)
		{
			OutElement = Buffer[Head];
			Head = Buffer.GetNextIndex(Head);

			return true;
		}

		return false;
	}

	/**
	* Empties the queue.
	*
	* @see IsEmpty
	*/
	void Empty()
	{
		Head = Tail;
	}

	/**
	* Adds an item to the end of the queue.
	*
	* @param Element The element to add.
	* @return true if the item was added, false if the queue was full.
	*/
	bool Enqueue(const ElementType& Element)
	{
		uint32 NewTail = Buffer.GetNextIndex(Tail);

		if (NewTail != Head)
		{
			Buffer[Tail] = Element;
			Tail = NewTail;

			return true;
		}

		return false;
	}

	/**
	* Checks whether the queue is empty.
	*
	* @return true if the queue is empty, false otherwise.
	* @see Empty, IsFull
	*/
	FORCEINLINE bool IsEmpty() const
	{
		return (Head == Tail);
	}

	/**
	* Checks whether the queue is full.
	*
	* @return true if the queue is full, false otherwise.
	* @see IsEmpty
	*/
	bool IsFull() const
	{
		return (Buffer.GetNextIndex(Tail) == Head);
	}

	/**
	* Returns the oldest item in the queue without removing it.
	*
	* @param OutItem Will contain the item if the queue is not empty.
	* @return true if an item has been returned, false if the queue was empty.
	*/
	bool Peek(ElementType& OutItem)
	{
		if (Head != Tail)
		{
			OutItem = Buffer[Head];

			return true;
		}

		return false;
	}

	ElementType* Get()
	{
		if (Head != Tail)
		{
			return &Buffer[Head];
		}

		return nullptr;
	}

	ElementType* CurrentPtr()
	{
		if (Head != Tail)
		{
			return &Buffer[Tail];
		}

		return nullptr;
	}

	bool Current(ElementType& OutItem)
	{
		if (Head != Tail)
		{
			OutItem = Buffer[Tail];

			return true;
		}

		return false;
	}

private:

	/** Holds the buffer. */
	TCircularBuffer<ElementType> Buffer;

	/** Holds the index to the first item in the buffer. */
	MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) volatile uint32 Head GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);

	/** Holds the index to the last item in the buffer. */
	MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) volatile uint32 Tail GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);
};

class FSnapdragonVRHMD;

class FSnapdragonVRHMDTextureSet:public FOpenGLTexture2D
{
public:
    static FSnapdragonVRHMDTextureSet* CreateTexture2DSet(
        FOpenGLDynamicRHI* InGLRHI,
        uint32 SizeX, 
        uint32 SizeY,
        uint32 InNumSamples,
		uint32 InNumSamplesTileMem,
        EPixelFormat InFormat,
        uint32 InFlags,
		uint32 InTargetableTextureFlags);

    FSnapdragonVRHMDTextureSet(
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
		uint32 InTargetableTexureFlags);

    virtual ~FSnapdragonVRHMDTextureSet() override;

    void SwitchToNextElement();
    void SetIndexAsCurrentRenderTarget();
    GLuint GetRenderTargetResource();

private:

    FSnapdragonVRHMDTextureSet(const FSnapdragonVRHMDTextureSet &) = delete;
    FSnapdragonVRHMDTextureSet(FSnapdragonVRHMDTextureSet &&) = delete;
    FSnapdragonVRHMDTextureSet &operator=(const FSnapdragonVRHMDTextureSet &) = delete;

    enum { mkRenderTargetTextureNum = 3 };
    FTexture2DRHIRef    mRenderTargetTexture2DHIRef[mkRenderTargetTextureNum];
    int                 mRenderTargetTexture2DHIRefIndex;
};

class FSnapdragonVRHMDCustomPresent : public FRHICustomPresent
{
    friend class FViewExtension;
    friend class FSnapdragonVRHMD;

	enum 
	{
		kMaxPoseHistoryLength = 8
	};

public:
    // FRHICustomPresent
    virtual void OnBackBufferResize() override;
    virtual bool Present(int32& InOutSyncInterval) override;

    //FSnapdragonVRHMDCustomPresent
    FSnapdragonVRHMDCustomPresent(FSnapdragonVRHMD* pHMD);
    void BeginRendering(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily);
    void FinishRendering();
    void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI);
    void DoBeginVR();
    void DoEndVR();
    bool AllocateRenderTargetTexture(
        uint32 SizeX, 
        uint32 SizeY, 
        uint8 Format, 
        uint32 NumMips, 
        uint32 Flags, 
        uint32 TargetableTextureFlags, 
        FTexture2DRHIRef& OutTargetableTexture, 
        FTexture2DRHIRef& OutShaderResourceTexture, 
        uint32 NumSamples);

public:
	static FCriticalSection InVRModeCriticalSection;
	static FCriticalSection	PerfLevelCriticalSection;
    bool                bInVRMode;
    bool                bContextInitialized;

    struct FPoseStateFrame
    {
        svrHeadPoseState Pose;
        uint64 FrameNumber;
    };

    bool PoseIsEmpty();

	FCircularQueue<FPoseStateFrame> PoseHistory;

private:

	TRefCountPtr<FSnapdragonVRHMDTextureSet> mTextureSet;
};


class FSnapdragonVRHMD : public FHeadMountedDisplayBase, public ISceneViewExtension, public TSharedFromThis<FSnapdragonVRHMD, ESPMode::ThreadSafe>
{
public:
	/** IHeadMountedDisplay interface */
	virtual FName GetDeviceName() const override
	{
		static FName DefaultName(TEXT("SnapdragonVRHMD"));
		return DefaultName;
	}

	virtual bool IsHMDConnected() override { return true; }
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;
	virtual void RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;

	virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual class TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual void ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation) override;

	//virtual void UpdatePlayerCameraRotation(class APlayerCameraManager* Camera, struct FMinimalViewInfo& POV) override;
	virtual bool UpdatePlayerCamera(FQuat& OutCurrentOrientation, FVector& OutCurrentPosition) override;

	virtual bool IsChromaAbCorrectionEnabled() const override;

	virtual bool IsPositionalTrackingEnabled() const override;

	virtual bool IsHeadTrackingAllowed() const override;

	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;

	virtual void SetClippingPlanes(float NCP, float FCP) override;

	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;

	virtual FString GetVersionString() const override;
	void SetCPUAndGPULevels(const int32 InCPULevel, const int32 InGPULevel) const;

	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;

	virtual bool HasHiddenAreaMesh() const override { return false; }
	virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual bool HasVisibleAreaMesh() const override { return false; }
	virtual void DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;

	virtual void UpdateScreenSettings(const FViewport* InViewport) override {}

	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
	virtual void OnEndPlay(FWorldContext& InWorldContext) override;
    virtual bool OnStartGameFrame( FWorldContext& WorldContext ) override;
    virtual bool OnEndGameFrame( FWorldContext& WorldContext ) override;

	virtual void DrawDebug(UCanvas* Canvas) override;
	void DrawDebugTrackingCameraFrustum(UWorld* World, const FRotator& ViewRotation, const FVector& ViewLocation);

	/** IStereoRendering interface */
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOVDegrees) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const override;
	virtual void GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;
	virtual bool ShouldUseSeparateRenderTarget() const override;
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport*) override;
	FRHICustomPresent* GetCustomPresent() override;

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;

	virtual uint32 GetNumberOfBufferedFrames() const override;
    virtual bool AllocateRenderTargetTexture(
        uint32 Index, 
        uint32 SizeX, 
        uint32 SizeY, 
        uint8 Format, 
        uint32 NumMips, 
        uint32 Flags, 
        uint32 TargetableTextureFlags, 
        FTexture2DRHIRef& OutTargetableTexture, 
        FTexture2DRHIRef& OutShaderResourceTexture, 
        uint32 NumSamples = 1) override;

	void ShutdownRendering();

public:
	FSnapdragonVRHMD();
	virtual ~FSnapdragonVRHMD();


	bool IsInitialized() const;
	static void PerfLevelLog(const TCHAR* const InPrefix, enum svrPerfLevel InPerfLevelCpu, enum svrPerfLevel InPerfLevelGpu);
	static bool PerfLevelsLastSetByCvarRead(
		enum svrPerfLevel* OutPerfLevelCpuCurrent, 
		enum svrPerfLevel* OutPerfLevelGpuCurrent, 
		const enum svrPerfLevel InPerfLevelCpuDefault,
		const enum svrPerfLevel InPerfLevelGpuDefault);
	static enum svrPerfLevel GetCVarSnapdragonVrPerfLevelDefault();
	static void PerfLevelCpuWrite(const enum svrPerfLevel InPerfLevel);
	static void PerfLevelGpuWrite(const enum svrPerfLevel InPerfLevel);
	static enum svrPerfLevel PerfLevelCpuLastSet, PerfLevelGpuLastSet;

private:
	void Startup();
	void InitializeIfNecessaryOnResume();
	void CleanupIfNecessary();

	void ApplicationWillEnterBackgroundDelegate();
	void ApplicationWillDeactivateDelegate();
	void ApplicationHasEnteredForegroundDelegate();
	void ApplicationHasReactivatedDelegate();

	void PoseToOrientationAndPosition(const svrHeadPose& Pose, FQuat& CurrentOrientation, FVector& CurrentPosition, const float WorldToMetersScale);

	void BeginVRMode();
	void EndVRMode();

	bool GetHeadPoseState(svrHeadPoseState* HeadPoseState);

	void SendEvents();

private:
	bool bInitialized;
	bool bResumed;

	TRefCountPtr<FSnapdragonVRHMDCustomPresent> pSnapdragonVRBridge;

	bool InitializeExternalResources();

	//Temporary until svr pose is integrated
	FQuat					CurHmdOrientation;
	FQuat					LastHmdOrientation;
	FVector                 CurHmdPosition;
	FVector					LastHmdPosition;
    FRotator				DeltaControlRotation;    
    FQuat					DeltaControlOrientation; 
	double					LastSensorTime;

	FIntPoint				RenderTargetSize;
	FIntRect				EyeRenderViewport[2];
};


DEFINE_LOG_CATEGORY_STATIC(LogSVR, Log, All);

#endif //SNAPDRAGONVR_SUPPORTED_PLATFORMS
