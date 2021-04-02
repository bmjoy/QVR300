//=============================================================================
// FILE: svrApiTimeWarp.cpp
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <android/native_window.h>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

#include "svrCpuTimer.h"
#include "svrGpuTimer.h"
#include "svrGeometry.h"
#include "svrProfile.h"
#include "svrShader.h"
#include "svrUtil.h"
#include "svrConfig.h"
#include "svrRenderExt.h"

#include "private/svrApiCore.h"
#include "private/svrApiEvent.h"
#include "private/svrApiHelper.h"
#include "private/svrApiTimeWarp.h"

#include <sys/system_properties.h>
#include "FloatingPoint.h"

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR        0x0040
#endif

#if !defined(EGL_CONTEXT_PRIORITY_LEVEL_IMG)
#define EGL_CONTEXT_PRIORITY_LEVEL_IMG          0x3100
#define EGL_CONTEXT_PRIORITY_HIGH_IMG           0x3101
#define EGL_CONTEXT_PRIORITY_MEDIUM_IMG         0x3102
#define EGL_CONTEXT_PRIORITY_LOW_IMG            0x3103
#endif

#if !defined(GL_BINNING_CONTROL_HINT_QCOM)
#define GL_BINNING_CONTROL_HINT_QCOM            0x8FB0
#define GL_BINNING_QCOM                         0x8FB1
#define GL_VISIBILITY_OPTIMIZED_BINNING_QCOM    0x8FB2
#define GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM    0x8FB3
#endif

#if !defined(EGL_MUTABLE_RENDER_BUFFER_BIT_KHR)
#define EGL_MUTABLE_RENDER_BUFFER_BIT_KHR       0x1000
#endif

#if !defined(EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID)
#define   EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID 0x314C
#endif

#if !defined(TEXTURE_BORDER_COLOR_EXT)
#define TEXTURE_BORDER_COLOR_EXT                0x1004
#define CLAMP_TO_BORDER_EXT                     0x812D
#endif

#ifndef EGL_EXT_protected_surface
#define EGL_EXT_protected_surface 1
#define EGL_PROTECTED_CONTENT_EXT         0x32C0
#endif

#if !defined(HANDLE_TYPE_OPAQUE_FD_KHR)
#define HANDLE_TYPE_OPAQUE_FD_KHR   0x0001
#endif

// Render Timings
#define MAX_TIMINGS_TO_AVERAGE      100
#define NANOSECONDS_BETWEEN_AVERAGE 1000000000LL

// Whether to query the sync status (signaled/unsignaled) using eglClientWaitSyncKHR with a timeout of 0, or using eglGetSyncAttribKHR
// There was a bug at some point with eglClientWaitSyncKHR where passing 0 caused a wait which caused issue with Vk semaphores
#define VULKAN_USE_CLIENT_WAIT_SYNC 0

using namespace Svr;

// TimeWarp Properties
VAR(bool, gDisableLensCorrection,

false, kVariableNonpersistent);       //Override to disable any lens correction during time warp
VAR(bool, gDirectModeWarp,

true, kVariableNonpersistent);               //Override to enable/disable "direct" non-binning mode for time warp rendering
VAR(bool, gSingleBufferWindow,

true, kVariableNonpersistent);           //Override to enable/disable use of single buffer surface for time warp
VAR(int, gTimeWarpThreadCore,

2, kVariableNonpersistent);               //Core affinity for time warp thread (-1=system defined), ignored if the QVR Perf module is active.
VAR(bool, gTimeWarpClearBuffer,

false, kVariableNonpersistent);         //Enable/disable clearing the time warp surface before rendering
VAR(bool, gBusyWait,

false, kVariableNonpersistent);                    //Enable/disable "busy" wait between eyes (on some builds use of nanosleep function can oversleep by as much as 10ms if FIFO isn't set on the sleeping thread)
VAR(bool, gEnableWarpThreadFifo,

true, kVariableNonpersistent);         //Enable/disable setting SCHED_FIFO scheduling policy on the warp thread (must be true if gBusyWait is false to avoid tearing), ignored if the QVR Perf module is active.
VAR(int, gRecenterFrames,

0, kVariableNonpersistent);                   //Number of frames to disable reprojection after sensors are recentered
VAR(int, gSensorInitializeFrames,

0, kVariableNonpersistent);           //Number of frames to disable display after sensors are started
VAR(int, gTimeWarpMinLoopTime,

0, kVariableNonpersistent);              //For testing: minimum time (milliseconds) for a timewarp loop.  Can be used to simulate timewarp not able to keep up with render. Set to 0 to disable test
VAR(float, gTimeWarpWaitBias,

0.0f, kVariableNonpersistent);            //Bias applied to the time we wait to kick off warping for each eye

VAR(float, gTimeWarpClearColorR,

0.0f, kVariableNonpersistent);         //Fill color for TimeWarp clear (if gTimeWarpClearBuffer enabled)
VAR(float, gTimeWarpClearColorG,

0.0f, kVariableNonpersistent);         //Fill color for TimeWarp clear (if gTimeWarpClearBuffer enabled)
VAR(float, gTimeWarpClearColorB,

0.0f, kVariableNonpersistent);         //Fill color for TimeWarp clear (if gTimeWarpClearBuffer enabled)
VAR(float, gTimeWarpClearColorA,

1.0f, kVariableNonpersistent);         //Fill color for TimeWarp clear (if gTimeWarpClearBuffer enabled)

VAR(int, gTimeWarpEnabledEyeMask,

3, kVariableNonpersistent);           // svrEyeMask Bit Field: 1 = Left; 2 = Right; 3 = Both

VAR(int, gForceColorSpace,

-1.0f, kVariableNonpersistent);              //Force warp colorspace -1 = app specified, 0 = linear, 1 = sRGB

VAR(bool, gApplyDisplaySkew,

false, kVariableNonpersistent);            //Apply a skew during warp to counteract the impact of display rates
VAR(bool, gApplyDisplaySquash,

false, kVariableNonpersistent);          //Apply a skew during warp to counteract the impact of display rates
VAR(bool, gEnablePreTwist,

true, kVariableNonpersistent);               //Apply a skew during warp to counteract the impact of display rates
VAR(float, gSkewScaleFactor,

1.0f, kVariableNonpersistent);             //Scale factor to apply to the display skewing
VAR(float, gSquashScaleFactor,

1.0f, kVariableNonpersistent);           //Scale factor to apply to the display squash

// Compositing value
VAR(float, gCompositeRadius,

0.25f, kVariableNonpersistent);            //Any layer quad outside this range will be composited


// Warp Mesh
VAR(int, gWarpMeshType,

0, kVariableNonpersistent);                     //Warp mesh type: 0 = Columns (Left To Right); 1 = Columns (Right To Left); 2 = Rows (Top To Bottom); 3 = Rows (Bottom To Top)
VAR(int, gWarpMeshRows,

50, kVariableNonpersistent);                    //Number of rows in the warp mesh grid
VAR(int, gWarpMeshCols,

50, kVariableNonpersistent);                    //Number of columns in the warp mesh grid
VAR(int, gLayerMeshRows,

8, kVariableNonpersistent);                    //Number of rows in the layer mesh grid
VAR(int, gLayerMeshCols,

8, kVariableNonpersistent);                    //Number of columns in the layer mesh grid

VAR(float, gWarpEqrMeshRadius,

2.0, kVariableNonpersistent);            //Radius of warp mesh used for EquiRectangular display (Affects curvature)
VAR(float, gWarpEqrMeshScale,

0.9, kVariableNonpersistent);             //Amount of screen covered by the EquiRectangular warp mesh


VAR(float, gWarpMeshMinX,

-1.0f, kVariableNonpersistent);               //Screen space coordinate of minimum X-Value
VAR(float, gWarpMeshMaxX,

1.0f, kVariableNonpersistent);                //Screen space coordinate of maximum X-Value
VAR(float, gWarpMeshMinY,

-1.0f, kVariableNonpersistent);               //Screen space coordinate of minimum Y-Value
VAR(float, gWarpMeshMaxY,

1.0f, kVariableNonpersistent);                //Screen space coordinate of maximum Y-Value

VAR(float, gMeshOffsetLeftX,

0.0f, kVariableNonpersistent);             // Screen space adjustment for left mesh to center under lens
VAR(float, gMeshOffsetLeftY,

0.0f, kVariableNonpersistent);             // Screen space adjustment for left mesh to center under lens
VAR(float, gMeshOffsetRightX,

0.0f, kVariableNonpersistent);            // Screen space adjustment for right mesh to center under lens
VAR(float, gMeshOffsetRightY,

0.0f, kVariableNonpersistent);            // Screen space adjustment for right mesh to center under lens

VAR(float, gMeshAspectRatio,

1.0, kVariableNonpersistent);              // 0.0 = Use Screen Size; X.X = Force aspect ratio

// Vignette
VAR(bool, gMeshVignette,

false, kVariableNonpersistent);                //Enable/Disable vignette feature on the warp mesh
VAR(float, gVignetteRadius,

0.7f, kVariableNonpersistent);              //Start radius of vignette feature. Completes at gMeshDiscardUV applied to lens polynomial

// Clamp to Border
VAR(bool, gClampBorderEnabled,

true, kVariableNonpersistent);           //Enable/disable clamp to border color on eye buffers
VAR(float, gClampBorderColorR,

0.0f, kVariableNonpersistent);           //Fill color for clamp to border (if enabled)
VAR(float, gClampBorderColorG,

0.0f, kVariableNonpersistent);           //Fill color for clamp to border (if enabled)
VAR(float, gClampBorderColorB,

0.0f, kVariableNonpersistent);           //Fill color for clamp to border (if enabled)
VAR(float, gClampBorderColorA,

1.0f, kVariableNonpersistent);           //Fill color for clamp to border (if enabled)

VAR(bool, gClampBorderOverlayEnabled,

true, kVariableNonpersistent);    //Enable/disable clamp to border color on overlay buffers
VAR(float, gClampBorderOverlayColorR,

0.0f, kVariableNonpersistent);    //Fill color for clamp to border (if enabled)
VAR(float, gClampBorderOverlayColorG,

0.0f, kVariableNonpersistent);    //Fill color for clamp to border (if enabled)
VAR(float, gClampBorderOverlayColorB,

0.0f, kVariableNonpersistent);    //Fill color for clamp to border (if enabled)
VAR(float, gClampBorderOverlayColorA,

1.0f, kVariableNonpersistent);    //Fill color for clamp to border (if enabled)

// Stencil Mesh
VAR(bool, gStencilMeshEnabled,

false, kVariableNonpersistent);          //Enable/disable stencil mesh optimization on eye buffers
VAR(float, gStencilMeshRadius,

0.95f, kVariableNonpersistent);          //Radius of stencil mesh used for eye buffers
VAR(float, gStencilMeshColorR,

0.0f, kVariableNonpersistent);           //Fill color for stencil mesh (if enabled)
VAR(float, gStencilMeshColorG,

0.0f, kVariableNonpersistent);           //Fill color for stencil mesh (if enabled)
VAR(float, gStencilMeshColorB,

0.0f, kVariableNonpersistent);           //Fill color for stencil mesh (if enabled)
VAR(float, gStencilMeshColorA,

1.0f, kVariableNonpersistent);           //Fill color for stencil mesh (if enabled)

// Boundary System
VAR(bool, gEnableBoundarySystem,

false, kVariableNonpersistent);        //Enable/disable Boundary display
VAR(bool, gForceDisplayBoundarySystem,

false, kVariableNonpersistent);  //Forces the display of the Boundary display
VAR(bool, gLogBoundarySystem,

false, kVariableNonpersistent);           //Enable/disable loging of Boundary system

VAR(bool, gEnableBoundaryMesh,

false, kVariableNonpersistent);          //Enable/disable Boundary feature using mesh
VAR(int, gBoundaryWarpMeshRows,

2, kVariableNonpersistent);             //Number of rows in the Boundary mesh
VAR(int, gBoundaryWarpMeshCols,

2, kVariableNonpersistent);             //Number of cols in the Boundary mesh
VAR(float, gBoundaryZoneMinX,

-4.0, kVariableNonpersistent);            //Boundary zone size (Real world coordinates in meters)
VAR(float, gBoundaryZoneMaxX,

4.0, kVariableNonpersistent);             //Boundary zone size (Real world coordinates in meters)
VAR(float, gBoundaryZoneMinY,

-4.0, kVariableNonpersistent);            //Boundary zone size (Real world coordinates in meters)
VAR(float, gBoundaryZoneMaxY,

4.0, kVariableNonpersistent);             //Boundary zone size (Real world coordinates in meters)
VAR(float, gBoundaryZoneMinZ,

-4.0, kVariableNonpersistent);            //Boundary zone size (Real world coordinates in meters)
VAR(float, gBoundaryZoneMaxZ,

4.0, kVariableNonpersistent);             //Boundary zone size (Real world coordinates in meters)
VAR(float, gBoundaryUVScale,

10.0, kVariableNonpersistent);             //Boundary zone texture repeat scale (grid spacing)
VAR(float, gBoundaryGridWidth,

0.1, kVariableNonpersistent);            //Boundary zone grid line width (fractional part of result of gBoundaryUVScale)
VAR(float, gBoundaryVisabilityRadius,

0.5, kVariableNonpersistent);     //Distance from Boundary zone boundaries when the Boundary mesh starts to be visible
VAR(float, gBoundaryVisabilityScale,

1.5, kVariableNonpersistent);      //Scale for how quickly the Boundary mesh fades in to full opacity
VAR(float, gBoundaryMeshColorR,

1.0, kVariableNonpersistent);           //Boundary mesh color
VAR(float, gBoundaryMeshColorG,

1.0, kVariableNonpersistent);           //Boundary mesh color
VAR(float, gBoundaryMeshColorB,

1.0, kVariableNonpersistent);           //Boundary mesh color
VAR(float, gBoundaryMeshColorA,

1.0, kVariableNonpersistent);           //Boundary mesh color


// Lens Parameters
VAR(float, gLensOffsetX,

0.0f, kVariableNonpersistent);                 //Horizontal lens offset
VAR(float, gLensOffsetY,

0.0f, kVariableNonpersistent);                 //Vertical lens offset

// Lens Distortion Polynomial: K0 + K1*r + K2*r^2 + K3*r^3 + K4*r^4 + K5*r^5 + K6*r^6
VAR(bool, gLensInverse,

false, kVariableNonpersistent);

VAR(float, gLensPolyK0,

1.0f, kVariableNonpersistent);

VAR(float, gLensPolyK1,

0.0f, kVariableNonpersistent);

VAR(float, gLensPolyK2,

0.22f, kVariableNonpersistent);

VAR(float, gLensPolyK3,

0.0f, kVariableNonpersistent);

VAR(float, gLensPolyK4,

0.24f, kVariableNonpersistent);

VAR(float, gLensPolyK5,

0.0f, kVariableNonpersistent);

VAR(float, gLensPolyK6,

0.0f, kVariableNonpersistent);

// Chromatic Aberration Correction: 1.0 + K0 + K2*r^2
VAR(float, gChromaticPolyK0_R,

0.994f, kVariableNonpersistent);

VAR(float, gChromaticPolyK1_R,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK2_R,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK3_R,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK4_R,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK5_R,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK6_R,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK0_G,

1.003f, kVariableNonpersistent);

VAR(float, gChromaticPolyK1_G,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK2_G,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK3_G,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK4_G,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK5_G,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK6_G,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK0_B,

1.014f, kVariableNonpersistent);

VAR(float, gChromaticPolyK1_B,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK2_B,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK3_B,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK4_B,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK5_B,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPolyK6_B,

0.0f, kVariableNonpersistent);

VAR(float, gChromaticPixelBorder,

0.0f, kVariableNonpersistent);

VAR(float, gPreTwistRatio, 1.0f, kVariableNonpersistent);
VAR(float, gPreTwistRatioY, 1.0f, kVariableNonpersistent);
VAR(int, gPreTwistRatioMode, 0, kVariableNonpersistent);

// Warp Mesh discard UV value
VAR(float, gMeshDiscardUV,

1.0f, kVariableNonpersistent);               //After lens polynomial applied, anything greater than this value will be discarded.

//Logging Options
VAR(bool, gLogEyeOverSleep,

false, kVariableNonpersistent);             //Enable/disable checking and logging of oversleep during eye buffer rendering
VAR(bool, gLogFrameDoubles,

false, kVariableNonpersistent);             //Enables output of LogCat messages when TimeWarp frames are reused
VAR(bool, gLogMeshCreation,

false, kVariableNonpersistent);             //Enables output of LogCat messages when meshes are created
VAR(bool, gLogVSyncData,

false, kVariableNonpersistent);                //Enables output of LogCat messages when VSync data is updated
VAR(bool, gLogPrediction,

false, kVariableNonpersistent);

VAR(bool, gLogDroppedFrames,

false, kVariableNonpersistent);            //If next frame warped is not one more than last frame (Application may break this if it assigns wrong frameIndx)

VAR(bool, gLogShaderUniforms,

false, kVariableNonpersistent);

VAR(bool, gLogEyeRender,

false, kVariableNonpersistent);

//guowei add
VAR(float, gRightEyeAdjustBias,

    0.0f, kVariableNonpersistent);            //Bias for RightEye render

#ifdef ENABLE_MOTION_VECTORS
EXTERN_VAR(bool, gEnableMotionVectors);
EXTERN_VAR(bool, gUseMotionVectors);
EXTERN_VAR(bool, gLogMotionVectors);
EXTERN_VAR(bool, gSmoothMotionVectors);
EXTERN_VAR(bool, gSmoothMotionVectorsWithGPU);
EXTERN_VAR(bool, gRenderMotionVectors);
EXTERN_VAR(bool, gRenderMotionInput);
EXTERN_VAR(bool, gGenerateBothEyes);
#endif // ENABLE_MOTION_VECTORS

EXTERN_VAR(bool, gUseQvrPerfModule);

EXTERN_VAR(int, gPredictVersion);

EXTERN_VAR(float, gLeftFrustum_Near);

EXTERN_VAR(float, gLeftFrustum_Far);

EXTERN_VAR(float, gLeftFrustum_Left);

EXTERN_VAR(float, gLeftFrustum_Right);

EXTERN_VAR(float, gLeftFrustum_Top);

EXTERN_VAR(float, gLeftFrustum_Bottom);

EXTERN_VAR(float, gRightFrustum_Near);

EXTERN_VAR(float, gRightFrustum_Far);

EXTERN_VAR(float, gRightFrustum_Left);

EXTERN_VAR(float, gRightFrustum_Right);

EXTERN_VAR(float, gRightFrustum_Top);

EXTERN_VAR(float, gRightFrustum_Bottom);

EXTERN_VAR(bool, gUseLinePtr);

EXTERN_VAR(float, gTimeToHalfExposure);

EXTERN_VAR(float, gExtraAdjustPeriodRatio);

EXTERN_VAR(bool, gDisablePredictedTime);

EXTERN_VAR(bool, gHeuristicPredictedTime);

EXTERN_VAR(bool, gDebugWithProperty);

// External value from config file in svrApiPredictiveSensor
extern int gSensorHomePosition;

// Extern Hidden Stencil mesh data
extern float gHiddenMeshVerts[];
extern int gNumHiddenMeshVerts;
extern unsigned int gHiddenMeshIndices[];
extern int gNumHiddenMeshIndices;

// Want to timeline things
extern uint64_t gVrStartTimeNano;

// TODO: Should this be configurable?
#define STENCIL_MESH_MAX_RADIUS 0.95f

glm::mat4 mLastR = glm::mat4(1);
bool mBHasData = false;

// Global count of warp frames
int gWarpFrameCount = 0;

// What was the last frame warped
int gLastFrameIndx = 0;

// Need a global variable to keep track of a doubled frame.
// It is needed in some lower level routines but don't want
// to pass the variable to each layer.
int gFrameDoubled = 0;

// Warp data used by the TimeWarpThread
SvrAsyncWarpResources gWarpData;

// Convert the config file integer into an enumeration others can use
svrWarpMeshOrder gMeshOrderEnum = kMeshOrderLeftToRight;

int gFifoPriorityWarp = 98;
int gNormalPriorityWarp = 0;    // Cause they want something :)

// Need to ignore reprojection if just recentered.
// Handle here since may have frames in flight
extern int gRecenterTransition;

// Vulkan app needs a Surface/Context
EGLDisplay gVkConsumer_eglDisplay = EGL_NO_DISPLAY;
EGLSurface gVkConsumer_eglSurface = EGL_NO_SURFACE;
EGLContext gVkConsumer_eglContext = EGL_NO_CONTEXT;

float gLensPolyUnitRadius = 1.0f;

#ifdef ENABLE_MOTION_VECTORS
extern uint32_t gCurrentMotionDataIndx;
extern SvrMotionData *gpMotionData;

extern uint32_t gMostRecentMotionLeft;
extern uint32_t gMostRecentMotionRight;

// If we are using spacewarp, but not on a frame, we need a stub result texture
extern GLuint gStubResultTexture;
#endif // ENABLE_MOTION_VECTORS

// Common local function that lives in svrApiCore.cpp
void L_SetThreadPriority(const char *pName, int policy, int priority);

void L_AddHeuristicPredictData(uint64_t newData);

float L_GetHeuristicPredictTime();

// Forward Declarations
int AddOneQuad(unsigned int currentIndex, unsigned int *indexData, unsigned int numIndices,
               unsigned int numRows, unsigned int numCols);

#ifdef MOTION_TO_PHOTON_TEST
// Motion To Photon Test
VAR(float, gMotionToPhotonC, 10.0f, kVariableNonpersistent);            //Scale factor driving intensity of color from angular velocity
VAR(float, gMotionToPhotonAccThreshold, 0.999998f, kVariableNonpersistent);  //Minimum threshold for motion to be considered significant enough to light the display

float       gMotionToPhotonW = 0.0f;
glm::fquat  gMotionToPhotonLast;
uint64_t    gMotionToPhotonTimeStamp = 0;

#endif // MOTION_TO_PHOTON_TEST

// Each vert has the following structure
#define NUM_VERT_ATTRIBS    4

struct warpMeshVert {
    float Position[4];
    float TexCoordR[4];
    float TexCoordG[4];
    float TexCoordB[4];
};

// Global overlay vert data that we don't want to allocate every frame
warpMeshVert *gOverlayVertData = NULL;


// Need to create the layout attributes for the data.
// Must fit these in to reserved types:
//      kPosition  => Position
//      kNormal    => TexCoordR
//      kColor     => TexCoordG
//      kTexcoord0 => TexCoordB
//      kTexcoord1 => Not Used
SvrProgramAttribute gMeshAttribs[NUM_VERT_ATTRIBS] =
        {
                //  Index       Size    Type        Normalized      Stride              Offset
                {kPosition,  4, GL_FLOAT, false, sizeof(warpMeshVert), 0},
                {kNormal,    4, GL_FLOAT, false, sizeof(warpMeshVert), 16},
                {kColor,     4, GL_FLOAT, false, sizeof(warpMeshVert), 32},
                {kTexcoord0, 4, GL_FLOAT, false, sizeof(warpMeshVert), 48}
        };

bool gOverrideMeshClearLeft = false;
bool gOverrideMeshClearRight = false;

struct vulkanTexture {
    unsigned int hTexture;
    unsigned int hMemory;
    unsigned int memSize;
    unsigned int width;
    unsigned int height;
    unsigned int numMips;
};

typedef HashTable<unsigned int, vulkanTexture, DjB2Hash> VulkanMap;
VulkanMap gVulkanMap;

// How many Vulkan texture maps do we need?
// There are SVR_MAX_RENDER_LAYERS available each frame.
// These things are usually triple buffered.
// Double it cause we want to be safe.
#define MAX_VULKAN_MAP  (2 * 3 * SVR_MAX_RENDER_LAYERS)

enum svrMeshMode {
    kMeshModeColumns = 0,
    kMeshModeRows
};

enum svrMeshDistortion {
    kNoDistortion = 0,
    kBarrleDistortion,
    kSphereDistortion,      // Really "Barrel" but with sphere UVs
};

struct svrMeshLayout {
    glm::vec4 LL_Pos;     // Lower Left:  0 = X-Position; 1 = Y-Position; 2 = Z-Position; 3 = Padding
    glm::vec4 LR_Pos;     // Lower Right: 0 = X-Position; 1 = Y-Position; 2 = Z-Position; 3 = Padding
    glm::vec4 UL_Pos;     // Upper Left:  0 = X-Position; 1 = Y-Position; 2 = Z-Position; 3 = Padding
    glm::vec4 UR_Pos;     // Upper Right: 0 = X-Position; 1 = Y-Position; 2 = Z-Position; 3 = Padding

    glm::vec2 LL_UV;      //Lower Left:  0 = U-Value; 1 = V-Value
    glm::vec2 LR_UV;      //Lower Right: 0 = U-Value; 1 = V-Value
    glm::vec2 UL_UV;      //Upper Left:  0 = U-Value; 1 = V-Value
    glm::vec2 UR_UV;      //Upper Right: 0 = U-Value; 1 = V-Value
};

// These shaders live in svrApiTimeWarpShaders.cpp
extern const char svrBlitQuadVs[];
extern const char svrBlitQuadFs[];
extern const char svrBlitQuadYuvFs[];
extern const char svrBlitQuadFs_Image[];
extern const char svrBlitQuadYuvFs_Image[];
extern const char svrStencilVs[];
extern const char svrStencilArrayVs[];
extern const char svrStencilFs[];

extern const char svrBoundaryMeshVs[];
extern const char svrBoundaryMeshArrayVs[];
extern const char svrBoundaryMeshFs[];

extern const char warpShaderVs[];
extern const char warpShaderFs[];

float ToDegrees = 57.2957795131f;
float PI = 3.1415926535f;
float PI_MUL_2 = 6.2831853072f;
float PI_2 = 1.57079632679489661923;


//-----------------------------------------------------------------------------
void InitializeStencilMesh(SvrGeometry &stencilGeom, svrWhichEye whichEye)
//-----------------------------------------------------------------------------
{
    LOGI("Initializing stencil mesh (%d vertices; %d indices)...", gNumHiddenMeshVerts,
         gNumHiddenMeshIndices);

    // ********************************
    // Vertices
    // ********************************
    float *pLocalVerts = new float[3 * gNumHiddenMeshVerts];
    if (pLocalVerts == NULL) {
        LOGE("Unable to allocate memory for %d stencil mesh vertices!", gNumHiddenMeshVerts);
        return;
    }

    float vertRadius = 0.0f;
    float vertPos[3];

    for (int whichVert = 0; whichVert < gNumHiddenMeshVerts; whichVert++) {
        // Where is this vert?
        vertPos[0] = gHiddenMeshVerts[3 * whichVert + 0];
        vertPos[1] = gHiddenMeshVerts[3 * whichVert + 1];
        vertPos[2] = gHiddenMeshVerts[3 * whichVert + 2];

        // What is the radius of this vert?
        vertRadius =
                (vertPos[0] * vertPos[0]) + (vertPos[1] * vertPos[1]) + (vertPos[2] * vertPos[2]);
        vertRadius = sqrt(vertRadius);

        // If this is one of the inside vertices then adjust it to the new radius
        if (vertRadius < STENCIL_MESH_MAX_RADIUS) {
            // LOGE("Stencil vertice %d has radius %0.2f => Correcting to %0.2f.", whichVert, vertRadius, gStencilMeshRadius);

            // Create a normalized vector in this direction
            if (vertRadius != 0.0f) {
                vertPos[0] /= vertRadius;
                vertPos[1] /= vertRadius;
                vertPos[2] /= vertRadius;
            }

            vertPos[0] *= gStencilMeshRadius;
            vertPos[1] *= gStencilMeshRadius;
            vertPos[2] *= gStencilMeshRadius;

            // This mesh has the odd reflect back in the center
            float invertAmount;
            switch (whichEye) {
                case kLeftEye:
                    if (vertPos[0] > 1.0f) {
                        invertAmount = vertPos[0] - 1.0f;
                        // LOGE("Stencil Mesh X value: %0.2f => %0.2f", vertPos[0], 1.0f - invertAmount);
                        vertPos[0] = 1.0f - invertAmount;
                    }
                    break;

                case kRightEye:
                    if (vertPos[0] < -1.0f) {
                        invertAmount = -1.0f - vertPos[0];
                        // LOGE("Stencil Mesh X value: %0.2f => %0.2f", vertPos[0], -1.0f + invertAmount);
                        vertPos[0] = -1.0f + invertAmount;
                    }
                    break;

                default:
                    LOGE("Invalid Eye specified to InitializeStencilMesh: %d", whichEye);
                    break;
            }

            // These are screen coordinates, so bound them
            if (vertPos[0] < -1.0f)
                vertPos[0] = -1.0f;
            if (vertPos[0] > 1.0f)
                vertPos[0] = 1.0f;

            if (vertPos[1] < -1.0f)
                vertPos[1] = -1.0f;
            if (vertPos[1] > 1.0f)
                vertPos[1] = 1.0f;
        } else {
            // LOGE("Stencil vertice %d has radius %0.2f => Greater than %0.2f so not adjusting.", whichVert, vertRadius, STENCIL_MESH_MAX_RADIUS);
        }


        pLocalVerts[3 * whichVert + 0] = vertPos[0];
        pLocalVerts[3 * whichVert + 1] = vertPos[1];
        pLocalVerts[3 * whichVert + 2] = vertPos[2];
    }

    // ********************************
    // Indices
    // ********************************
    unsigned int *pLocalIndices = new unsigned int[gNumHiddenMeshIndices];
    if (pLocalIndices == NULL) {
        LOGE("Unable to allocate memory for %d stencil mesh indices!", gNumHiddenMeshIndices);
        delete[] pLocalVerts;
        return;
    }

    // Indices are from an OBJ file and so the indices are NOT 0 based.
    for (int whichIndex = 0; whichIndex < gNumHiddenMeshIndices; whichIndex++) {
        pLocalIndices[whichIndex] = gHiddenMeshIndices[whichIndex] - 1;
    }

    // ********************************
    // Geometry
    // ********************************
    SvrProgramAttribute attribs[1] =
            {
                    {
                            kPosition,                  //index
                            3,                          //size
                            GL_FLOAT,                   //type
                            false,                      //normalized
                            3 * sizeof(float),            //stride
                            0                            //offset
                    }
            };

    stencilGeom.Initialize(&attribs[0], 1,
                           pLocalIndices, gNumHiddenMeshIndices,
                           pLocalVerts, 3 * sizeof(float) * gNumHiddenMeshVerts,
                           gNumHiddenMeshVerts);

    // Clean up memory
    delete[] pLocalVerts;
    delete[] pLocalIndices;
}

//-----------------------------------------------------------------------------
void
RenderStencilMesh(SvrAsyncWarpResources *pWarpData, svrWhichEye whichEye, svrTextureType imageType)
//-----------------------------------------------------------------------------
{
    Svr::SvrShader *pCurrentShader = &pWarpData->stencilShader;
    if (imageType == kTypeTextureArray) {
        pCurrentShader = &pWarpData->stencilArrayShader;

        // Only render for left eye if it is an array
        if (whichEye != kLeftEye) {
            return;
        }
    }

    if (pCurrentShader->GetShaderId() == 0) {
        // Shader has not been set up
        LOGE("Unable to render stencil mesh: Stencil Shader NOT initialized!");
        return;
    }

    if (glIsProgram(pCurrentShader->GetShaderId()) == GL_FALSE) {
        // Shader is not a valid program
        LOGE("Unable to render stencil mesh: Stencil Shader NOT valid!");
        return;
    }

    // What is the depth range of the current target
    float DepthRange[2];
    GL(glGetFloatv(GL_DEPTH_RANGE, DepthRange));
    // LOGE("Current Depth Range: [%0.2f, %0.2f]", DepthRange[0], DepthRange[1]);

    // This assumes the correct eye buffer has been bound with viewport and scissor set

    // Bind the stencil shader...
    pCurrentShader->Bind();

    // ... set uniforms ...
    glm::vec4 posScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    pCurrentShader->SetUniformVec4(1, posScaleoffset);

    glm::vec4 depthValue = glm::vec4(DepthRange[0], DepthRange[0], DepthRange[0], DepthRange[0]);
    pCurrentShader->SetUniformVec4(2, depthValue);

    glm::vec4 stencilColor = glm::vec4(gStencilMeshColorR, gStencilMeshColorG, gStencilMeshColorB,
                                       gStencilMeshColorA);
    pCurrentShader->SetUniformVec4(3, stencilColor);

    // ... no textures ...

    // ... render the stencil mesh
    switch (whichEye) {
        case kLeftEye:
            pWarpData->stencilMeshGeomLeft.Submit();
            break;

        case kRightEye:
            pWarpData->stencilMeshGeomRight.Submit();
            break;

        default:
            LOGE("Invalid Eye specified to RenderStencilMesh: %d", whichEye);
            break;
    }

    // No longer want to use the shader
    pCurrentShader->Unbind();
}

//-----------------------------------------------------------------------------
void InitializeBoundaryMesh()
//-----------------------------------------------------------------------------
{
    SvrAsyncWarpResources *pWarpData = &gWarpData;

    // Make sure we are at least 2x2
    if (gBoundaryWarpMeshRows < 2)
        gBoundaryWarpMeshRows = 2;
    if (gBoundaryWarpMeshCols < 2)
        gBoundaryWarpMeshCols = 2;

    unsigned int numRows = gBoundaryWarpMeshRows;
    unsigned int numCols = gBoundaryWarpMeshCols;

    svrMeshMode meshMode = kMeshModeColumns;
    switch (gMeshOrderEnum) {
        case kMeshOrderLeftToRight:
        case kMeshOrderRightToLeft:
            meshMode = kMeshModeColumns;
            break;

        case kMeshOrderTopToBottom:
        case kMeshOrderBottomToTop:
            meshMode = kMeshModeRows;
            break;
    }

    // ********************************
    // Generate the indices for triangles
    // ********************************
    // Each row has numCols quads, each quad is two triangles, each triangle is three indices
    unsigned int maxIndices = numRows * numCols * 2 * 3;
    unsigned int numIndices = 0;

    // Allocate memory for the index data
    unsigned int *indexData = new unsigned int[maxIndices];
    if (indexData == NULL) {
        LOGE("Unable to allocate memory for Boundary mesh indices!");
        return;
    }
    memset(indexData, 0, maxIndices * sizeof(unsigned int));

    // Fill in the index data (Notice loops don't do last ones since we look right and up)
    unsigned int currentIndex = 0;

    switch (meshMode) {
        // ****************************************************
        case kMeshModeColumns:
            // ****************************************************
            if (gMeshOrderEnum == kMeshOrderLeftToRight) {
                for (unsigned int whichX = 0; whichX < numCols; whichX++) {
                    for (unsigned int whichY = 0; whichY < numRows; whichY++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Columns Left To Right
            else if (gMeshOrderEnum == kMeshOrderRightToLeft) // Columns Right To Left
            {
                for (int whichX = numCols - 1; whichX >= 0; whichX--) {
                    for (unsigned int whichY = 0; whichY < numRows; whichY++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Columns Left To Right
            break;

            // ****************************************************
        case kMeshModeRows:
            // ****************************************************
            if (gMeshOrderEnum == kMeshOrderTopToBottom) {
                for (int whichY = numRows - 1; whichY >= 0; whichY--) {
                    for (unsigned int whichX = 0; whichX < numCols; whichX++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Rows Top to Bottom
            else if (gMeshOrderEnum == kMeshOrderBottomToTop) {
                for (unsigned int whichY = 0; whichY < numRows; whichY++) {
                    for (unsigned int whichX = 0; whichX < numCols; whichX++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Rows Bottom to Top
            break;
    }

    LOGI("Boundary mesh has %d indices (out of maximum of %d)", numIndices, maxIndices);

    // ********************************
    // Vertices
    // ********************************
    float pMinPos[3] = {0.0f, 0.0f, 0.0f};
    float pMaxPos[3] = {0.0f, 0.0f, 0.0f};
    float pMinUV[2] = {0.0f, 0.0f};   // Same UV for each face of the Boundary zone
    float pMaxUV[2] = {1.0f, 1.0f};   // Same UV for each face of the Boundary zone
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float deltaZ = 0.0f;

    unsigned int numVerts = (numRows + 1) * (numCols + 1);
    LOGI("Creating Boundary Render Mesh: %d verts in a (%d rows x %d columns) grid", numVerts,
         numRows, numCols);

    // Allocate memory for the vertex data: 3 position + 2 texcoord
    unsigned int numElementsPerVert = 5;
    float *pVertData = new float[numElementsPerVert * numVerts];
    if (pVertData == NULL) {
        LOGE("Unable to allocate %d bytes of memory for Boundary render mesh!",
             (int) (numElementsPerVert * numVerts * sizeof(float)));
        return;
    }
    memset(pVertData, 0, numElementsPerVert * numVerts * sizeof(float));

    // Same UV for each face of the Boundary zone
    for (uint32_t whichMesh = 0; whichMesh < kNumBoundaryMeshes; whichMesh++) {
        // What are the mesh boundaries for this side?
        switch (whichMesh) {
            case kBoundaryMeshPX:
                if (gLogMeshCreation)
                    LOGI("Creating Boundary mesh for PX...");

                pMinPos[0] = gBoundaryZoneMaxX;
                pMinPos[1] = gBoundaryZoneMinY;
                pMinPos[2] = gBoundaryZoneMinZ;

                pMaxPos[0] = gBoundaryZoneMaxX;
                pMaxPos[1] = gBoundaryZoneMaxY;
                pMaxPos[2] = gBoundaryZoneMaxZ;

                deltaX = 0.0f;
                deltaY = (pMaxPos[1] - pMinPos[1]) / (float) numRows;
                deltaZ = (pMaxPos[2] - pMinPos[2]) / (float) numCols;
                break;
            case kBoundaryMeshNX:
                if (gLogMeshCreation)
                    LOGI("Creating Boundary mesh for NX...");

                pMinPos[0] = gBoundaryZoneMinX;
                pMinPos[1] = gBoundaryZoneMinY;
                pMinPos[2] = gBoundaryZoneMaxZ;

                pMaxPos[0] = gBoundaryZoneMinX;
                pMaxPos[1] = gBoundaryZoneMaxY;
                pMaxPos[2] = gBoundaryZoneMinZ;

                deltaX = 0.0f;
                deltaY = (pMaxPos[1] - pMinPos[1]) / (float) numRows;
                deltaZ = (pMaxPos[2] - pMinPos[2]) / (float) numCols;
                break;

            case kBoundaryMeshPY:
                if (gLogMeshCreation)
                    LOGI("Creating Boundary mesh for PY...");

                pMinPos[0] = gBoundaryZoneMinX;
                pMinPos[1] = gBoundaryZoneMaxY;
                pMinPos[2] = gBoundaryZoneMinZ;

                pMaxPos[0] = gBoundaryZoneMaxX;
                pMaxPos[1] = gBoundaryZoneMaxY;
                pMaxPos[2] = gBoundaryZoneMaxZ;

                deltaX = (pMaxPos[0] - pMinPos[0]) / (float) numCols;
                deltaY = 0.0f;
                deltaZ = (pMaxPos[2] - pMinPos[2]) / (float) numRows;
                break;
            case kBoundaryMeshNY:
                if (gLogMeshCreation)
                    LOGI("Creating Boundary mesh for NY...");

                pMinPos[0] = gBoundaryZoneMinX;
                pMinPos[1] = gBoundaryZoneMinY;
                pMinPos[2] = gBoundaryZoneMaxZ;

                pMaxPos[0] = gBoundaryZoneMaxX;
                pMaxPos[1] = gBoundaryZoneMinY;
                pMaxPos[2] = gBoundaryZoneMinZ;

                deltaX = (pMaxPos[0] - pMinPos[0]) / (float) numCols;
                deltaY = 0.0f;
                deltaZ = (pMaxPos[2] - pMinPos[2]) / (float) numRows;
                break;

            case kBoundaryMeshPZ:
                if (gLogMeshCreation)
                    LOGI("Creating Boundary mesh for PZ...");

                pMinPos[0] = gBoundaryZoneMaxX;
                pMinPos[1] = gBoundaryZoneMinY;
                pMinPos[2] = gBoundaryZoneMaxZ;

                pMaxPos[0] = gBoundaryZoneMinX;
                pMaxPos[1] = gBoundaryZoneMaxY;
                pMaxPos[2] = gBoundaryZoneMaxZ;

                deltaX = (pMaxPos[0] - pMinPos[0]) / (float) numCols;
                deltaY = (pMaxPos[1] - pMinPos[1]) / (float) numRows;
                deltaZ = 0.0f;
                break;
            case kBoundaryMeshNZ:
                if (gLogMeshCreation)
                    LOGI("Creating Boundary mesh for NZ...");

                pMinPos[0] = gBoundaryZoneMinX;
                pMinPos[1] = gBoundaryZoneMinY;
                pMinPos[2] = gBoundaryZoneMinZ;

                pMaxPos[0] = gBoundaryZoneMaxX;
                pMaxPos[1] = gBoundaryZoneMaxY;
                pMaxPos[2] = gBoundaryZoneMinZ;

                deltaX = (pMaxPos[0] - pMinPos[0]) / (float) numCols;
                deltaY = (pMaxPos[1] - pMinPos[1]) / (float) numRows;
                deltaZ = 0.0f;
                break;
        }

        float deltaU = (pMaxUV[0] - pMinUV[0]) / (float) numCols;
        float deltaV = (pMaxUV[1] - pMinUV[1]) / (float) numRows;

        // ********************************
        // Fill in the vertex data
        // ********************************
        for (unsigned int whichX = 0; whichX < (numCols + 1); whichX++) {
            for (unsigned int whichY = 0; whichY < (numRows + 1); whichY++) {
                // Based on the mesh type we want to do columns or rows
                unsigned int whichVert = whichX * (numRows + 1) + whichY;   // Columns
                // unsigned int whichVert = whichY * (numCols + 1) + whichX;   // Rows

                unsigned int index = numElementsPerVert * whichVert;

                switch (whichMesh) {
                    case kBoundaryMeshPX:
                    case kBoundaryMeshNX:
                        pVertData[index + 0] = pMinPos[0];
                        pVertData[index + 1] = pMinPos[1] + deltaY * whichY;
                        pVertData[index + 2] = pMinPos[2] + deltaZ * whichX;
                        break;

                    case kBoundaryMeshPY:
                    case kBoundaryMeshNY:
                        pVertData[index + 0] = pMinPos[0] + deltaX * whichX;
                        pVertData[index + 1] = pMinPos[1];
                        pVertData[index + 2] = pMinPos[2] + deltaZ * whichY;
                        break;

                    case kBoundaryMeshPZ:
                    case kBoundaryMeshNZ:
                        pVertData[index + 0] = pMinPos[0] + deltaX * whichX;
                        pVertData[index + 1] = pMinPos[1] + deltaY * whichY;
                        pVertData[index + 2] = pMinPos[2];
                        break;
                }

                pVertData[index + 3] = gBoundaryUVScale * (pMinUV[0] + deltaU * whichX);
                pVertData[index + 4] = gBoundaryUVScale * (pMinUV[1] + deltaV * whichY);

                if (gLogMeshCreation) {
                    LOGI("    Vert [%d,%d]: Pos = (%0.5f, %0.5f, %0.5f); UV = (%0.5f, %0.5f);",
                         whichX, whichY,
                         pVertData[index + 0], pVertData[index + 1], pVertData[index + 2],
                         pVertData[index + 3], pVertData[index + 4]);
                }

            }   // Each Column
        }   // Each Row

        unsigned int numAttribs = 2;
        int stride = (int) (numElementsPerVert * sizeof(float));
        SvrProgramAttribute BoundaryAttribs[2] =
                {
                        //  Index       Size    Type        Normalized      Stride      Offset
                        {kPosition,  3, GL_FLOAT, false, stride, 0},
                        {kTexcoord0, 2, GL_FLOAT, false, stride, 12}
                };

        pWarpData->BoundaryMeshGeom[whichMesh].Initialize(&BoundaryAttribs[0], numAttribs,
                                                          indexData, numIndices,
                                                          pVertData, numElementsPerVert * numVerts *
                                                                     sizeof(float),
                                                          numVerts);

    }   // Each Boundary mesh


    // Clean up local memory
    delete[] pVertData;
    delete[] indexData;

}

//-----------------------------------------------------------------------------
void
RenderBoundaryMesh(SvrAsyncWarpResources *pWarpData, svrWhichEye whichEye, svrTextureType imageType)
//-----------------------------------------------------------------------------
{
    // TODO: What if this is a multi-view eye buffer?

    Svr::SvrShader *pCurrentShader = &pWarpData->BoundaryMeshShader;
    if (imageType == kTypeTextureArray) {
        pCurrentShader = &pWarpData->BoundaryMeshArrayShader;

        // Only render for left eye if it is an array
        if (whichEye != kLeftEye) {
            return;
        }
    }

    if (pCurrentShader->GetShaderId() == 0) {
        // Shader has not been set up
        LOGE("Unable to render Boundary mesh: Boundary Shader NOT initialized!");
        return;
    }

    if (glIsProgram(pCurrentShader->GetShaderId()) == GL_FALSE) {
        // Shader is not a valid program
        LOGE("Unable to render Boundary mesh: Boundary Shader NOT valid!");
        return;
    }

    // Get current head pose to see if we need to render anything
    // No IPD because we don't know what value they are using for the rest of the scene.
    svrHeadPoseState poseState = svrGetPredictedHeadPose(0.0f);
    // glm::fquat poseQuat = glm::fquat(poseState.pose.rotation.w, poseState.pose.rotation.x, poseState.pose.rotation.y, poseState.pose.rotation.z);
    // glm::mat4 viewMat = glm::mat4_cast(poseQuat);

    glm::mat4 leftProjMtx = glm::frustum(gLeftFrustum_Left, gLeftFrustum_Right, gLeftFrustum_Bottom,
                                         gLeftFrustum_Top, gLeftFrustum_Near, gLeftFrustum_Far);
    glm::mat4 rightProjMtx = glm::frustum(gRightFrustum_Left, gRightFrustum_Right,
                                          gRightFrustum_Bottom, gRightFrustum_Top,
                                          gRightFrustum_Near, gRightFrustum_Far);

    glm::mat4 eyeViewMat[2];
    SvrGetEyeViewMatrices(poseState, false, 0.0f, 0.0f, 0.0f, eyeViewMat[0], eyeViewMat[1]);
    // glm::vec3 worldPos = glm::vec3(-eyeViewMat[0][3][0], -eyeViewMat[0][3][1], -eyeViewMat[0][3][2]);
    glm::vec3 worldPos = glm::vec3(-poseState.pose.position.x, -poseState.pose.position.y,
                                   -poseState.pose.position.z);

    // Enable Blending
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glEnable(GL_BLEND));

    // Disable depth write
    GL(glDepthMask(GL_FALSE));

    // Disable Face Culling
    GL(glDisable(GL_CULL_FACE));

    // Bind the boundary shader...
    pCurrentShader->Bind();

    // ... set uniforms ...
    switch (whichEye) {
        case kLeftEye:
            pCurrentShader->SetUniformMat4(1, leftProjMtx);
            break;
        case kRightEye:
            pCurrentShader->SetUniformMat4(1, rightProjMtx);
            break;
        default:
            // Have to bind something
            pCurrentShader->SetUniformMat4(1, leftProjMtx);
            break;
    }

    // These are the same since we set IPD to 0.0
    pCurrentShader->SetUniformMat4(2, eyeViewMat[whichEye]);

    glm::mat4 identMat = glm::mat4(1.0f);
    pCurrentShader->SetUniformMat4(3, identMat);

    // ... textures ...
    // pCurrentShader->SetUniformSampler("srcTex", imageHandle, GL_TEXTURE_2D, 0);

    if (gLogBoundarySystem)
        LOGI("Boundary Position: (%0.2f, %0.2f, %0.2f)", worldPos.x, worldPos.y, worldPos.z);

    // Mesh color based on how close they are
    glm::vec4 BoundaryColor = glm::vec4(gBoundaryMeshColorR, gBoundaryMeshColorG,
                                        gBoundaryMeshColorB, gBoundaryMeshColorA);
    bool collisionOccurred = false;
    bool renderThisSide = false;
    for (uint32_t whichMesh = 0; whichMesh < kNumBoundaryMeshes; whichMesh++) {
        // What are the mesh boundaries for this side?
        renderThisSide = false;
        float lerpValue = 0.0f;

        switch (whichMesh) {
            case kBoundaryMeshPX:
                if (worldPos.x > (gBoundaryZoneMaxX - gBoundaryVisabilityRadius)) {
                    renderThisSide = true;
                    lerpValue = gBoundaryVisabilityScale *
                                fabs(worldPos.x - (gBoundaryZoneMaxX - gBoundaryVisabilityRadius)) /
                                gBoundaryVisabilityRadius;
                }
                break;

            case kBoundaryMeshNX:
                if (worldPos.x < (gBoundaryZoneMinX + gBoundaryVisabilityRadius)) {
                    renderThisSide = true;
                    lerpValue = gBoundaryVisabilityScale *
                                fabs(worldPos.x - (gBoundaryZoneMinX + gBoundaryVisabilityRadius)) /
                                gBoundaryVisabilityRadius;
                }
                break;

            case kBoundaryMeshPY:
                if (worldPos.y > (gBoundaryZoneMaxY - gBoundaryVisabilityRadius)) {
                    renderThisSide = true;
                    lerpValue = gBoundaryVisabilityScale *
                                fabs(worldPos.y - (gBoundaryZoneMaxY - gBoundaryVisabilityRadius)) /
                                gBoundaryVisabilityRadius;
                }
                break;

            case kBoundaryMeshNY:
                if (worldPos.y < (gBoundaryZoneMinY + gBoundaryVisabilityRadius)) {
                    renderThisSide = true;
                    lerpValue = gBoundaryVisabilityScale *
                                fabs(worldPos.y - (gBoundaryZoneMinY + gBoundaryVisabilityRadius)) /
                                gBoundaryVisabilityRadius;
                }
                break;

            case kBoundaryMeshPZ:
                if (worldPos.z > (gBoundaryZoneMaxZ - gBoundaryVisabilityRadius)) {
                    renderThisSide = true;
                    lerpValue = gBoundaryVisabilityScale *
                                fabs(worldPos.z - (gBoundaryZoneMaxZ - gBoundaryVisabilityRadius)) /
                                gBoundaryVisabilityRadius;
                }
                break;

            case kBoundaryMeshNZ:
                if (worldPos.z < (gBoundaryZoneMinZ + gBoundaryVisabilityRadius)) {
                    renderThisSide = true;
                    lerpValue = gBoundaryVisabilityScale *
                                fabs(worldPos.z - (gBoundaryZoneMinZ + gBoundaryVisabilityRadius)) /
                                gBoundaryVisabilityRadius;
                }
                break;
        }

        // Check if forced display from application
        if (gForceDisplayBoundarySystem) {
            renderThisSide = true;
            lerpValue = 1.0f;
        }

        if (lerpValue < 0.0f)
            lerpValue = 0.0f;
        if (lerpValue > 1.0f)
            lerpValue = 1.0f;

        BoundaryColor = lerpValue *
                        glm::vec4(gBoundaryMeshColorR, gBoundaryMeshColorG, gBoundaryMeshColorB,
                                  gBoundaryMeshColorA);
        pCurrentShader->SetUniformVec4(4, BoundaryColor);

        // Boundary Parameters
        // X: Width
        // Y: Not Used
        // Z: Not Used
        // W: Not Used
        glm::vec4 BoundaryParams = glm::vec4(1.0f - gBoundaryGridWidth, 0.0f, 0.0f, 0.0f);
        pCurrentShader->SetUniformVec4(5, BoundaryParams);

        if (renderThisSide) {
            collisionOccurred = true;
            if (gLogBoundarySystem) {
                switch (whichMesh) {
                    case kBoundaryMeshPX:
                        LOGI("    Rendering Boundary mesh: kBoundaryMeshPX");
                        break;

                    case kBoundaryMeshNX:
                        LOGI("    Rendering Boundary mesh: kBoundaryMeshNX");
                        break;

                    case kBoundaryMeshPY:
                        LOGI("    Rendering Boundary mesh: kBoundaryMeshPY");
                        break;

                    case kBoundaryMeshNY:
                        LOGI("    Rendering Boundary mesh: kBoundaryMeshNY");
                        break;

                    case kBoundaryMeshPZ:
                        LOGI("    Rendering Boundary mesh: kBoundaryMeshPZ");
                        break;

                    case kBoundaryMeshNZ:
                        LOGI("    Rendering Boundary mesh: kBoundaryMeshNZ");
                        break;
                }
            }

            pWarpData->BoundaryMeshGeom[whichMesh].Submit();
        }

    }

    // No longer want to use the shader
    pCurrentShader->Unbind();

    // We disabled depth write.  Put it back
    GL(glDepthMask(GL_TRUE));

    // We enabled blending, put it back
    GL(glDisable(GL_BLEND));

    // We disabled Face Culling. Put it back
    GL(glEnable(GL_CULL_FACE));

    // If a collision occurred only want to send one message (not for each eye)
    if (!gForceDisplayBoundarySystem && collisionOccurred && whichEye == kLeftEye) {
        svrEventQueue *pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
        svrEventData data;
        memset(&data, 0, sizeof(svrEventData));
        pQueue->SubmitEvent(kEventBoundarySystemCollision, data);
    }

}

//-----------------------------------------------------------------------------
SvrResult svrBeginEye(svrWhichEye whichEye, svrTextureType imageType)
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("svrBeginEye Failed: Called without VR initialized");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->inVrMode == false) {
        LOGE("svrBeginEye Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    SvrAsyncWarpResources *pWarpData = &gWarpData;

    if (gStencilMeshEnabled && pWarpData->stencilShader.GetShaderId() == 0) {
        // This needs to be created on this thread so the render context is correct!
        InitializeStencilMesh(pWarpData->stencilMeshGeomLeft, kLeftEye);
        InitializeStencilMesh(pWarpData->stencilMeshGeomRight, kRightEye);

        int numVertStrings = 0;
        int numFragStrings = 0;
        const char *vertShaderStrings[16];
        const char *fragShaderStrings[16];

        LOGI("Initializing stencil shader...");
        vertShaderStrings[numVertStrings++] = svrStencilVs;
        fragShaderStrings[numFragStrings++] = svrStencilFs;
        if (!pWarpData->stencilShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings,
                                                 fragShaderStrings, "svrStencilVs",
                                                 "svrStencilFs")) {
            LOGE("Failed to initialize stencilShader");
            return SVR_ERROR_UNKNOWN;
        }

        LOGI("Initializing stencil (Array) shader...");
        numVertStrings = 0;
        numFragStrings = 0;
        vertShaderStrings[numVertStrings++] = svrStencilArrayVs;
        fragShaderStrings[numFragStrings++] = svrStencilFs;
        if (!pWarpData->stencilArrayShader.Initialize(numVertStrings, vertShaderStrings,
                                                      numFragStrings, fragShaderStrings,
                                                      "svrStencilArrayVs", "svrStencilFs")) {
            LOGE("Failed to initialize stencilArrayShader");
            return SVR_ERROR_UNKNOWN;
        }
    }

    if (gStencilMeshEnabled) {
        RenderStencilMesh(&gWarpData, whichEye, imageType);
    }

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult L_InitializeBoundaryMeshSystem()
//-----------------------------------------------------------------------------
{
    SvrAsyncWarpResources *pWarpData = &gWarpData;

    // This needs to be created on this thread so the render context is correct!
    InitializeBoundaryMesh();

    int numVertStrings = 0;
    int numFragStrings = 0;
    const char *vertShaderStrings[16];
    const char *fragShaderStrings[16];

    LOGI("Initializing Boundary shader...");
    vertShaderStrings[numVertStrings++] = svrBoundaryMeshVs;
    fragShaderStrings[numFragStrings++] = svrBoundaryMeshFs;
    if (!pWarpData->BoundaryMeshShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings,
                                                  fragShaderStrings, "svrBoundaryMeshVs",
                                                  "svrBoundaryMeshFs")) {
        LOGE("Failed to initialize BoundaryMeshShader");
        return SVR_ERROR_UNKNOWN;
    }

    LOGI("Initializing Boundary (Array) shader...");
    numVertStrings = 0;
    numFragStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBoundaryMeshArrayVs;
    fragShaderStrings[numFragStrings++] = svrBoundaryMeshFs;
    if (!pWarpData->BoundaryMeshArrayShader.Initialize(numVertStrings, vertShaderStrings,
                                                       numFragStrings, fragShaderStrings,
                                                       "svrBoundaryMeshArrayVs",
                                                       "svrBoundaryMeshFs")) {
        LOGE("Failed to initialize BoundaryMeshArrayShader");
        return SVR_ERROR_UNKNOWN;
    }

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrEndEye(svrWhichEye whichEye, svrTextureType imageType)
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("svrEndEye Failed: Called without VR initialized");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->inVrMode == false) {
        LOGE("svrEndEye Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    SvrAsyncWarpResources *pWarpData = &gWarpData;

    if (gEnableBoundarySystem && gEnableBoundaryMesh) {
        if (pWarpData->BoundaryMeshShader.GetShaderId() == 0)
            L_InitializeBoundaryMeshSystem();

        RenderBoundaryMesh(&gWarpData, whichEye, imageType);
    }

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT bool svrIsBoundarySystemEnabled()
//-----------------------------------------------------------------------------
{
    // For now, the Boundary system means the Boundary mesh
    if (gEnableBoundarySystem && gEnableBoundaryMesh) {
        return true;
    }

    return false;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT void svrEnableBoundarySystem(bool enableBoundary)
//-----------------------------------------------------------------------------
{
    // For now, the Boundary system means the Boundary mesh
    gEnableBoundarySystem = enableBoundary;
    gEnableBoundaryMesh = enableBoundary;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT void svrForceDisplayBoundarySystem(bool forceDisplayBoundary)
//-----------------------------------------------------------------------------
{
    gForceDisplayBoundarySystem = forceDisplayBoundary;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT SvrResult

svrSetBoundaryParameters(float *pMinValues, float *pMaxValues, float visibilityRadius)
//-----------------------------------------------------------------------------
{
    // Do some parameter checking
    if (pMinValues == NULL || pMaxValues == NULL) {
        LOGE("NULL parameters passed to svrSetBoundaryParameters()");
        return SVR_ERROR_UNSUPPORTED;
    }

    if (pMinValues[0] >= pMaxValues[0] ||
        pMinValues[1] >= pMaxValues[1] ||
        pMinValues[2] >= pMaxValues[2]) {
        LOGE("Invalid size parameters passed to svrSetBoundaryParameters()");
        return SVR_ERROR_UNSUPPORTED;
    }

    if (visibilityRadius <= 0.0f ||
        pMaxValues[0] - pMinValues[0] <= 2.0f * visibilityRadius ||
        pMaxValues[1] - pMinValues[1] <= 2.0f * visibilityRadius ||
        pMaxValues[2] - pMinValues[2] <= 2.0f * visibilityRadius) {
        LOGE("Invalid visibility radius passed to svrSetBoundaryParameters()");
        return SVR_ERROR_UNSUPPORTED;
    }

    // Everything was valid.  Must clean up old assets since size has changed
    SvrAsyncWarpResources *pWarpData = &gWarpData;
    for (uint32_t whichMesh = 0; whichMesh < kNumBoundaryMeshes; whichMesh++) {
        pWarpData->BoundaryMeshGeom[whichMesh].Destroy();
    }
    pWarpData->BoundaryMeshShader.Destroy();
    pWarpData->BoundaryMeshArrayShader.Destroy();

    // Assign new values and get out
    gBoundaryZoneMinX = pMinValues[0];
    gBoundaryZoneMinY = pMinValues[1];
    gBoundaryZoneMinZ = pMinValues[2];

    gBoundaryZoneMaxX = pMaxValues[0];
    gBoundaryZoneMaxY = pMaxValues[1];
    gBoundaryZoneMaxZ = pMaxValues[2];

    gBoundaryVisabilityRadius = visibilityRadius;

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT SvrResult

svrGetBoundaryParameters(float *pMinValues, float *pMaxValues, float *pVisibilityRadius)
//-----------------------------------------------------------------------------
{
    // Do some parameter checking
    if (pMinValues == NULL || pMaxValues == NULL || pVisibilityRadius == NULL) {
        LOGE("NULL parameters passed to svrGetBoundaryParameters()");
        return SVR_ERROR_UNSUPPORTED;
    }

    // Read current values and get out
    pMinValues[0] = gBoundaryZoneMinX;
    pMinValues[1] = gBoundaryZoneMinY;
    pMinValues[2] = gBoundaryZoneMinZ;

    pMaxValues[0] = gBoundaryZoneMaxX;
    pMaxValues[1] = gBoundaryZoneMaxY;
    pMaxValues[2] = gBoundaryZoneMaxZ;

    *pVisibilityRadius = gBoundaryVisabilityRadius;

    return SVR_ERROR_NONE;
}


//-----------------------------------------------------------------------------
bool svrUpdateEyeContextSurface()
//-----------------------------------------------------------------------------
{
    LOGI("svrUpdateEyeContextSurface()");

    EGLDisplay eyeDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    LOGI("   Current Display : %p", eyeDisplay);

    //First we need to bind the eye calling threads context to a small pubffer surface so the
    //warp context can take over the window surface
    gAppContext->modeContext->eyeRenderContext = eglGetCurrentContext();
    LOGI("   eyeRenderContext : %p", gAppContext->modeContext->eyeRenderContext);

    gAppContext->modeContext->eyeRenderWarpSurface = eglGetCurrentSurface(EGL_DRAW);
    LOGI("   eyeRenderWarpSurface : %p", gAppContext->modeContext->eyeRenderWarpSurface);

    EGLint largestPBuffer = -1;
    eglQuerySurface(eyeDisplay, gAppContext->modeContext->eyeRenderWarpSurface, EGL_LARGEST_PBUFFER,
                    &largestPBuffer);

    //Can't find any other way to directly query if the current surface for the eye buffer context is a pbuffer other
    //than checking the largest pbuffer attribute.  If the value is unmodified then the surface is a window surface
    //otherwise a pbuffer is already current and we don't need to do anything else.
    if (largestPBuffer == -1) {
        //Save the original surface the application created for the window and its configuration
        gAppContext->modeContext->eyeRenderOrigSurface = eglGetCurrentSurface(EGL_DRAW);
        LOGI("   eyeRenderSurface : %p", gAppContext->modeContext->eyeRenderOrigSurface);

        eglQuerySurface(eyeDisplay, gAppContext->modeContext->eyeRenderOrigSurface, EGL_CONFIG_ID,
                        &gAppContext->modeContext->eyeRenderOrigConfigId);


        EGLConfig pbufferEyeSurfaceConfig = 0;

        EGLint configId;
        eglQueryContext(eyeDisplay, gAppContext->modeContext->eyeRenderContext, EGL_CONFIG_ID,
                        &configId);
        if (0 == configId) {
            configId = 9;
        }
        EGLint numConfigs;
        EGLConfig configs[1024];
        eglGetConfigs(eyeDisplay, configs, 1024, &numConfigs);
        int i = 0;
        for (; i < numConfigs; i++) {
            EGLint tmpConfigId;
            eglGetConfigAttrib(eyeDisplay, configs[i], EGL_CONFIG_ID, &tmpConfigId);
            if (tmpConfigId == configId) {
                pbufferEyeSurfaceConfig = configs[i];
            }
        }
        LOGI("Warp Context Replace Config ID : %d", i);

        // For some reason the current driver is not happy with EGL_PROTECTED_CONTENT_EXT
        // even being in the attrib list. Even if it is EGL_FALSE.
        if (gAppContext->modeContext->isProtectedContent) {
            const EGLint surfaceAttribs[] =
                    {
                            EGL_WIDTH, 16,
                            EGL_HEIGHT, 16,
                            EGL_PROTECTED_CONTENT_EXT, EGL_TRUE,
                            EGL_NONE
                    };

            LOGI("svrUpdateEyeContextSurface : isProtectedContent == TRUE");
            gAppContext->modeContext->eyeRenderWarpSurface = eglCreatePbufferSurface(eyeDisplay,
                                                                                     pbufferEyeSurfaceConfig,
                                                                                     surfaceAttribs);
        } else {
            const EGLint surfaceAttribs[] =
                    {
                            EGL_WIDTH, 16,
                            EGL_HEIGHT, 16,
                            EGL_NONE
                    };

            LOGI("svrUpdateEyeContextSurface : isProtectedContent == FALSE");
            gAppContext->modeContext->eyeRenderWarpSurface = eglCreatePbufferSurface(eyeDisplay,
                                                                                     pbufferEyeSurfaceConfig,
                                                                                     surfaceAttribs);
        }

        if (gAppContext->modeContext->eyeRenderWarpSurface == EGL_NO_SURFACE) {
            LOGE("svrUpdateEyeContextSurface: Failed to create eye context pbuffer surface");
            return false;
        }

        if (eglMakeCurrent(eyeDisplay, gAppContext->modeContext->eyeRenderWarpSurface,
                           gAppContext->modeContext->eyeRenderWarpSurface,
                           gAppContext->modeContext->eyeRenderContext) == EGL_FALSE) {
            LOGE("svrUpdateEyeContextSurface: eglMakeCurrent failed for eye context, 0x%x",
                 eglGetError());

            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool svrCreateWarpContext()
//-----------------------------------------------------------------------------
{
    LOGI("svrCreateWarpContext()");

    if (eglGetCurrentContext() == 0 && eglGetCurrentSurface(EGL_DRAW) == 0) {
        // initialize OpenGL ES and EGL
        gVkConsumer_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        EGLint major, minor;
        eglInitialize(gVkConsumer_eglDisplay, &major, &minor);
        LOGI("EGL version %d.%d", major, minor);
    }

    EGLint format;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config = 0;
    EGLint isProtectedContent = EGL_FALSE;

    svrDeviceInfo di = svrGetDeviceInfo();
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    //Find an appropriate config for our warp context
    EGLConfig configs[512];
    EGLint numConfigs = 0;

    eglGetConfigs(display, NULL, 0, &numConfigs);
    eglGetConfigs(display, configs, 512, &numConfigs);

    LOGI("Found %d EGL configs...", numConfigs);

    //If we are running on Android-M (23) then we can't mutate the surface that was built for the eye buffer context
    //to a single buffer surface so we have to destroy the surface that was bound to the window and create a new surface.
    //If we are running on Android-N (24) or greater then just use the existing surface and mutate it.
    if (true)//di.deviceOSVersion < 24)
    {
        const EGLint attribs[] = {
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 0,
                EGL_NONE
        };

        if (gAppContext->modeContext->eyeRenderOrigSurface != EGL_NO_SURFACE) {
            //Since the existing surface that was created for the eye buffer context is bound to the
            //OS window we won't be able to bind another surface to the window without first destroying the
            //original
            LOGI("Destroying eyeRenderOrigSurface...");
            eglDestroySurface(display, gAppContext->modeContext->eyeRenderOrigSurface);
            gAppContext->modeContext->eyeRenderOrigSurface = EGL_NO_SURFACE;
        }

        //Find an appropriate surface configuration
        int configId;
        for (configId = 0; configId < numConfigs; configId++) {
            EGLint value = 0;

            eglGetConfigAttrib(display, configs[configId], EGL_RENDERABLE_TYPE, &value);
            if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
                LOGI("    EGL config %d rejected: Not ES3 surface", configId);
                continue;
            }

            eglGetConfigAttrib(display, configs[configId], EGL_SURFACE_TYPE, &value);
            if ((value & (EGL_WINDOW_BIT)) != (EGL_WINDOW_BIT)) {
                LOGI("    EGL config %d rejected: Not EGL Window", configId);
                continue;
            }

            int j = 0;
            for (; attribs[j] != EGL_NONE; j += 2) {
                eglGetConfigAttrib(display, configs[configId], attribs[j], &value);
                if (value != attribs[j + 1]) {
                    LOGI("    EGL config %d rejected: Attrib %d (Need %d, got %d)", configId, j,
                         attribs[j + 1], value);
                    break;
                }
            }
            if (attribs[j] == EGL_NONE) {
                LOGI("    EGL config %d Accepted!", configId);
                config = configs[configId];
                break;
            }
        }

        if (config == 0) {
            LOGE("svrCreateWarpContext: Failed to find suitable EGL config");
            return -1;
        }

        //Create the new surface
        EGLint renderBufferType = EGL_BACK_BUFFER;
        if (gSingleBufferWindow) {
            renderBufferType = EGL_SINGLE_BUFFER;
        }

        if (gAppContext->modeContext->isProtectedContent) {
            isProtectedContent = EGL_TRUE;
            LOGI("isProtectedContent is true");
        }

        EGLint colorSpace = gAppContext->modeContext->colorspace;

        if (gForceColorSpace >= 0) {
            if (gForceColorSpace == 0) {
                colorSpace = EGL_COLORSPACE_LINEAR;
            } else if (gForceColorSpace == 1) {
                colorSpace = EGL_COLORSPACE_sRGB;
            } else {
                colorSpace = EGL_COLORSPACE_LINEAR;
            }
        }

        const EGLint windowAttribs[] =
                {
                        EGL_RENDER_BUFFER, renderBufferType,
                        EGL_PROTECTED_CONTENT_EXT, isProtectedContent,
                        EGL_COLORSPACE, colorSpace,
                        EGL_NONE

                };

        surface = eglCreateWindowSurface(display, config, gAppContext->modeContext->nativeWindow,
                                         windowAttribs);

        if (surface == EGL_NO_SURFACE) {

            EGLint error = eglGetError();

            char *pError = NULL;
            switch (error) {
                case EGL_SUCCESS:
                    pError = (char *) "EGL_SUCCESS";
                    break;
                case EGL_NOT_INITIALIZED:
                    pError = (char *) "EGL_NOT_INITIALIZED";
                    break;
                case EGL_BAD_ACCESS:
                    pError = (char *) "EGL_BAD_ACCESS";
                    break;
                case EGL_BAD_ALLOC:
                    pError = (char *) "EGL_BAD_ALLOC";
                    break;
                case EGL_BAD_ATTRIBUTE:
                    pError = (char *) "EGL_BAD_ATTRIBUTE";
                    break;
                case EGL_BAD_CONTEXT:
                    pError = (char *) "EGL_BAD_CONTEXT";
                    break;
                case EGL_BAD_CONFIG:
                    pError = (char *) "EGL_BAD_CONFIG";
                    break;
                case EGL_BAD_CURRENT_SURFACE:
                    pError = (char *) "EGL_BAD_CURRENT_SURFACE";
                    break;
                case EGL_BAD_DISPLAY:
                    pError = (char *) "EGL_BAD_DISPLAY";
                    break;
                case EGL_BAD_SURFACE:
                    pError = (char *) "EGL_BAD_SURFACE";
                    break;
                case EGL_BAD_MATCH:
                    pError = (char *) "EGL_BAD_MATCH";
                    break;
                case EGL_BAD_PARAMETER:
                    pError = (char *) "EGL_BAD_PARAMETER";
                    break;
                case EGL_BAD_NATIVE_PIXMAP:
                    pError = (char *) "EGL_BAD_NATIVE_PIXMAP";
                    break;
                case EGL_BAD_NATIVE_WINDOW:
                    pError = (char *) "EGL_BAD_NATIVE_WINDOW";
                    break;
                case EGL_CONTEXT_LOST:
                    pError = (char *) "EGL_CONTEXT_LOST";
                    break;
                default:
                    pError = (char *) "Unknown EGL Error!";
                    LOGE("Unknown! (0x%x)", error);
                    break;
            }

            LOGE("svrCreateWarpContext: eglCreateWindowSurface failed (Egl Error = %s)", pError);

            return false;
        }
    } else {
        //We will use the existing surface that was created for the eye buffer context
        surface = gAppContext->modeContext->eyeRenderOrigSurface;

        //Find the EGL config for the warp context that matches what was used by the eye
        //buffer context
        for (int i = 0; i < numConfigs; i++) {
            EGLint tmpConfigId;
            eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID, &tmpConfigId);
            if (tmpConfigId == gAppContext->modeContext->eyeRenderOrigConfigId) {
                config = configs[i];
                break;
            }
        }

        //Check if the incoming configuration supports GLES3
        EGLint renderableType = 0;
        eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &renderableType);
        if ((renderableType & EGL_OPENGL_ES3_BIT_KHR) == 0) {
            LOGE("svrCreateWarpContext: Eye Buffer Surface doesn't support GLES3");
        }

        //Log the surface channel bit depths (prefer an 888 surface with no depth)
        EGLint r, g, b, d, s;
        eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &r);
        eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(display, config, EGL_RED_SIZE, &b);
        eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &d);
        eglGetConfigAttrib(display, config, EGL_SAMPLES, &s);

        LOGI("svrCreateWarpContext: Eye Buffer Surface Format : R-%d,G-%d,B-%d,D-%d,S-%d", r, g, b,
             d, s);
    }

    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(gAppContext->modeContext->nativeWindow, di.displayWidthPixels,
                                     di.displayHeightPixels, format);

    //Create the time warp context
    EGLint contextAttribs[] =
            {
                    EGL_CONTEXT_CLIENT_VERSION, 3,
                    EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
                    EGL_PROTECTED_CONTENT_EXT, isProtectedContent,
                    EGL_NONE
            };
    context = eglCreateContext(display, config, gAppContext->modeContext->eyeRenderContext,
                               contextAttribs);

    if (context == EGL_NO_CONTEXT) {
        LOGE("svrCreateWarpContext: Failed to create EGL context");
        return false;
    }

    //Check the context priority that was actually assigned to ensure we are high priority
    EGLint resultPriority;
    eglQueryContext(display, context, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &resultPriority);
    switch (resultPriority) {
        case EGL_CONTEXT_PRIORITY_HIGH_IMG:
            LOGI("svrCreateWarpContext: context has high priority");
            break;
        case EGL_CONTEXT_PRIORITY_MEDIUM_IMG:
            LOGE("svrCreateWarpContext: context has medium priority");
            break;
        case EGL_CONTEXT_PRIORITY_LOW_IMG:
            LOGE("svrCreateWarpContext: context has low priority");
            break;
        default:
            LOGE("svrCreateWarpContext: unknown context priority");
            break;
    }

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("svrCreateWarpContext: eglMakeCurrent failed");
        return false;
    }

    //If we are on Android-N switch the surface to a single buffer surface
    if (gSingleBufferWindow && gAppContext->deviceInfo.deviceOSVersion >= 24) {
        bool bRes = eglSurfaceAttrib(display, surface, EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER);
        if (!bRes) {
            LOGE("svrCreateWarpContext: eglSurfaceAttrib EGL_RENDER_BUFFER to EGL_SINGLE_BUFFER failed.");
            SvrCheckEglError(__FILE__, __LINE__);
        }

        bRes = eglSurfaceAttrib(display, surface, EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID, 1);
        if (!bRes) {
            LOGE("svrCreateWarpContext: eglSurfaceAttrib EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID to true failed.");
            SvrCheckEglError(__FILE__, __LINE__);
        }
    }

    //Verify that we got what we actually asked for
    EGLint acutalRenderBuffer;
    eglQueryContext(display, context, EGL_RENDER_BUFFER, &acutalRenderBuffer);
    if (gSingleBufferWindow) {
        if (gAppContext->deviceInfo.deviceOSVersion >= 24) {
            LOGI("svrCreateWarpContext: Requested mutable EGL_BACK_BUFFER");
        } else {
            LOGI("svrCreateWarpContext: Requested EGL_SINGLE_BUFFER");
        }
    } else {
        LOGI("svrCreateWarpContext: Requested EGL_BACK_BUFFER");
    }

    if (acutalRenderBuffer == EGL_SINGLE_BUFFER) {
        LOGI("    svrCreateWarpContext: Got EGL_SINGLE_BUFFER");
    } else if (acutalRenderBuffer == EGL_BACK_BUFFER) {
        LOGI("    svrCreateWarpContext: Got EGL_BACK_BUFFER");
    } else if (acutalRenderBuffer == EGL_NONE) {
        LOGE("    svrCreateWarpContext: Got EGL_NONE");
    } else {
        LOGE("    svrCreateWarpContext: Got Unknown");
    }


    //Grab some data from the surface and signal we've finished setting up the warp context
    eglQuerySurface(display, surface, EGL_WIDTH, &gAppContext->modeContext->warpRenderSurfaceWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT,
                    &gAppContext->modeContext->warpRenderSurfaceHeight);
    LOGI("svrCreateWarpContext: Warp Surface Dimensions ( %d x %d )",
         gAppContext->modeContext->warpRenderSurfaceWidth,
         gAppContext->modeContext->warpRenderSurfaceHeight);

    gAppContext->modeContext->warpRenderContext = context;
    gAppContext->modeContext->warpRenderSurface = surface;
    LOGI("svrCreateWarpContext: Timewarp context successfully created");

    pthread_mutex_lock(&gAppContext->modeContext->warpThreadContextMutex);
    gAppContext->modeContext->warpContextCreated = true;
    pthread_cond_signal(&gAppContext->modeContext->warpThreadContextCv);
    pthread_mutex_unlock(&gAppContext->modeContext->warpThreadContextMutex);

    return 0;
}

//-----------------------------------------------------------------------------
void svrDestroyWarpContext()
//-----------------------------------------------------------------------------
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    // No longer need the warp surface to be the current surface.
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // If we have Vulkan consumers then clean them up now
    if (gVkConsumer_eglDisplay != EGL_NO_DISPLAY && gVkConsumer_eglSurface != EGL_NO_SURFACE)
        eglDestroySurface(gVkConsumer_eglDisplay, gVkConsumer_eglSurface);
    gVkConsumer_eglSurface = EGL_NO_SURFACE;

    if (gVkConsumer_eglDisplay != EGL_NO_DISPLAY && gVkConsumer_eglContext != EGL_NO_CONTEXT)
        eglDestroyContext(gVkConsumer_eglDisplay, gVkConsumer_eglContext);
    gVkConsumer_eglContext = EGL_NO_CONTEXT;

    gVkConsumer_eglDisplay = EGL_NO_DISPLAY;

    if (gAppContext != NULL && gAppContext->modeContext != NULL) {
        eglDestroySurface(display, gAppContext->modeContext->warpRenderSurface);
        gAppContext->modeContext->warpRenderSurface = EGL_NO_SURFACE;
        eglDestroyContext(display, gAppContext->modeContext->warpRenderContext);
        gAppContext->modeContext->warpRenderContext = EGL_NO_CONTEXT;
    } else {
        LOGE("svrDestroyWarpContext called without valid application or VR mode context");
    }
}

//-----------------------------------------------------------------------------
void GenerateWarpCoords(float *pBaseUV, float *pWarpR, float *pWarpG, float *pWarpB,
                        svrMeshDistortion distortType)
//-----------------------------------------------------------------------------
{
    // If this is going to be a spherical mesh we need to calculate the ratio of FOV to the 360 degrees
    // from the equirectangular texture
    float scaleU = 1.0f;
    float scaleV = 1.0f;
    if (distortType == kSphereDistortion) {
        // NOT taking into account that the left FOV may not be the same as the right FOV.
        float leftThetaA = atan2(fabs(gLeftFrustum_Left), gLeftFrustum_Near);
        float leftThetaB = atan2(fabs(gLeftFrustum_Right), gLeftFrustum_Near);
        float leftPhiA = atan2(fabs(gLeftFrustum_Top), gLeftFrustum_Near);
        float leftPhiB = atan2(fabs(gLeftFrustum_Bottom), gLeftFrustum_Near);

        scaleU = (leftThetaA + leftThetaB) / PI_MUL_2;
        scaleV = (leftPhiA + leftPhiB) / PI;
    }

    // We no longer adjust for the aspect ratio for at least these reasons:  
    //      We don't really know the size of eye buffers, so adjusting may not be correct. 
    //      Zooming in because of the aspect ration wastes pixels that are rendered but never make it to the screen.  
    //      Zooming in messes up the apps field of view.
    //      It makes it harder to tune the lens parameters because things don't move at the same scale as expected.

    // Later: We were having problems getting squares on screen.  They were squished by the aspect
    // ratio of the display.  We had to add back in this code to get them to be square again. This code was removed by #116
    // So here is the core problem: With this zooming the aspect ratio is correct, but it feels like head motion is off
    // because we are zoomed in.  

    // Code From #115 - Begin
    // Adjust the UV coordinates to account for the aspect ratio of the screen
    float aspect = (gAppContext->deviceInfo.displayWidthPixels * 0.5) /
                   gAppContext->deviceInfo.displayHeightPixels;

    if (gMeshAspectRatio > 0.0f)
        aspect = gMeshAspectRatio;

    // Based on the aspect ratio, shrink the range of one axis and then adjust by half to center the image.
    // This makes sure that the image is square
    if (aspect < 1.0f) {
        pBaseUV[0] = (pBaseUV[0] * aspect) + (1.0f - aspect) * 0.5f;
    } else {
        // Need to adjust the vertical axis
        aspect = 1.0f / aspect;
        pBaseUV[1] = (pBaseUV[1] * aspect) + (1.0f - aspect) * 0.5f;
    }
    // Code From #115 - End



    // ******************************************
    // Calculate distortion coordinates
    // ******************************************
    // [0, 1] => [-1, 1]
    float DistCoordX = 2.0f * pBaseUV[0] - 1.0f;
    float DistCoordY = 2.0f * pBaseUV[1] - 1.0f;

    if (gDisableLensCorrection) {
        switch (distortType) {
            case kNoDistortion:
            case kBarrleDistortion:
            case kSphereDistortion:
            default:
                pWarpR[0] = DistCoordX;
                pWarpR[1] = DistCoordY;
                pWarpR[2] = 0.0f;

                pWarpG[0] = DistCoordX;
                pWarpG[1] = DistCoordY;
                pWarpG[2] = 0.0f;

                pWarpB[0] = DistCoordX;
                pWarpB[1] = DistCoordY;
                pWarpB[2] = 0.0f;
                break;
        }

        return;
    }

    // Adjust origin to center of lens rather than center of screen
    DistCoordX -= gLensOffsetX;
    DistCoordY -= gLensOffsetY;

    // Need the radius for the distortion polynomial
    float DistSqrd = (DistCoordX * DistCoordX) + (DistCoordY * DistCoordY);
    float Radius1 = sqrt(DistSqrd);
    float Radius2 = Radius1 * Radius1;
    float Radius3 = Radius2 * Radius1;
    float Radius4 = Radius3 * Radius1;
    float Radius5 = Radius4 * Radius1;
    float Radius6 = Radius5 * Radius1;

    // ******************************************
    // Calculate distortion scale
    // ******************************************
    // Lens Distortion Polynomial: K0 + K1*r + K2*r^2 + K3*r^3 + K4*r^4 + K5*r^5 + K6*r^6
    float DistortionScale = 0.0f;
    DistortionScale += gLensPolyK0;
    DistortionScale += gLensPolyK1 * Radius1;
    DistortionScale += gLensPolyK2 * Radius2;
    DistortionScale += gLensPolyK3 * Radius3;
    DistortionScale += gLensPolyK4 * Radius4;
    DistortionScale += gLensPolyK5 * Radius5;
    DistortionScale += gLensPolyK6 * Radius6;

    if (gLensInverse && DistortionScale != 0.0f)
        DistortionScale = 1.0f / DistortionScale;

    // ******************************************
    // Calculate Chromatic Adjustment
    // ******************************************
    float ChromaScale[3] = {1.0f, 1.0f, 1.0f};

    // Chromatic Aberration Correction: K0 + K1*r + K2*r^2 + K3*r^3 + K4*r^4
    ChromaScale[0] = DistortionScale * (gChromaticPolyK0_R +
                                        gChromaticPolyK1_R * Radius1 +
                                        gChromaticPolyK2_R * Radius2 +
                                        gChromaticPolyK3_R * Radius3 +
                                        gChromaticPolyK4_R * Radius4 +
                                        gChromaticPolyK5_R * Radius5 +
                                        gChromaticPolyK6_R * Radius6);      // Red
    ChromaScale[1] = DistortionScale * (gChromaticPolyK0_G +
                                        gChromaticPolyK1_G * Radius1 +
                                        gChromaticPolyK2_G * Radius2 +
                                        gChromaticPolyK3_G * Radius3 +
                                        gChromaticPolyK4_G * Radius4 +
                                        gChromaticPolyK5_G * Radius5 +
                                        gChromaticPolyK6_G * Radius6);      // Green
    ChromaScale[2] = DistortionScale * (gChromaticPolyK0_B +
                                        gChromaticPolyK1_B * Radius1 +
                                        gChromaticPolyK2_B * Radius2 +
                                        gChromaticPolyK3_B * Radius3 +
                                        gChromaticPolyK4_B * Radius4 +
                                        gChromaticPolyK5_B * Radius5 +
                                        gChromaticPolyK6_B * Radius6);      // Blue

    // ******************************************
    // Scale distortion by scale
    // ******************************************
    float ChromaCoordX[3];
    float ChromaCoordY[3];

    for (int WhichCoord = 0; WhichCoord < 3; WhichCoord++) {
        ChromaCoordX[WhichCoord] = ChromaScale[WhichCoord] * DistCoordX;
        ChromaCoordY[WhichCoord] = ChromaScale[WhichCoord] * DistCoordY;
    }

    // ******************************************
    // Undistort coordinates
    // ******************************************
    float ActualCoordX[3];
    float ActualCoordY[3];
    for (int WhichCoord = 0; WhichCoord < 3; WhichCoord++) {
        // Adjust for aspect ratio
        ActualCoordX[WhichCoord] = ChromaCoordX[WhichCoord];
        ActualCoordY[WhichCoord] = ChromaCoordY[WhichCoord];

        // Adjust origin to center of lens rather than center of screen
        ActualCoordX[WhichCoord] += gLensOffsetX;
        ActualCoordY[WhichCoord] += gLensOffsetY;

        //We want to leave the UVs in NDC space for timewarp
        //The timewarp perspective matrix will put the UV coordinates
        //back to [0,1]
    }

    // ******************************************
    // Set the output values
    // ******************************************
    switch (distortType) {
        case kNoDistortion:
        case kBarrleDistortion:
        case kSphereDistortion:
        default:
            pWarpR[0] = ActualCoordX[0];
            pWarpR[1] = ActualCoordY[0];

            pWarpG[0] = ActualCoordX[1];
            pWarpG[1] = ActualCoordY[1];

            pWarpB[0] = ActualCoordX[2];
            pWarpB[1] = ActualCoordY[2];
            break;
    }

}

//-----------------------------------------------------------------------------
int AddOneQuad(unsigned int currentIndex, unsigned int *indexData, unsigned int numIndices,
               unsigned int numRows, unsigned int numCols)
//-----------------------------------------------------------------------------
{
    unsigned int A = 0;
    unsigned int B = 0;
    unsigned int C = 0;

    int numIndicesAdded = 0;

    // First Triangle
    A = currentIndex;
    B = currentIndex + 1;
    C = currentIndex + (numCols + 1);

    indexData[numIndices++] = A;
    indexData[numIndices++] = B;
    indexData[numIndices++] = C;

    numIndicesAdded += 3;

    if (gLogMeshCreation) {
        LOGI("Adding Triangle: (%d, %d, %d)", A, B, C);
    }

    // Second Triangle
    A = currentIndex + 1;
    B = currentIndex + 1 + (numCols + 1);
    C = currentIndex + (numCols + 1);

    indexData[numIndices++] = A;
    indexData[numIndices++] = B;
    indexData[numIndices++] = C;

    numIndicesAdded += 3;

    if (gLogMeshCreation) {
        LOGI("Adding Triangle: (%d, %d, %d)", A, B, C);
    }

    return numIndicesAdded;

}

//-----------------------------------------------------------------------------
void L_ClampEquiRectScreenPos(glm::vec4 &screenPos)
//-----------------------------------------------------------------------------
{
    // What is the radius in screen coordinates
    float radius = sqrt(screenPos.x * screenPos.x + screenPos.y * screenPos.y);
    float maxRadius = gWarpEqrMeshScale;
    glm::vec2 radialVector = normalize(glm::vec2(screenPos.x, screenPos.y));

    if (gStencilMeshEnabled && gStencilMeshRadius < maxRadius)
        maxRadius = gStencilMeshRadius;

    if (radius > maxRadius) {
        screenPos.x = maxRadius * radialVector.x;
        screenPos.y = maxRadius * radialVector.y;
    }

}

//-----------------------------------------------------------------------------
void L_ClampBarrelMesh(warpMeshVert *pVertData, float lensPolyUnitRadius)
//-----------------------------------------------------------------------------
{
    // What is the radius in screen coordinates
    float xDelta = pVertData->Position[0] / pVertData->Position[3];
    float yDelta = pVertData->Position[1] / pVertData->Position[3];
    float radius = sqrt(xDelta * xDelta + yDelta * yDelta);
    float maxRadius = lensPolyUnitRadius;

    glm::vec2 radialVector = normalize(glm::vec2(xDelta, yDelta));

    // float radiusR = sqrt(pVertData->TexCoordR[0] * pVertData->TexCoordR[0] + pVertData->TexCoordR[1] * pVertData->TexCoordR[1]);
    // float radiusG = sqrt(pVertData->TexCoordG[0] * pVertData->TexCoordG[0] + pVertData->TexCoordG[1] * pVertData->TexCoordG[1]);
    // float radiusB = sqrt(pVertData->TexCoordB[0] * pVertData->TexCoordB[0] + pVertData->TexCoordB[1] * pVertData->TexCoordB[1]);
    // 
    // // Find first color that goes over minimum range
    // radius = radiusR;
    // if (radiusG < radius)
    //     radius = radiusG;
    // if (radiusB < radius)
    //     radius = radiusB;

    if (gStencilMeshEnabled && gStencilMeshRadius < maxRadius)
        maxRadius = gStencilMeshRadius;

    if (radius > maxRadius) {
        // float oldX = pVertData->Position[0];
        // float oldY = pVertData->Position[1];
        pVertData->Position[0] = maxRadius * radialVector.x * pVertData->Position[3];
        pVertData->Position[1] = maxRadius * radialVector.y * pVertData->Position[3];
        // LOGI("DEBUG!     Radial Vector (%0.2f, %0.2f): Radius %0.2f > %0.2f: (%0.2f, %0.2f) -> (%0.2f, %0.2f)", radialVector.x, radialVector.y, radius, maxRadius, oldX, oldY, pVertData->Position[0], pVertData->Position[1]);
    } else {
        // LOGI("DEBUG!     Radial Vector (%0.2f, %0.2f): Radius %0.2f !> %0.2f: No Change for (%0.2f, %0.2f)", radialVector.x, radialVector.y, radius, maxRadius, pVertData->Position[0], pVertData->Position[1]);
    }
}

//-----------------------------------------------------------------------------
void L_AdjustForVignette(warpMeshVert *pVertData, float lensPolyUnitRadius)
//-----------------------------------------------------------------------------
{
    // Set some default values
    pVertData->TexCoordR[3] = 1.0f;
    pVertData->TexCoordG[3] = 1.0f;
    pVertData->TexCoordB[3] = 1.0f;

    if (!gMeshVignette)
        return;

    // What is the radius in screen coordinates
    float xDelta = pVertData->Position[0];
    float yDelta = pVertData->Position[1];
    float radius = sqrt(xDelta * xDelta + yDelta * yDelta);

    // Just put the radius in the attribute. Will be handled by the shader
    pVertData->TexCoordR[3] = radius;
    pVertData->TexCoordG[3] = radius;
    pVertData->TexCoordB[3] = radius;
}


//-----------------------------------------------------------------------------
float L_GetLensPolyUnitRadius()
//-----------------------------------------------------------------------------
{
    // gMeshDiscardUV Causes stretching if no discard in shader (Don't want discard!)
    // That is why it is limited to 1.0 here
    // Later: Talked about it and like the debug option of setting to larger than one.
    // if (gMeshDiscardUV > 1.0f || gMeshDiscardUV <= 0.0f)
    //     gMeshDiscardUV = 1.0f;

    float currentR = 0.0f;
    float lastR = 0.0f;
    float currentTest = 0.0f;
    //float lastTest = 0.0f;
    float deltaR = 0.01f;
    for (uint32_t stepCount = 0; stepCount < 150; stepCount++) {
        currentR = deltaR * (float) stepCount;

        // Need the radius for the distortion polynomial
        float Radius1 = currentR;
        float Radius2 = Radius1 * Radius1;
        float Radius3 = Radius2 * Radius1;
        float Radius4 = Radius3 * Radius1;
        float Radius5 = Radius4 * Radius1;
        float Radius6 = Radius5 * Radius1;

        // ******************************************
        // Calculate distortion scale
        // ******************************************
        // Lens Distortion Polynomial: K0 + K1*r + K2*r^2 + K3*r^3 + K4*r^4 + K5*r^5 + K6*r^6
        float DistortionScale = 0.0f;
        DistortionScale += gLensPolyK0;
        DistortionScale += gLensPolyK1 * Radius1;
        DistortionScale += gLensPolyK2 * Radius2;
        DistortionScale += gLensPolyK3 * Radius3;
        DistortionScale += gLensPolyK4 * Radius4;
        DistortionScale += gLensPolyK5 * Radius5;
        DistortionScale += gLensPolyK6 * Radius6;

        if (gLensInverse && DistortionScale != 0.0f)
            DistortionScale = 1.0f / DistortionScale;

        // Here is the logic hole we found:
        // If the lens polynomial causes the UV values to peak, and then get smaller,
        // the UVs will wrap on themselves around the edges and cause odd effects.
        // So we need to also trap that case.
        // But, on further testing it turns out you can't really catch this case because
        // before going less than last time it slows down so much it ends up repeating 
        // the same pixels.
        currentTest = DistortionScale * currentR;
        // if (currentTest != 0.0f && currentTest <= lastTest)
        // {
        //     LOGE("Error! Lens polynomial has a local maximum. Returning %0.2f", lastR);
        //     return lastR;
        // }

        // LOGI("Lens Polynomial Adjusted Radius: %0.2f => %0.4f", currentR, currentTest);
        if (currentTest >= gMeshDiscardUV) {
            // LOGI("Lens Polynomial causes UV to exceed %0.2f at radius %0.2f. Returning %0.2f", gMeshDiscardUV, currentR, lastR);
            return lastR;
        }

        // Did not use the last value, so save it off
        lastR = currentR;
        //lastTest = currentTest;
    }

    // For some reason never hit the maximum with this polynomial!
    LOGE("Error! Lens polynomial never reached 1.0!");
    return 1.0f;

}

//-----------------------------------------------------------------------------
void L_SetOneSphereVert(warpMeshVert *vertData,
                        uint32_t whichVert,
                        float thisTheta,
                        float thisPhi,
                        float *baseUV,
                        glm::vec4 &sphereScaleOffset,
                        glm::mat4 &thisProjMtx,
                        glm::mat4 &thisInvProjMtx,
                        svrMeshDistortion distortType)
//-----------------------------------------------------------------------------
{
    glm::vec4 thisPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 screenPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 warpWorldPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    // Looking down Z-Axis (Building from Left to Right and Bottom to Top)
    float zPos = -gWarpEqrMeshRadius * cos(thisPhi) * cos(thisTheta);
    float xPos = gWarpEqrMeshRadius * cos(thisPhi) * sin(thisTheta);
    float yPos = gWarpEqrMeshRadius * sin(thisPhi);

    if (gLogMeshCreation)
        LOGI("World Pos: Theta/Phi = %0.2f / %0.2f; (X, Y, Z) = (%0.2f, %0.2f, %0.2f)", thisTheta,
             thisPhi, xPos, yPos, zPos);

    // Need Screen Coordinates
    thisPos = glm::vec4(xPos, yPos, zPos, 1.0f);
    screenPos = thisProjMtx * thisPos;
    screenPos /= screenPos.w;   // Want NDC coordinates so divide by W component

    // Adjust based on stencil mesh and gWarpEqrMeshScale
    if (!gDisableLensCorrection)
        L_ClampEquiRectScreenPos(screenPos);

    // What section of the screen are we on
    screenPos.x *= sphereScaleOffset.x;     // Compiler failed on this: sphereScaleOffset.y * screenPos.y + screenPos.w;
    screenPos.x += sphereScaleOffset.z;
    screenPos.y *= sphereScaleOffset.y;
    screenPos.y += sphereScaleOffset.w;

    vertData[whichVert].Position[0] = screenPos.x;
    vertData[whichVert].Position[1] = screenPos.y;
    vertData[whichVert].Position[2] = screenPos.z;

    if (distortType == kNoDistortion) {
        // TODO: Do we need special handling if this is a spherical mesh?
        // No distortion just mean no chromatic aberration for a spherical mesh
        vertData[whichVert].TexCoordR[0] = xPos;
        vertData[whichVert].TexCoordR[1] = yPos;
        vertData[whichVert].TexCoordR[2] = zPos;
        vertData[whichVert].TexCoordR[3] = 1.0f;

        vertData[whichVert].TexCoordG[0] = xPos;
        vertData[whichVert].TexCoordG[1] = yPos;
        vertData[whichVert].TexCoordG[2] = zPos;
        vertData[whichVert].TexCoordG[3] = 1.0f;

        vertData[whichVert].TexCoordB[0] = xPos;
        vertData[whichVert].TexCoordB[1] = yPos;
        vertData[whichVert].TexCoordB[2] = zPos;
        vertData[whichVert].TexCoordB[3] = 1.0f;
    } else {
        // Get the warp coordinates, calculate the change in Theta and Phi, then update the positions
        warpMeshVert tempVert;
        GenerateWarpCoords(baseUV, tempVert.TexCoordR, tempVert.TexCoordG, tempVert.TexCoordB,
                           distortType);

        float uvRatioU = 1.0f;
        float uvRatioV = 1.0f;

        // Base UV coordinates are in [0, 1] and we need them in [-1, 1]
        float baseUPrime = 2.0f * baseUV[0] - 1.0f;
        float baseVPrime = 2.0f * baseUV[1] - 1.0f;

        // ****************
        // Red Channel
        // ****************
        if (baseUPrime != 0.0f)
            uvRatioU = tempVert.TexCoordR[0] / baseUPrime;

        if (baseVPrime != 0.0f)
            uvRatioV = tempVert.TexCoordR[1] / baseVPrime;

        // Start with screen coordinates...
        thisPos = glm::vec4(xPos, yPos, zPos, 1.0f);
        screenPos = thisProjMtx * thisPos;
        // screenPos /= screenPos.w;   // Want NDC coordinates so divide by W component

        // ... adjust for warp coords (Kind of flattens out since ignores Z, but not noticable)
        screenPos.x *= uvRatioU;
        screenPos.y *= uvRatioV;

        // ... then reproject back the new position
        thisPos = glm::vec4(screenPos.x, screenPos.y, screenPos.z, screenPos.w);
        warpWorldPos = thisInvProjMtx * thisPos;

        vertData[whichVert].TexCoordR[0] = warpWorldPos.x;
        vertData[whichVert].TexCoordR[1] = warpWorldPos.y;
        vertData[whichVert].TexCoordR[2] = warpWorldPos.z;

        if (gLogMeshCreation)
            LOGI("    R: (X, Y, Z) = (%0.2f, %0.2f, %0.2f)", vertData[whichVert].TexCoordR[0],
                 vertData[whichVert].TexCoordR[1], vertData[whichVert].TexCoordR[2]);

        // ****************
        // Green Channel
        // ****************
        if (baseUPrime != 0.0f)
            uvRatioU = tempVert.TexCoordG[0] / baseUPrime;

        if (baseVPrime != 0.0f)
            uvRatioV = tempVert.TexCoordG[1] / baseVPrime;

        // Start with screen coordinates...
        thisPos = glm::vec4(xPos, yPos, zPos, 1.0f);
        screenPos = thisProjMtx * thisPos;
        // screenPos /= screenPos.w;   // Want NDC coordinates so divide by W component

        // ... adjust for warp coords (Kind of flattens out since ignores Z, but not noticable)
        screenPos.x *= uvRatioU;
        screenPos.y *= uvRatioV;

        // ... then reproject back the new position
        thisPos = glm::vec4(screenPos.x, screenPos.y, screenPos.z, screenPos.w);
        warpWorldPos = thisInvProjMtx * thisPos;

        vertData[whichVert].TexCoordG[0] = warpWorldPos.x;
        vertData[whichVert].TexCoordG[1] = warpWorldPos.y;
        vertData[whichVert].TexCoordG[2] = warpWorldPos.z;

        if (gLogMeshCreation)
            LOGI("    G: (X, Y, Z) = (%0.2f, %0.2f, %0.2f)", vertData[whichVert].TexCoordG[0],
                 vertData[whichVert].TexCoordG[1], vertData[whichVert].TexCoordG[2]);

        // ****************
        // Blue Channel
        // ****************
        if (baseUPrime != 0.0f)
            uvRatioU = tempVert.TexCoordB[0] / baseUPrime;

        if (baseVPrime != 0.0f)
            uvRatioV = tempVert.TexCoordB[1] / baseVPrime;

        // Start with screen coordinates...
        thisPos = glm::vec4(xPos, yPos, zPos, 1.0f);
        screenPos = thisProjMtx * thisPos;
        // screenPos /= screenPos.w;   // Want NDC coordinates so divide by W component

        // ... adjust for warp coords (Kind of flattens out since ignores Z, but not noticable)
        screenPos.x *= uvRatioU;
        screenPos.y *= uvRatioV;

        // ... then reproject back the new position
        thisPos = glm::vec4(screenPos.x, screenPos.y, screenPos.z, screenPos.w);
        warpWorldPos = thisInvProjMtx * thisPos;

        vertData[whichVert].TexCoordB[0] = warpWorldPos.x;
        vertData[whichVert].TexCoordB[1] = warpWorldPos.y;
        vertData[whichVert].TexCoordB[2] = warpWorldPos.z;

        if (gLogMeshCreation)
            LOGI("    B: (X, Y, Z) = (%0.2f, %0.2f, %0.2f)", vertData[whichVert].TexCoordB[0],
                 vertData[whichVert].TexCoordB[1], vertData[whichVert].TexCoordB[2]);
    }

    // Handle Vignette
    if (gMeshVignette)
        L_AdjustForVignette(&vertData[whichVert], gLensPolyUnitRadius);
}

//-----------------------------------------------------------------------------
void InitializeWarpMesh(SvrGeometry &warpGeom, svrWarpMeshArea whichMesh, unsigned int numRows,
                        unsigned int numCols, svrMeshLayout &meshLayout, glm::vec4 meshScaleOffset,
                        svrMeshMode meshMode, svrMeshDistortion distortType)
//-----------------------------------------------------------------------------
{
    // Make sure we are at least 2x2
    if (numRows < 2)
        numRows = 2;
    if (numCols < 2)
        numCols = 2;

    unsigned int numVerts = (numRows + 1) * (numCols + 1);
    LOGI("Creating TimeWarp Render Mesh: %d verts in a (%d rows x %d columns) grid", numVerts,
         numRows, numCols);

    // Allocate memory for the vertex data
    warpMeshVert *vertData = new warpMeshVert[numVerts];
    if (vertData == NULL) {
        LOGE("Unable to allocate memory for render mesh!");
        return;
    }
    memset(vertData, 0, numVerts * sizeof(warpMeshVert));

    // Based on the current lens polynomial, what is the radius that gets us to 1.0?
    float lensPolyUnitRadius = L_GetLensPolyUnitRadius();
    gLensPolyUnitRadius = lensPolyUnitRadius;

    bool oldDistortFlag = gDisableLensCorrection;

    // If sphere mesh, figure out what FOV is being used
    // Assume not symmetric frustum so calculate both sides

    float leftThetaA = atan2(fabs(gLeftFrustum_Left), gLeftFrustum_Near);
    float leftThetaB = atan2(fabs(gLeftFrustum_Right), gLeftFrustum_Near);
    float leftPhiA = atan2(fabs(gLeftFrustum_Top), gLeftFrustum_Near);
    float leftPhiB = atan2(fabs(gLeftFrustum_Bottom), gLeftFrustum_Near);

    float rightThetaA = atan2(fabs(gRightFrustum_Left), gRightFrustum_Near);
    float rightThetaB = atan2(fabs(gRightFrustum_Right), gRightFrustum_Near);
    float rightPhiA = atan2(fabs(gRightFrustum_Top), gRightFrustum_Near);
    float rightPhiB = atan2(fabs(gRightFrustum_Bottom), gRightFrustum_Near);

    // Set the projection matrices based on device information from SVR
    glm::mat4 leftProjMtx = glm::frustum(gLeftFrustum_Left, gLeftFrustum_Right, gLeftFrustum_Bottom,
                                         gLeftFrustum_Top, gLeftFrustum_Near, gLeftFrustum_Far);
    glm::mat4 rightProjMtx = glm::frustum(gRightFrustum_Left, gRightFrustum_Right,
                                          gRightFrustum_Bottom, gRightFrustum_Top,
                                          gRightFrustum_Near, gRightFrustum_Far);

    glm::mat4 leftInvProjMtx = inverse(leftProjMtx);
    glm::mat4 rightInvProjMtx = inverse(rightProjMtx);

    glm::mat4 thisProjMtx;
    glm::mat4 thisInvProjMtx;

    if ((true || gLogMeshCreation) && distortType == kSphereDistortion) {
        LOGI("Generating Spherical Warp Mesh:");
        LOGI("    Sphere Radius: %0.2f", gWarpEqrMeshRadius);
        LOGI("    Left Theta: [%0.4f, %0.4f]", -leftThetaA * ToDegrees, leftThetaB * ToDegrees);
        LOGI("    Left Phi: [%0.4f, %0.4f]", -leftPhiB * ToDegrees, leftPhiA * ToDegrees);
        LOGI("    Right Theta: [%0.4f, %0.4f]", -rightThetaA * ToDegrees, rightThetaB * ToDegrees);
        LOGI("    Right Phi: [%0.4f, %0.4f]", -rightPhiB * ToDegrees, rightPhiA * ToDegrees);
    }

    // If this is a spherical mesh, we have some more variables to set up
    bool isSphericalMesh = false;
    float minTheta = 0.0f;
    float deltaTheta = 0.0f;
    float minPhi = 0.0f;
    float deltaPhi = 0.0f;

    // Since end up with screen coordinates, but want to control what part of the screen
    // we are in, we need a scale and offset.
    glm::vec4 sphereScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

    if (distortType == kSphereDistortion) {
        switch (whichMesh) {
            case kMeshLeft_Sphere:
                isSphericalMesh = true;
                minTheta = -leftThetaA;
                deltaTheta = (leftThetaA + leftThetaB) /
                             (float) numCols;    // These are both positive angles
                minPhi = -leftPhiB;
                deltaPhi = (leftPhiA + leftPhiB) /
                           (float) numRows;          // These are both positive angles
                sphereScaleOffset = glm::vec4(0.5f, 1.0f, -0.5f, 0.0f);
                thisProjMtx = leftProjMtx;
                thisInvProjMtx = leftInvProjMtx;
                break;

            case kMeshRight_Sphere:
                isSphericalMesh = true;
                minTheta = -rightThetaA;
                deltaTheta = (rightThetaA + rightThetaB) /
                             (float) numCols;  // These are both positive angles
                minPhi = -rightPhiB;
                deltaPhi = (rightPhiA + rightPhiB) /
                           (float) numRows;        // These are both positive angles
                sphereScaleOffset = glm::vec4(0.5f, 1.0f, 0.5f, 0.0f);
                thisProjMtx = rightProjMtx;
                thisInvProjMtx = rightInvProjMtx;
                break;

            case kMeshUL_Sphere:
                isSphericalMesh = true;
                minTheta = -leftThetaA;
                deltaTheta = (leftThetaA + leftThetaB) /
                             (float) numCols;    // These are both positive angles
                minPhi = 0.0f;
                deltaPhi = rightPhiA / (float) numRows;
                sphereScaleOffset = glm::vec4(0.5f, 1.0f, -0.5f, 0.0f);
                thisProjMtx = leftProjMtx;
                thisInvProjMtx = leftInvProjMtx;
                break;

            case kMeshUR_Sphere:
                isSphericalMesh = true;
                minTheta = -rightThetaA;
                deltaTheta = (rightThetaA + rightThetaB) /
                             (float) numCols;  // These are both positive angles
                minPhi = 0.0f;
                deltaPhi = rightPhiA / (float) numRows;
                sphereScaleOffset = glm::vec4(0.5f, 1.0f, 0.5f, 0.0f);
                thisProjMtx = rightProjMtx;
                thisInvProjMtx = rightInvProjMtx;
                break;

            case kMeshLL_Sphere:
                isSphericalMesh = true;
                minTheta = -leftThetaA;
                deltaTheta = (leftThetaA + leftThetaB) /
                             (float) numCols;    // These are both positive angles
                minPhi = -leftPhiB;
                deltaPhi = leftPhiB / (float) numRows;
                sphereScaleOffset = glm::vec4(0.5f, 1.0f, -0.5f, 0.0f);
                thisProjMtx = leftProjMtx;
                thisInvProjMtx = leftInvProjMtx;
                break;

            case kMeshLR_Sphere:
                isSphericalMesh = true;
                minTheta = -rightThetaA;
                deltaTheta = (rightThetaA + rightThetaB) /
                             (float) numCols;  // These are both positive angles
                minPhi = -rightPhiB;
                deltaPhi = rightPhiA / (float) numRows;
                sphereScaleOffset = glm::vec4(0.5f, 1.0f, 0.5f, 0.0f);
                thisProjMtx = rightProjMtx;
                thisInvProjMtx = rightInvProjMtx;
                break;

            default:
                LOGE("ERROR! Uknown sphere mesh specified (%d)!  Defaulting to normal barrel distortion.",
                     (int) whichMesh);
                break;
        }

        if (gLogMeshCreation && isSphericalMesh) {
            LOGI("Creating spherical mesh. Theta = [%0.2f, %0.2f]; Phi = [%0.2f, %0.2f]",
                 ToDegrees * minTheta,
                 ToDegrees * (minTheta + deltaTheta * (float) numCols),
                 ToDegrees * minPhi,
                 ToDegrees * (minPhi + deltaPhi * (float) numRows));
        }

    }

    // ********************************
    // Fill in the vertex data
    // ********************************
    float baseUV[2];
    for (unsigned int whichX = 0; whichX < (numCols + 1); whichX++) {
        for (unsigned int whichY = 0; whichY < (numRows + 1); whichY++) {
            // Based on the mesh type we want to do columns or rows
            unsigned int whichVert = whichX * (numRows + 1) + whichY;   // Columns
            // unsigned int whichVert = whichY * (numCols + 1) + whichX;   // Rows

            // For Position and UV, need to lerp two ends based on row, then
            // lerp between these two values based on column
            float rowLerp = (float) whichY / (float) numRows;
            float colLerp = (float) whichX / (float) numCols;

            glm::vec4 tempPosL = glm::lerp(meshLayout.LL_Pos, meshLayout.UL_Pos, rowLerp);
            glm::vec4 tempPosR = glm::lerp(meshLayout.LR_Pos, meshLayout.UR_Pos, rowLerp);
            glm::vec4 tempPos = glm::lerp(tempPosL, tempPosR, colLerp);

            glm::vec2 tempUVL = glm::lerp(meshLayout.LL_UV, meshLayout.UL_UV, rowLerp);
            glm::vec2 tempUVR = glm::lerp(meshLayout.LR_UV, meshLayout.UR_UV, rowLerp);
            glm::vec2 tempUV = glm::lerp(tempUVL, tempUVR, colLerp);

            // ************************
            // Position Values
            // ************************
            vertData[whichVert].Position[0] = tempPos.x;
            vertData[whichVert].Position[1] = tempPos.y;
            vertData[whichVert].Position[2] = tempPos.z;
            vertData[whichVert].Position[3] = 1.0f;

            // ************************
            // UV Values
            // ************************
            baseUV[0] = tempUV.x;
            baseUV[1] = tempUV.y;

            // Set the base for vignette
            vertData[whichVert].TexCoordR[3] = 1.0f;
            vertData[whichVert].TexCoordG[3] = 1.0f;
            vertData[whichVert].TexCoordB[3] = 1.0f;

            if (isSphericalMesh) {
                // Where are these spherical coordinates
                float thisTheta = minTheta + deltaTheta * (float) whichX;
                float thisPhi = minPhi + deltaPhi * (float) whichY;
                L_SetOneSphereVert(vertData, whichVert, thisTheta, thisPhi, baseUV,
                                   sphereScaleOffset, thisProjMtx, thisInvProjMtx, distortType);
            }   // isSphericalMesh

            switch (distortType) {
                // TODO: Do we need special handling if this is a spherical mesh?

                case kNoDistortion:
                default:
                    // Don't want distortion on these, but don't want to change the flag
                    gDisableLensCorrection = true;
                    GenerateWarpCoords(baseUV, vertData[whichVert].TexCoordR,
                                       vertData[whichVert].TexCoordG, vertData[whichVert].TexCoordB,
                                       distortType);
                    gDisableLensCorrection = oldDistortFlag;
                    break;

                case kBarrleDistortion:
                    // Warp the base texture coordinates based on lens properties
                    GenerateWarpCoords(baseUV, vertData[whichVert].TexCoordR,
                                       vertData[whichVert].TexCoordG, vertData[whichVert].TexCoordB,
                                       distortType);

                    if (!gDisableLensCorrection)
                        L_ClampBarrelMesh(&vertData[whichVert], gLensPolyUnitRadius);

                    // Handle Vignette
                    if (gMeshVignette)
                        L_AdjustForVignette(&vertData[whichVert], gLensPolyUnitRadius);

                    // Now adjust where we want it to be on screen
                    // LOGI("    ScaleOffset: (%0.5f, %0.5f, %0.5f, %0.5f)", meshScaleOffset.x, meshScaleOffset.y, meshScaleOffset.z, meshScaleOffset.w);
                    vertData[whichVert].Position[0] *= meshScaleOffset.x;
                    vertData[whichVert].Position[0] += meshScaleOffset.z;
                    vertData[whichVert].Position[1] *= meshScaleOffset.y;
                    vertData[whichVert].Position[1] += meshScaleOffset.w;
                    break;

                case kSphereDistortion:
                    // Coordinates already generated above
                    break;
            }

        }   // Each Column
    }   // Each Row

    if (gLogMeshCreation) {
        for (unsigned int whichVert = 0; whichVert < numVerts; whichVert++) {
            if (isSphericalMesh) {
                LOGI("Vert %d of %d: Pos = (%0.5f, %0.5f, %0.5f); UV = (%0.5f, %0.5f);",
                     whichVert + 1, numVerts, vertData[whichVert].Position[0],
                     vertData[whichVert].Position[1], vertData[whichVert].Position[2],
                     vertData[whichVert].TexCoordR[0], vertData[whichVert].TexCoordR[1]);
            } else {
                LOGI("Vert %d of %d: Pos = (%0.5f, %0.5f, %0.5f); UV = (%0.5f, %0.5f);",
                     whichVert + 1, numVerts, vertData[whichVert].Position[0],
                     vertData[whichVert].Position[1], vertData[whichVert].Position[2],
                     vertData[whichVert].TexCoordR[0], vertData[whichVert].TexCoordR[1]);
            }
        }
        LOGI("****************************************");
        LOGI("****************************************");
    }

    // ********************************
    // Generate the indices for triangles
    // ********************************
    // Each row has numCols quads, each quad is two triangles, each triangle is three indices
    unsigned int maxIndices = numRows * numCols * 2 * 3;
    unsigned int numIndices = 0;

    // Allocate memory for the index data
    unsigned int *indexData = new unsigned int[maxIndices];
    if (indexData == NULL) {
        LOGE("Unable to allocate memory for render mesh indices!");
        delete[] vertData;
        return;
    }
    memset(indexData, 0, maxIndices * sizeof(unsigned int));

    // Fill in the index data (Notice loops don't do last ones since we look right and up)
    unsigned int currentIndex = 0;

    switch (meshMode) {
        // ****************************************************
        case kMeshModeColumns:
            // ****************************************************
            if (gMeshOrderEnum == kMeshOrderLeftToRight) {
                for (unsigned int whichX = 0; whichX < numCols; whichX++) {
                    for (unsigned int whichY = 0; whichY < numRows; whichY++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Columns Left To Right
            else if (gMeshOrderEnum == kMeshOrderRightToLeft) // Columns Right To Left
            {
                for (int whichX = numCols - 1; whichX >= 0; whichX--) {
                    for (unsigned int whichY = 0; whichY < numRows; whichY++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Columns Left To Right
            break;

            // ****************************************************
        case kMeshModeRows:
            // ****************************************************
            if (gMeshOrderEnum == kMeshOrderTopToBottom) {
                for (int whichY = numRows - 1; whichY >= 0; whichY--) {
                    for (unsigned int whichX = 0; whichX < numCols; whichX++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Rows Top to Bottom
            else if (gMeshOrderEnum == kMeshOrderBottomToTop) {
                for (unsigned int whichY = 0; whichY < numRows; whichY++) {
                    for (unsigned int whichX = 0; whichX < numCols; whichX++) {
                        // What index is the bottom left corner of this quad
                        currentIndex = whichX * (numRows + 1) + whichY;   // Columns
                        // currentIndex = whichY * (numCols + 1) + whichX;   // Rows

                        numIndices += AddOneQuad(currentIndex, indexData, numIndices, numRows,
                                                 numCols);

                    }   // Each Column
                }   // Each Row
            }   // Rows Bottom to Top
            break;
    }

    LOGI("Warp mesh has %d indices (out of maximum of %d)", numIndices, maxIndices);

    warpGeom.Initialize(&gMeshAttribs[0], NUM_VERT_ATTRIBS,
                        indexData, numIndices,
                        vertData, sizeof(warpMeshVert) * numVerts,
                        numVerts);

    // Clean up local memory
    delete[] vertData;
    delete[] indexData;

}

//-----------------------------------------------------------------------------
void UpdateOverlayGeometry(svrMeshLayout &meshLayout, glm::vec4 meshScaleOffset)
//-----------------------------------------------------------------------------
{
    SvrAsyncWarpResources *pWarpData = &gWarpData;
    SvrGeometry *pOverlayGeom = &pWarpData->warpMeshes[kMeshOverlay];

    uint32_t numRows = gLayerMeshRows;
    uint32_t numCols = gLayerMeshCols;

    // Make sure we are at least 2x2
    if (numRows < 2)
        numRows = 2;
    if (numCols < 2)
        numCols = 2;

    unsigned int numVerts = (numRows + 1) * (numCols + 1);

    // Allocate memory for the vertex data
    // If we were super paranoid we could check that gLayerMeshRows/gLayerMeshCols 
    // has not changed.
    if (gOverlayVertData == NULL) {
        gOverlayVertData = new warpMeshVert[numVerts];
        if (gOverlayVertData == NULL) {
            LOGE("Unable to allocate memory for overlay render mesh!");
            return;
        }
    }
    memset(gOverlayVertData, 0, numVerts * sizeof(warpMeshVert));

    // Will need ratios if quads are not full textures.
    // These ratios could be off if quad doesn't have right angles.
    // For example (LR_UV.x - LL_UV.x) != (UR_UV.x - UL_UV.x)
    float uRatio = fabs(meshLayout.LR_UV.x - meshLayout.LL_UV.x);
    float vRatio = fabs(meshLayout.UL_UV.y - meshLayout.LL_UV.y);

    // ********************************
    // Fill in the vertex data
    // ********************************
    float baseUV[2];
    warpMeshVert tempVert;
    // LOGE("****************************************");
    // LOGE("Overlay Mesh Update (U/V Ratio = %0.2f/%0.2f):", uRatio, vRatio);
    // LOGE("****************************************");
    for (unsigned int whichX = 0; whichX < (numCols + 1); whichX++) {
        for (unsigned int whichY = 0; whichY < (numRows + 1); whichY++) {
            // Based on the mesh type we want to do columns or rows
            unsigned int whichVert = whichX * (numRows + 1) + whichY;   // Columns
            // unsigned int whichVert = whichY * (numCols + 1) + whichX;   // Rows

            // For Position and UV, need to lerp two ends based on row, then
            // lerp between these two values based on column
            float rowLerp = (float) whichY / (float) numRows;
            float colLerp = (float) whichX / (float) numCols;

            glm::vec4 tempPosL = glm::lerp(meshLayout.LL_Pos, meshLayout.UL_Pos, rowLerp);
            glm::vec4 tempPosR = glm::lerp(meshLayout.LR_Pos, meshLayout.UR_Pos, rowLerp);
            glm::vec4 tempPos = glm::lerp(tempPosL, tempPosR, colLerp);

            glm::vec2 tempUVL = glm::lerp(meshLayout.LL_UV, meshLayout.UL_UV, rowLerp);
            glm::vec2 tempUVR = glm::lerp(meshLayout.LR_UV, meshLayout.UR_UV, rowLerp);
            glm::vec2 tempUV = glm::lerp(tempUVL, tempUVR, colLerp);

            // ************************
            // Position Values
            // ************************
            gOverlayVertData[whichVert].Position[0] = tempPos.x;
            gOverlayVertData[whichVert].Position[1] = tempPos.y;
            gOverlayVertData[whichVert].Position[2] = tempPos.z;
            gOverlayVertData[whichVert].Position[3] = tempPos.w;

            // Here is what we need to do: 
            // Figure out the warp coordinate based on screen space!
            // Then scale the UV based on the actual texture coordinates. An offset for a full
            // screen quad is not the same as one for part of an atlas.
            // This routine wants coordinates in [0,1]
            baseUV[0] = 0.5f * tempPos.x + 0.5f;
            baseUV[1] = 0.5f * tempPos.y + 0.5f;
            GenerateWarpCoords(baseUV, tempVert.TexCoordR, tempVert.TexCoordG, tempVert.TexCoordB,
                               kBarrleDistortion);

            gOverlayVertData[whichVert].TexCoordR[0] =
                    tempUV.x + uRatio * (tempVert.TexCoordR[0] - tempPos.x);
            gOverlayVertData[whichVert].TexCoordR[1] =
                    tempUV.y + vRatio * (tempVert.TexCoordR[1] - tempPos.y);

            gOverlayVertData[whichVert].TexCoordG[0] =
                    tempUV.x + uRatio * (tempVert.TexCoordG[0] - tempPos.x);
            gOverlayVertData[whichVert].TexCoordG[1] =
                    tempUV.y + vRatio * (tempVert.TexCoordG[1] - tempPos.y);

            gOverlayVertData[whichVert].TexCoordB[0] =
                    tempUV.x + uRatio * (tempVert.TexCoordB[0] - tempPos.x);
            gOverlayVertData[whichVert].TexCoordB[1] =
                    tempUV.y + vRatio * (tempVert.TexCoordB[1] - tempPos.y);

            // [-1,1] => [0,1]
            // tempVert.TexCoordR[0] = 0.5f * tempVert.TexCoordR[0] + 0.5f;
            // tempVert.TexCoordR[1] = 0.5f * tempVert.TexCoordR[1] + 0.5f;
            // LOGI("Vert %d: Pos: (%0.5f, %0.5f, %0.5f); UV: (%0.5f, %0.5f) => (%0.5f, %0.5f);",  whichVert + 1, 
            //                                                                                     tempPos.x, tempPos.y, tempPos.z,
            //                                                                                     tempUV.x, tempUV.y,
            //                                                                                     gOverlayVertData[whichVert].TexCoordG[0], gOverlayVertData[whichVert].TexCoordG[1]);

            // [0,1] => [-1,1]
            gOverlayVertData[whichVert].TexCoordR[0] =
                    2.0f * gOverlayVertData[whichVert].TexCoordR[0] - 1.0f;
            gOverlayVertData[whichVert].TexCoordR[1] =
                    2.0f * gOverlayVertData[whichVert].TexCoordR[1] - 1.0f;
            gOverlayVertData[whichVert].TexCoordG[0] =
                    2.0f * gOverlayVertData[whichVert].TexCoordG[0] - 1.0f;
            gOverlayVertData[whichVert].TexCoordG[1] =
                    2.0f * gOverlayVertData[whichVert].TexCoordG[1] - 1.0f;
            gOverlayVertData[whichVert].TexCoordB[0] =
                    2.0f * gOverlayVertData[whichVert].TexCoordB[0] - 1.0f;
            gOverlayVertData[whichVert].TexCoordB[1] =
                    2.0f * gOverlayVertData[whichVert].TexCoordB[1] - 1.0f;

            // ************************
            // UV Values
            // ************************
            // baseUV[0] = tempUV.x;
            // baseUV[1] = tempUV.y;

            // Warp the base texture coordinates based on lens properties
            // GenerateWarpCoords(baseUV, gOverlayVertData[whichVert].TexCoordR, gOverlayVertData[whichVert].TexCoordG, gOverlayVertData[whichVert].TexCoordB, kBarrleDistortion);

            // There is a little bit of texture stretching on the edges due to a complicated interaction of lens polynomial,
            // mesh density, mesh placement, texture coordinates, clamp to border, etc.  Therefore, we can pull in the clamping 
            // of the mesh to hide this.
            float scaleFactor = 1.0f;
            if (!gDisableLensCorrection)
                L_ClampBarrelMesh(&gOverlayVertData[whichVert], scaleFactor * gLensPolyUnitRadius);

            // Handle Vignette
            if (gMeshVignette)
                L_AdjustForVignette(&gOverlayVertData[whichVert],
                                    scaleFactor * gLensPolyUnitRadius);

            // Now adjust where we want it to be on screen
            // LOGI("    ScaleOffset: (%0.5f, %0.5f, %0.5f, %0.5f)", meshScaleOffset.x, meshScaleOffset.y, meshScaleOffset.z, meshScaleOffset.w);
            gOverlayVertData[whichVert].Position[0] *= meshScaleOffset.x;
            gOverlayVertData[whichVert].Position[0] +=
                    meshScaleOffset.z * gOverlayVertData[whichVert].Position[3];
            gOverlayVertData[whichVert].Position[1] *= meshScaleOffset.y;
            gOverlayVertData[whichVert].Position[1] +=
                    meshScaleOffset.w * gOverlayVertData[whichVert].Position[3];
        }   // Each Column
    }   // Each Row
    // LOGE("****************************************");

    if (gLogMeshCreation) {
        LOGI("****************************************");
        LOGI("ScaleOffset: (%0.5f, %0.5f, %0.5f, %0.5f)", meshScaleOffset.x, meshScaleOffset.y,
             meshScaleOffset.z, meshScaleOffset.w);
        for (unsigned int whichVert = 0; whichVert < numVerts; whichVert++) {
            LOGI("Vert %d of %d: Pos = (%0.5f, %0.5f, %0.5f, %0.5f); UV = (%0.5f, %0.5f);",
                 whichVert + 1,
                 numVerts,
                 gOverlayVertData[whichVert].Position[0],
                 gOverlayVertData[whichVert].Position[1],
                 gOverlayVertData[whichVert].Position[2],
                 gOverlayVertData[whichVert].Position[3],
                 gOverlayVertData[whichVert].TexCoordR[0],
                 gOverlayVertData[whichVert].TexCoordR[1]);
        }
        LOGI("****************************************");
    }

    // Only have to update the vertex data (Not the indicies)
    pOverlayGeom->Update(gOverlayVertData, sizeof(warpMeshVert) * numVerts, numVerts);

}


//-----------------------------------------------------------------------------
glm::mat4 CalculateWarpMatrix(glm::fquat &origPose, glm::fquat &latestPose, float deltaYaw = 0, float deltaPitch = 0)
//-----------------------------------------------------------------------------
{
    glm::fquat diff = origPose * glm::inverse(latestPose);
    glm::fquat invDiff = glm::inverse(diff);
    invDiff.z *= -1.0f;
    if (!gDisableLensCorrection) {
        return glm::mat4_cast(invDiff);
    }
    glm::vec3 eularRadians = glm::eulerAngles(invDiff);
    glm::mat4 mtx = glm::rotate(glm::mat4(1), eularRadians.z, glm::vec3(0, 0, 1));
    double tanHorizontalRotateRadian = glm::tan(eularRadians.y);
    double tanVerticalRotateRadian = -glm::tan(eularRadians.x);
    float fovY = gAppContext->deviceInfo.leftEyeFrustum.top /
                 gAppContext->deviceInfo.leftEyeFrustum.near;
    float fovX = gAppContext->deviceInfo.leftEyeFrustum.right /
                 gAppContext->deviceInfo.leftEyeFrustum.near;
    mtx[3].x = tanHorizontalRotateRadian / fovX;
    mtx[3].y = (float) (tanVerticalRotateRadian / fovY);
    mtx[3].z = gAppContext->deviceInfo.displayWidthPixels / 2.0f /
               gAppContext->deviceInfo.displayHeightPixels;
    mtx[3].w = 1 + deltaYaw;
    mtx[2].w = deltaPitch;
    return mtx;
}

//-----------------------------------------------------------------------------
glm::mat4 CalculateProjectionMatrix()
//-----------------------------------------------------------------------------
{
    //Project the UVs in NDC onto the far plane and convert from NDC to viewport space
    glm::mat4 retMtx;

    retMtx[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    retMtx[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    retMtx[2] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    retMtx[3] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

    return glm::transpose(retMtx);
}

//-----------------------------------------------------------------------------
bool L_CompileOneWarpShader(uint32_t whichShader)
//-----------------------------------------------------------------------------
{
    SvrAsyncWarpResources *pWarpData = &gWarpData;

    // Now that the map is completed, compile the associated shaders.
    int numVertStrings = 0;
    int numFragStrings = 0;
    const char *vertShaderStrings[32];
    const char *fragShaderStrings[32];

    char vertName[256];
    char fragName[256];

    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = UBER_VERSION_STRING;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = UBER_VERSION_STRING;

    // Must do image extension first because extensions must be defined before "any non-preprocessor tokens"
    if (pWarpData->pWarpShaderMap[whichShader] & kWarpImage) {
        vertShaderStrings[numVertStrings++] = UBER_IMAGE_EXT_STRING;
        fragShaderStrings[numFragStrings++] = UBER_IMAGE_EXT_STRING;
    }

    vertShaderStrings[numVertStrings++] = UBER_PRECISION_STRING;
    fragShaderStrings[numFragStrings++] = UBER_PRECISION_STRING;

#ifdef ENABLE_MOTION_VECTORS
    // If motion vectors are enabled, all shaders support it (not a shader flag)
    if (gLogMotionVectors)
        LOGI("Adding motion vector support to shader: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);

    if (gEnableMotionVectors)
    {
        vertShaderStrings[numVertStrings++] = UBER_SPACEWARP_STRING;
        fragShaderStrings[numFragStrings++] = UBER_SPACEWARP_STRING;
    }

    if (gEnableMotionVectors && gSmoothMotionVectors && gSmoothMotionVectorsWithGPU)
    {
        vertShaderStrings[numVertStrings++] = UBER_SPACEWARP_GPU_STRING;
        fragShaderStrings[numFragStrings++] = UBER_SPACEWARP_GPU_STRING;
    }
#endif // ENABLE_MOTION_VECTORS

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpProjected) {
        vertShaderStrings[numVertStrings++] = UBER_PROJECT_STRING;
        fragShaderStrings[numFragStrings++] = UBER_PROJECT_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpImage) {
        vertShaderStrings[numVertStrings++] = UBER_IMAGE_STRING;
        fragShaderStrings[numFragStrings++] = UBER_IMAGE_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpUvScaleOffset) {
        vertShaderStrings[numVertStrings++] = UBER_UV_SCALE_STRING;
        fragShaderStrings[numFragStrings++] = UBER_UV_SCALE_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpUvClamp) {
        vertShaderStrings[numVertStrings++] = UBER_UV_CLAMP_STRING;
        fragShaderStrings[numFragStrings++] = UBER_UV_CLAMP_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpChroma) {
        vertShaderStrings[numVertStrings++] = UBER_CHROMA_STRING;
        fragShaderStrings[numFragStrings++] = UBER_CHROMA_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpArray) {
        vertShaderStrings[numVertStrings++] = UBER_ARRAY_STRING;
        fragShaderStrings[numFragStrings++] = UBER_ARRAY_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpVignette) {
        vertShaderStrings[numVertStrings++] = UBER_VIGNETTE_STRING;
        fragShaderStrings[numFragStrings++] = UBER_VIGNETTE_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpEquirect) {
        vertShaderStrings[numVertStrings++] = UBER_SPHERE_STRING;
        vertShaderStrings[numVertStrings++] = UBER_EQR_STRING;

        fragShaderStrings[numFragStrings++] = UBER_SPHERE_STRING;
        fragShaderStrings[numFragStrings++] = UBER_EQR_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpCubemap) {
        vertShaderStrings[numVertStrings++] = UBER_SPHERE_STRING;
        vertShaderStrings[numVertStrings++] = UBER_CUBEMAP_STRING;

        fragShaderStrings[numFragStrings++] = UBER_SPHERE_STRING;
        fragShaderStrings[numFragStrings++] = UBER_CUBEMAP_STRING;
    }

    if (pWarpData->pWarpShaderMap[whichShader] & kWarpARDevice) {
        vertShaderStrings[numVertStrings++] = WILLIE_AR_STRING;
        LOGI("willie_test L_CompileOneWarpShader whichShader=%d has kWarpARDevice");
    } else {
        LOGI("willie_test L_CompileOneWarpShader whichShader=%d NOT have kWarpARDevice");
    }

    vertShaderStrings[numVertStrings++] = warpShaderVs;
    fragShaderStrings[numFragStrings++] = warpShaderFs;

    LOGE("Compiling shader %d (Mask = 0x%08x)...", whichShader,
         pWarpData->pWarpShaderMap[whichShader]);
    sprintf(vertName, "Vert: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);
    sprintf(fragName, "Frag: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);
    if (!pWarpData->pWarpShaders[whichShader].Initialize(numVertStrings, vertShaderStrings,
                                                         numFragStrings, fragShaderStrings,
                                                         vertName, fragName)) {
        LOGE("Failed to initialize warp shader: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);
        return false;
    }

    // Shader compiled!
    return true;
}

//-----------------------------------------------------------------------------
bool InitializeAsyncWarpData(SvrAsyncWarpResources *pWarpData)
//-----------------------------------------------------------------------------
{
    svrMeshLayout meshLayout;

    int undistortedRows = 2;
    int undistortedCols = 2;

    glm::vec4 meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

    // For shader compiling
    int numVertStrings = 0;
    int numFragStrings = 0;
    const char *vertShaderStrings[32];
    const char *fragShaderStrings[32];

    // Here is the core problem:
    // Clamp to border is set to black.  Chromatic adjustment says that each color
    // channel has a different UV value.  So, at the edge it is possible that the blue
    // channel is clamped but red and green are not.  This has the effect of blacking
    // out the blue channel only.  So something that is blue-green now is only
    // green and you end up with a green border.
    svrDeviceInfo di = svrGetDeviceInfo();

    float widthAdjust = 0.0f;
    if (di.targetEyeWidthPixels != 0)
        widthAdjust = gChromaticPixelBorder / (float) di.targetEyeWidthPixels;

    float heightAdjust = 0.0f;
    if (di.targetEyeHeightPixels != 0)
        heightAdjust = gChromaticPixelBorder / (float) di.targetEyeHeightPixels;

    switch (gMeshOrderEnum) {
        case kMeshOrderLeftToRight:
        case kMeshOrderRightToLeft:
            LOGI("Creating TimeWarp Render Mesh: Left");    // Columns Left to Right [-1, 0]
            meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.LL_UV = glm::vec2(0.0f + widthAdjust, 0.0f + heightAdjust);
            meshLayout.LR_UV = glm::vec2(1.0f - widthAdjust, 0.0f + heightAdjust);
            meshLayout.UL_UV = glm::vec2(0.0f + widthAdjust, 1.0f - heightAdjust);
            meshLayout.UR_UV = glm::vec2(1.0f - widthAdjust, 1.0f - heightAdjust);

            meshScaleOffset = glm::vec4(0.5f, 1.0f, -0.5f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLeft], kMeshLeft, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeColumns,
                               kBarrleDistortion);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLeft_Low], kMeshLeft_Low, undistortedRows,
                               undistortedCols, meshLayout, meshScaleOffset, kMeshModeColumns,
                               kNoDistortion);

            meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLeft_Sphere], kMeshLeft_Sphere,
                               gWarpMeshRows, gWarpMeshCols, meshLayout, meshScaleOffset,
                               kMeshModeColumns, kSphereDistortion);

            LOGI("Creating TimeWarp Render Mesh: Right");   // Columns Left to Right [0, 1]
            meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.LL_UV = glm::vec2(0.0f + widthAdjust, 0.0f + heightAdjust);
            meshLayout.LR_UV = glm::vec2(1.0f - widthAdjust, 0.0f + heightAdjust);
            meshLayout.UL_UV = glm::vec2(0.0f + widthAdjust, 1.0f - heightAdjust);
            meshLayout.UR_UV = glm::vec2(1.0f - widthAdjust, 1.0f - heightAdjust);

            meshScaleOffset = glm::vec4(0.5f, 1.0f, 0.5f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshRight], kMeshRight, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeColumns,
                               kBarrleDistortion);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshRight_Low], kMeshRight_Low,
                               undistortedRows, undistortedCols, meshLayout, meshScaleOffset,
                               kMeshModeColumns, kNoDistortion);

            meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshRight_Sphere], kMeshRight_Sphere,
                               gWarpMeshRows, gWarpMeshCols, meshLayout, meshScaleOffset,
                               kMeshModeColumns, kSphereDistortion);
            break;

        case kMeshOrderTopToBottom:
        case kMeshOrderBottomToTop:
            LOGI("Creating TimeWarp Render Mesh: Upper Left");    // Rows Top to Bottom [Upper Left]
            meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, 0.0f, 0.0f, 0.0f);
            meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, 0.0f, 0.0f, 0.0f);
            meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.LL_UV = glm::vec2(0.0f + widthAdjust, 0.5f);
            meshLayout.LR_UV = glm::vec2(1.0f - widthAdjust, 0.5f);
            meshLayout.UL_UV = glm::vec2(0.0f + widthAdjust, 1.0f - heightAdjust);
            meshLayout.UR_UV = glm::vec2(1.0f - widthAdjust, 1.0f - heightAdjust);

            meshScaleOffset = glm::vec4(0.5f, 1.0f, -0.5f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshUL], kMeshUL, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kBarrleDistortion);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshUL_Low], kMeshUL_Low, undistortedRows,
                               undistortedCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kNoDistortion);

            meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshUL_Sphere], kMeshUL_Sphere, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kSphereDistortion);

            LOGI("Creating TimeWarp Render Mesh: Upper Right");    // Rows Top to Bottom [Upper Right]
            meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, 0.0f, 0.0f, 0.0f);
            meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, 0.0f, 0.0f, 0.0f);
            meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMaxY, 0.0f, 0.0f);
            meshLayout.LL_UV = glm::vec2(0.0f + widthAdjust, 0.5f);
            meshLayout.LR_UV = glm::vec2(1.0f - widthAdjust, 0.5f);
            meshLayout.UL_UV = glm::vec2(0.0f + widthAdjust, 1.0f - heightAdjust);
            meshLayout.UR_UV = glm::vec2(1.0f - widthAdjust, 1.0f - heightAdjust);

            meshScaleOffset = glm::vec4(0.5f, 1.0f, 0.5f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshUR], kMeshUR, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kBarrleDistortion);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshUR_Low], kMeshUR_Low, undistortedRows,
                               undistortedCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kNoDistortion);

            meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshUR_Sphere], kMeshUR_Sphere, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kSphereDistortion);

            LOGI("Creating TimeWarp Render Mesh: Lower Left");    // Rows Top to Bottom [Lower Left]
            meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, 0.0f, 0.0f, 0.0f);
            meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, 0.0f, 0.0f, 0.0f);
            meshLayout.LL_UV = glm::vec2(0.0f + widthAdjust, 0.0f + heightAdjust);
            meshLayout.LR_UV = glm::vec2(1.0f - widthAdjust, 0.0f + heightAdjust);
            meshLayout.UL_UV = glm::vec2(0.0f + widthAdjust, 0.5f);
            meshLayout.UR_UV = glm::vec2(1.0f - widthAdjust, 0.5f);

            meshScaleOffset = glm::vec4(0.5f, 1.0f, -0.5f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLL], kMeshLL, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kBarrleDistortion);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLL_Low], kMeshLL_Low, undistortedRows,
                               undistortedCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kNoDistortion);

            meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLL_Sphere], kMeshLL_Sphere, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kSphereDistortion);

            LOGI("Creating TimeWarp Render Mesh: Lower Right");    // Rows Top to Bottom [Lower Right]
            meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMinY, 0.0f, 0.0f);
            meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, 0.0f, 0.0f, 0.0f);
            meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, 0.0f, 0.0f, 0.0f);
            meshLayout.LL_UV = glm::vec2(0.0f + widthAdjust, 0.0f + heightAdjust);
            meshLayout.LR_UV = glm::vec2(1.0f - widthAdjust, 0.0f + heightAdjust);
            meshLayout.UL_UV = glm::vec2(0.0f + widthAdjust, 0.5f);
            meshLayout.UR_UV = glm::vec2(1.0f - widthAdjust, 0.5f);

            meshScaleOffset = glm::vec4(0.5f, 1.0f, 0.5f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLR], kMeshLR, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kBarrleDistortion);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLR_Low], kMeshLR_Low, undistortedRows,
                               undistortedCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kNoDistortion);

            meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            InitializeWarpMesh(pWarpData->warpMeshes[kMeshLR_Sphere], kMeshLR_Sphere, gWarpMeshRows,
                               gWarpMeshCols, meshLayout, meshScaleOffset, kMeshModeRows,
                               kSphereDistortion);
            break;
    }

    // No matter what there is only one overlay mesh
    svrMeshMode meshMode = kMeshModeColumns;
    switch (gMeshOrderEnum) {
        case kMeshOrderLeftToRight:
        case kMeshOrderRightToLeft:
            meshMode = kMeshModeColumns;
            break;

        case kMeshOrderTopToBottom:
        case kMeshOrderBottomToTop:
            meshMode = kMeshModeRows;
            break;
    }

    meshLayout.LL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMinY, 0.0f, 0.0f);
    meshLayout.LR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMinY, 0.0f, 0.0f);
    meshLayout.UL_Pos = glm::vec4(gWarpMeshMinX, gWarpMeshMaxY, 0.0f, 0.0f);
    meshLayout.UR_Pos = glm::vec4(gWarpMeshMaxX, gWarpMeshMaxY, 0.0f, 0.0f);
    meshLayout.LL_UV = glm::vec2(0.0f, 0.0f);
    meshLayout.LR_UV = glm::vec2(1.0f, 0.0f);
    meshLayout.UL_UV = glm::vec2(0.0f, 1.0f);
    meshLayout.UR_UV = glm::vec2(1.0f, 1.0f);

    meshScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

    InitializeWarpMesh(pWarpData->warpMeshes[kMeshOverlay], kMeshOverlay, gLayerMeshRows,
                       gLayerMeshCols, meshLayout, meshScaleOffset, meshMode, kBarrleDistortion);

    // Create warp shaders
    pWarpData->numWarpShaders = 0;
    pWarpData->pWarpShaderMap = NULL;
    pWarpData->pWarpShaders = NULL;


    // In order to figure out how many shaders we have make a temp list here.
    uint32_t tempShaderMap[] =
            {
                    // ****************************************************
                    // Need a zero, nothing.
                    // ****************************************************
                    0,

                    // ****************************************************
                    // Most basic shaders with nothing fancy (not even projected)
                    // ****************************************************
                    // Base Version
                    kWarpChroma,
                    kWarpChroma | kWarpVignette,
                    kWarpChroma | kWarpARDevice,

                    kWarpImage | kWarpChroma,
                    kWarpImage | kWarpChroma | kWarpVignette,

                    // EquiRectangular (Texture) Version
                    kWarpChroma | kWarpEquirect,
                    kWarpChroma | kWarpVignette | kWarpEquirect,

                    // EquiRectangular (Image) Version
                    kWarpImage | kWarpChroma | kWarpEquirect,
                    kWarpImage | kWarpChroma | kWarpVignette | kWarpEquirect,

                    // Cubemap (Texture) Version
                    kWarpChroma | kWarpCubemap,
                    kWarpChroma | kWarpVignette | kWarpCubemap,

                    // No Chromatic Versions of above
                    // Base Version
                    kWarpVignette,

                    kWarpImage,
                    kWarpImage | kWarpVignette,

                    // EquiRectangular (Texture) Version
                    kWarpEquirect,
                    kWarpVignette | kWarpEquirect,

                    // EquiRectangular (Image) Version
                    kWarpImage | kWarpEquirect,
                    kWarpImage | kWarpVignette | kWarpEquirect,

                    // Cubemap (Texture) Version
                    kWarpCubemap,
                    kWarpVignette | kWarpCubemap,

                    // ****************************************************
                    // x2: Same as above but with projection
                    // ****************************************************

                    // ****************************************************
                    // x2: Same as above but with UV handling
                    // ****************************************************

                    // ****************************************************
                    // x2: Same as ALL above but with array handling
                    // ****************************************************
            };

    // Verify we don't have any duplicates in the shader list!!
    uint32_t numShadersInMap = sizeof(tempShaderMap) / sizeof(uint32_t);
    LOGE("Checking %d base shader list for duplicate masks...", numShadersInMap);
    for (uint32_t first = 0; first < numShadersInMap; first++) {
        for (uint32_t second = first + 1; second < numShadersInMap; second++) {
            if (tempShaderMap[first] == tempShaderMap[second]) {
                LOGE("ERROR! Shader map %d and %d have the same mask (0x%08x)!", first, second,
                     tempShaderMap[first]);
            }
        }
    }

    // How many base shaders are there?
    uint32_t numBaseShaders = sizeof(tempShaderMap) / sizeof(uint32_t);
    pWarpData->numWarpShaders = numBaseShaders;
    pWarpData->numWarpShaders *= 2;  // For the Projection ones
    pWarpData->numWarpShaders *= 2;  // For the UV Handling ones
    pWarpData->numWarpShaders *= 2;  // For the Array Handling ones
    LOGI("Creating %d warp shaders...", pWarpData->numWarpShaders);

    // Allocate shader map list...
    pWarpData->pWarpShaderMap = new uint32_t[pWarpData->numWarpShaders];
    if (pWarpData->pWarpShaderMap == NULL) {
        LOGE("Unable to allocate shader map for %d shaders!", pWarpData->numWarpShaders);
        return false;
    }
    memset(pWarpData->pWarpShaderMap, 0, pWarpData->numWarpShaders * sizeof(uint32_t));

    // ...and the actual shader list
    pWarpData->pWarpShaders = new Svr::SvrShader[pWarpData->numWarpShaders];
    if (pWarpData->pWarpShaders == NULL) {
        LOGE("Unable to allocate list of %d shaders!", pWarpData->numWarpShaders);
        return false;
    }
    // Do NOT memset this list to clear it!!! Let constructors do their job.

    // Fill in the shader map before creating the actual shaders
    for (uint32_t whichShader = 0; whichShader < numBaseShaders; whichShader++) {
        // First set is just the temp shader map made above
        if (gMeshVignette)
            pWarpData->pWarpShaderMap[whichShader] = tempShaderMap[whichShader] | kWarpVignette;
        else
            pWarpData->pWarpShaderMap[whichShader] = tempShaderMap[whichShader];
    }

    for (uint32_t whichShader = 0; whichShader < numBaseShaders; whichShader++) {
        // Second set is the first set plus projection
        pWarpData->pWarpShaderMap[whichShader + numBaseShaders] =
                pWarpData->pWarpShaderMap[whichShader] | kWarpProjected;
    }

    for (uint32_t whichShader = 0; whichShader < 2 * numBaseShaders; whichShader++) {
        // Third set is above plus UV handling
        pWarpData->pWarpShaderMap[whichShader + 2 * numBaseShaders] =
                pWarpData->pWarpShaderMap[whichShader] | kWarpUvScaleOffset | kWarpUvClamp;
    }

    for (uint32_t whichShader = 0; whichShader < 4 * numBaseShaders; whichShader++) {
        // Third set is above plus array handling
        pWarpData->pWarpShaderMap[whichShader + 4 * numBaseShaders] =
                pWarpData->pWarpShaderMap[whichShader] | kWarpArray;
    }

    // Verify we don't have any duplicates in the shader list!!
    LOGE("Checking expanded shader list for duplicate masks...");
    for (uint32_t first = 0; first < pWarpData->numWarpShaders; first++) {
        // LOGI("Shader Map %d: 0x%08X", first, pWarpData->pWarpShaderMap[first]);

        for (uint32_t second = first + 1; second < pWarpData->numWarpShaders; second++) {
            if (pWarpData->pWarpShaderMap[first] == pWarpData->pWarpShaderMap[second]) {
                LOGE("ERROR! Shader map %d and %d have the same mask (0x%08x)!", first, second,
                     pWarpData->pWarpShaderMap[first]);
            }
        }
    }

    // Since this is large number of shaders that will not all be used, lazy compile them
    // the first time they are needed

#ifdef DEBUG_COMPILE_ALL_SHADERS
    // Now that the map is completed, compile the associated shaders.
    char vertName[256];
    char fragName[256];

    for (uint32_t whichShader = 0; whichShader < pWarpData->numWarpShaders; whichShader++)
    {
        numVertStrings = 0;
        vertShaderStrings[numVertStrings++] = UBER_VERSION_STRING;

        numFragStrings = 0;
        fragShaderStrings[numFragStrings++] = UBER_VERSION_STRING;

        // Must do image extension first because extensions must be defined before "any non-preprocessor tokens"
        if (pWarpData->pWarpShaderMap[whichShader] & kWarpImage)
        {
            vertShaderStrings[numVertStrings++] = UBER_IMAGE_EXT_STRING;
            fragShaderStrings[numFragStrings++] = UBER_IMAGE_EXT_STRING;
        }

        vertShaderStrings[numVertStrings++] = UBER_PRECISION_STRING;
        fragShaderStrings[numFragStrings++] = UBER_PRECISION_STRING;

#ifdef ENABLE_MOTION_VECTORS
        // If motion vectors are enabled, all shaders support it (not a shader flag)
        if (gLogMotionVectors)
            LOGI("Adding motion vector support to shader: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);

        if (gEnableMotionVectors)
        {
            vertShaderStrings[numVertStrings++] = UBER_SPACEWARP_STRING;
            fragShaderStrings[numFragStrings++] = UBER_SPACEWARP_STRING;
        }

        if (gEnableMotionVectors && gSmoothMotionVectors && gSmoothMotionVectorsWithGPU)
        {
            vertShaderStrings[numVertStrings++] = UBER_SPACEWARP_GPU_STRING;
            fragShaderStrings[numFragStrings++] = UBER_SPACEWARP_GPU_STRING;
        }
#endif // ENABLE_MOTION_VECTORS

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpProjected)
        {
            vertShaderStrings[numVertStrings++] = UBER_PROJECT_STRING;
            fragShaderStrings[numFragStrings++] = UBER_PROJECT_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpImage)
        {
            vertShaderStrings[numVertStrings++] = UBER_IMAGE_STRING;
            fragShaderStrings[numFragStrings++] = UBER_IMAGE_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpUvScaleOffset)
        {
            vertShaderStrings[numVertStrings++] = UBER_UV_SCALE_STRING;
            fragShaderStrings[numFragStrings++] = UBER_UV_SCALE_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpUvClamp)
        {
            vertShaderStrings[numVertStrings++] = UBER_UV_CLAMP_STRING;
            fragShaderStrings[numFragStrings++] = UBER_UV_CLAMP_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpChroma)
        {
            vertShaderStrings[numVertStrings++] = UBER_CHROMA_STRING;
            fragShaderStrings[numFragStrings++] = UBER_CHROMA_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpArray)
        {
            vertShaderStrings[numVertStrings++] = UBER_ARRAY_STRING;
            fragShaderStrings[numFragStrings++] = UBER_ARRAY_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpVignette)
        {
            vertShaderStrings[numVertStrings++] = UBER_VIGNETTE_STRING;
            fragShaderStrings[numFragStrings++] = UBER_VIGNETTE_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpEquirect)
        {
            vertShaderStrings[numVertStrings++] = UBER_SPHERE_STRING;
            vertShaderStrings[numVertStrings++] = UBER_EQR_STRING;

            fragShaderStrings[numFragStrings++] = UBER_SPHERE_STRING;
            fragShaderStrings[numFragStrings++] = UBER_EQR_STRING;
        }

        if (pWarpData->pWarpShaderMap[whichShader] & kWarpCubemap)
        {
            vertShaderStrings[numVertStrings++] = UBER_SPHERE_STRING;
            vertShaderStrings[numVertStrings++] = UBER_CUBEMAP_STRING;

            fragShaderStrings[numFragStrings++] = UBER_SPHERE_STRING;
            fragShaderStrings[numFragStrings++] = UBER_CUBEMAP_STRING;
        }

        vertShaderStrings[numVertStrings++] = warpShaderVs;
        fragShaderStrings[numFragStrings++] = warpShaderFs;

        // TODO: Lazy compile these!
        LOGE("Compiling shader %d (Mask = 0x%08x)...", whichShader, pWarpData->pWarpShaderMap[whichShader]);
        sprintf(vertName, "Vert: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);
        sprintf(fragName, "Frag: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);
        if (!pWarpData->pWarpShaders[whichShader].Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, vertName, fragName))
        {
            LOGE("Failed to initialize warp shader: 0x%08x", pWarpData->pWarpShaderMap[whichShader]);
            return false;
        }
    }
#endif // DEBUG_COMPILE_ALL_SHADERS

    {   // Blit Shader
        numVertStrings = 0;
        vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

        numFragStrings = 0;
        fragShaderStrings[numFragStrings++] = svrBlitQuadFs;
        if (!pWarpData->blitShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings,
                                              fragShaderStrings, "svrBlitQuadVs",
                                              "svrBlitQuadFs")) {
            LOGE("Failed to initialize blitShader");
            return false;
        }
    }

    {   // Blit YUV Shader
        numVertStrings = 0;
        vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

        numFragStrings = 0;
        fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs;
        if (!pWarpData->blitYuvShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings,
                                                 fragShaderStrings, "svrBlitQuadVs",
                                                 "svrBlitQuadYuvFs")) {
            LOGE("Failed to initialize blitYuvShader");
            return false;
        }
    }

    {   // Blit Image Shader
        numVertStrings = 0;
        vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

        numFragStrings = 0;
        fragShaderStrings[numFragStrings++] = svrBlitQuadFs_Image;
        if (!pWarpData->blitImageShader.Initialize(numVertStrings, vertShaderStrings,
                                                   numFragStrings, fragShaderStrings,
                                                   "svrBlitQuadVs_Image", "svrBlitQuadFs_Image")) {
            LOGE("Failed to initialize blitShader_Image");
            return false;
        }
    }

    {   // Blit YUV Image Shader
        numVertStrings = 0;
        vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

        numFragStrings = 0;
        fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs_Image;
        if (!pWarpData->blitYuvImageShader.Initialize(numVertStrings, vertShaderStrings,
                                                      numFragStrings, fragShaderStrings,
                                                      "svrBlitQuadVs_Image",
                                                      "svrBlitQuadYuvFs_Image")) {
            LOGE("Failed to initialize blitYuvShader_Image");
            return false;
        }
    }

    // Create the eye and overlay render target
    int localWidth = gAppContext->modeContext->warpRenderSurfaceWidth / 2;
    int localHeight = gAppContext->modeContext->warpRenderSurfaceHeight;

    for (int whichEye = 0; whichEye < kNumEyes; whichEye++) {
        LOGI("Creating eye render target ( %dx%d ), isProtectedContent: %d", localWidth,
             localHeight, gAppContext->modeContext->isProtectedContent);
        pWarpData->eyeTarget[whichEye].Initialize(localWidth, localHeight, 1, GL_RGBA8, false,
                                                  gAppContext->modeContext->isProtectedContent);

        LOGI("Creating overlay render target ( %dx%d )", localWidth, localHeight);
        pWarpData->overlayTarget[whichEye].Initialize(localWidth, localHeight, 1, GL_RGBA8, false,
                                                      gAppContext->modeContext->isProtectedContent);
    }

    // Create the sampler object for reprojected samplers
    float borderColor[] = {gClampBorderColorR, gClampBorderColorG, gClampBorderColorB,
                           gClampBorderColorA};
    GL(glGenSamplers(1, &pWarpData->eyeSamplerObj));

    GL(glSamplerParameteri(pWarpData->eyeSamplerObj, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glSamplerParameteri(pWarpData->eyeSamplerObj, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    if (gClampBorderEnabled) {
        GL(glSamplerParameterfv(pWarpData->eyeSamplerObj, TEXTURE_BORDER_COLOR_EXT,
                                &borderColor[0]));
        GL(glSamplerParameteri(pWarpData->eyeSamplerObj, GL_TEXTURE_WRAP_S, CLAMP_TO_BORDER_EXT));
        GL(glSamplerParameteri(pWarpData->eyeSamplerObj, GL_TEXTURE_WRAP_T, CLAMP_TO_BORDER_EXT));
    }

    // Overlay sampler has different set of color/alpha values
    borderColor[0] = gClampBorderOverlayColorR;
    borderColor[1] = gClampBorderOverlayColorG;
    borderColor[2] = gClampBorderOverlayColorB;
    borderColor[3] = gClampBorderOverlayColorA;
    GL(glGenSamplers(1, &pWarpData->overlaySamplerObj));

    GL(glSamplerParameteri(pWarpData->overlaySamplerObj, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glSamplerParameteri(pWarpData->overlaySamplerObj, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    if (gClampBorderOverlayEnabled) {
        GL(glSamplerParameterfv(pWarpData->overlaySamplerObj, TEXTURE_BORDER_COLOR_EXT,
                                &borderColor[0]));
        GL(glSamplerParameteri(pWarpData->overlaySamplerObj, GL_TEXTURE_WRAP_S,
                               CLAMP_TO_BORDER_EXT));
        GL(glSamplerParameteri(pWarpData->overlaySamplerObj, GL_TEXTURE_WRAP_T,
                               CLAMP_TO_BORDER_EXT));
    }


    return true;
}

//-----------------------------------------------------------------------------
void DestroyAsyncWarpData(SvrAsyncWarpResources *pWarpData)
//-----------------------------------------------------------------------------
{
    for (int whichMesh = 0; whichMesh < kNumWarpMeshAreas; whichMesh++) {
        pWarpData->warpMeshes[whichMesh].Destroy();
    }

    if (gOverlayVertData != NULL)
        delete[] gOverlayVertData;
    gOverlayVertData = NULL;

    // Destroy the shaders...
    for (uint32_t whichShader = 0; whichShader < pWarpData->numWarpShaders; whichShader++) {
        if (pWarpData->pWarpShaders != NULL)
            pWarpData->pWarpShaders[whichShader].Destroy();
    }

    //...delete the shader list...
    if (pWarpData->pWarpShaders != NULL)
        delete[] pWarpData->pWarpShaders;
    pWarpData->pWarpShaders = NULL;

    //...delete the shader map...
    if (pWarpData->pWarpShaderMap != NULL)
        delete[] pWarpData->pWarpShaderMap;
    pWarpData->pWarpShaderMap = NULL;

    //...and finally clear the count of shaders
    pWarpData->numWarpShaders = 0;


    pWarpData->blitShader.Destroy();
    pWarpData->blitYuvShader.Destroy();
    pWarpData->blitImageShader.Destroy();
    pWarpData->blitYuvImageShader.Destroy();

    LOGI("Destroying stencil assets...");
    pWarpData->stencilMeshGeomLeft.Destroy();
    pWarpData->stencilMeshGeomRight.Destroy();
    pWarpData->stencilShader.Destroy();
    pWarpData->stencilArrayShader.Destroy();

    for (int whichEye = 0; whichEye < kNumEyes; whichEye++) {
        pWarpData->eyeTarget[whichEye].Destroy();
        pWarpData->overlayTarget[whichEye].Destroy();
    }

    GL(glDeleteSamplers(1, &pWarpData->eyeSamplerObj));
    GL(glDeleteSamplers(1, &pWarpData->overlaySamplerObj));
}

//-----------------------------------------------------------------------------
void NanoSleep(uint64_t sleepTimeNano)
//-----------------------------------------------------------------------------
{
    uint64_t preSleep = GetTimeNano();

    if (!gBusyWait) {
        timespec t, rem;
        t.tv_sec = 0;
        t.tv_nsec = sleepTimeNano;
        nanosleep(&t, &rem);
    } else {
        while (true) {
            uint64_t timeNow = GetTimeNano();
            if ((timeNow - preSleep) > sleepTimeNano) {
                break;
            }
        }
    }

    uint64_t postSleep = GetTimeNano();

    if (gLogEyeOverSleep) {
        uint64_t sleepDelta = postSleep - preSleep;
        float waitDelta = (float) ((int64_t) sleepDelta - (int64_t) sleepTimeNano);

        if ((waitDelta * NANOSECONDS_TO_MILLISECONDS) > 1.0f) {
            LOGE("NanoSleep: Overslept by %2.3f ms", waitDelta * NANOSECONDS_TO_MILLISECONDS);
        }
    }
}

#ifdef MOTION_TO_PHOTON_TEST
bool bLeftFlash = false;
bool bRightFlash = false;

float GetMtpAmount(glm::fquat latestQuat)
{
    float factor = 0.0f;

    // Compute the difference of quaternions between the current sample and the last
    glm::fquat inverseLast = glm::inverse(gMotionToPhotonLast);
    glm::fquat diffValue = latestQuat * inverseLast;
    gMotionToPhotonW = diffValue.w;

    if (gMotionToPhotonW < 0.)
    {
        gMotionToPhotonW = -gMotionToPhotonW;
    }
    //LOGI("gMotionToPhotonW %f  %f", gMotionToPhotonW,gMotionToPhotonAccThreshold);

    if (gMotionToPhotonAccThreshold > 0.0f)
    {
        factor = 0.0f;

        if (bRightFlash)
        {
            //During the right eye render motion was picked up so automatically
            //flash for this eye then clear the flags
            factor = 1.0f;
            bRightFlash = false;
            bLeftFlash = false;
        }
        else if (gMotionToPhotonW < gMotionToPhotonAccThreshold)
        {
            uint64_t motoPhoTimeStamp = Svr::GetTimeNano();
            if (((motoPhoTimeStamp - gMotionToPhotonTimeStamp) * NANOSECONDS_TO_MILLISECONDS) > 500.0f)
            {
                //It's been more than 1/2s since the last time we flashed
                factor = 1.0f;
                gMotionToPhotonTimeStamp = motoPhoTimeStamp;
                bLeftFlash = true;
            }
        }

    }
    else
    {
        factor = gMotionToPhotonC * gMotionToPhotonW;
        if (factor > 1.0f)
            factor = 1.0f;
    }

    gMotionToPhotonLast = latestQuat;

    return factor;
}
#endif // MOTION_TO_PHOTON_TEST

//-----------------------------------------------------------------------------
unsigned int GetVulkanInteropHandle(svrRenderLayer *pRenderLayer)
//-----------------------------------------------------------------------------
{
    // First, see if this texture has already been imported
    vulkanTexture oneEntry;
    if (gVulkanMap.Find(pRenderLayer->imageHandle, &oneEntry)) {
        // Found the key already in the list
        // LOGI("Found Vulkan texture (%d -> %d) in the list", pRenderLayer->imageHandle, oneEntry.hTexture);
        return oneEntry.hTexture;
    }


    LOGI("Vulkan texture (%d) not in the list", pRenderLayer->imageHandle);

    // First generate the texture...
    GL(glGenTextures(1, &oneEntry.hTexture));

    // ... bind the texture...
    GL(glBindTexture(GL_TEXTURE_2D, oneEntry.hTexture));

    // ... generate the memory handle...
    // This is NOT really glGenMemoryObjectsKHR! It is really glCreateMemoryObjectsEXT
    oneEntry.hMemory = 0;
    GL(glGenMemoryObjectsKHR(1, &oneEntry.hMemory));

    // ... import the memory ...
    oneEntry.memSize = pRenderLayer->vulkanInfo.memSize;
    //LOGI("    hMemory = %d; MemSize = %d", oneEntry.hMemory, oneEntry.memSize);
    GL(glImportMemoryFdKHR(oneEntry.hMemory, oneEntry.memSize, HANDLE_TYPE_OPAQUE_FD_KHR,
                           pRenderLayer->imageHandle));

    // ... and copy the memory into the texture (No MSAA, compression, or anything other than RGB | RGBA]
    oneEntry.numMips = pRenderLayer->vulkanInfo.numMips;
    oneEntry.width = pRenderLayer->vulkanInfo.width;
    oneEntry.height = pRenderLayer->vulkanInfo.height;
    GLenum internalFormat = pRenderLayer->vulkanInfo.bytesPerPixel == 3 ? GL_RGB8 : GL_RGBA8;
    //LOGI("    Mips = %d; Width = %d; Height = %d; Memory = %d", oneEntry.numMips, oneEntry.width, oneEntry.height, oneEntry.hMemory);
    GL(glTexStorageMem2DKHR(GL_TEXTURE_2D, oneEntry.numMips, internalFormat, oneEntry.width,
                            oneEntry.height, oneEntry.hMemory, 0));

    // No longer need a bound texture
    GL(glBindTexture(GL_TEXTURE_2D, 0));

    // Finally, add this one to the map
    LOGI("Adding Vulkan texture (%d -> %d) to the list", pRenderLayer->imageHandle,
         oneEntry.hTexture);
    gVulkanMap.Insert(pRenderLayer->imageHandle, oneEntry);

    // Return the handle to the texture we just created
    return oneEntry.hTexture;
}

//-----------------------------------------------------------------------------
void L_MakeNdcCoords(svrLayoutCoords &ndcCoords, svrLayoutCoords &srcCoords)
//-----------------------------------------------------------------------------
{
    ndcCoords = srcCoords;

    if (ndcCoords.LowerLeftPos[3] != 0.0f) {
        ndcCoords.LowerLeftPos[0] /= ndcCoords.LowerLeftPos[3];
        ndcCoords.LowerLeftPos[1] /= ndcCoords.LowerLeftPos[3];
        ndcCoords.LowerLeftPos[2] /= ndcCoords.LowerLeftPos[3];
        ndcCoords.LowerLeftPos[3] = 1.0f;
    }

    if (ndcCoords.UpperLeftPos[3] != 0.0f) {
        ndcCoords.UpperLeftPos[0] /= ndcCoords.UpperLeftPos[3];
        ndcCoords.UpperLeftPos[1] /= ndcCoords.UpperLeftPos[3];
        ndcCoords.UpperLeftPos[2] /= ndcCoords.UpperLeftPos[3];
        ndcCoords.UpperLeftPos[3] = 1.0f;
    }

    if (ndcCoords.LowerRightPos[3] != 0.0f) {
        ndcCoords.LowerRightPos[0] /= ndcCoords.LowerRightPos[3];
        ndcCoords.LowerRightPos[1] /= ndcCoords.LowerRightPos[3];
        ndcCoords.LowerRightPos[2] /= ndcCoords.LowerRightPos[3];
        ndcCoords.LowerRightPos[3] = 1.0f;
    }

    if (ndcCoords.UpperRightPos[3] != 0.0f) {
        ndcCoords.UpperRightPos[0] /= ndcCoords.UpperRightPos[3];
        ndcCoords.UpperRightPos[1] /= ndcCoords.UpperRightPos[3];
        ndcCoords.UpperRightPos[2] /= ndcCoords.UpperRightPos[3];
        ndcCoords.UpperRightPos[3] = 1.0f;
    }
}

//-----------------------------------------------------------------------------
bool L_NeedsCompositeBuffer(svrFrameParamsInternal *pWarpFrame, uint32_t whichLayer)
//-----------------------------------------------------------------------------
{
    // Define needing composite as any screen param that is NOT full screen but still
    // outside a certain range.
    svrLayoutCoords ndcCoords;
    L_MakeNdcCoords(ndcCoords, pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords);

    if (ndcCoords.LowerLeftPos[0] == -1.0f && ndcCoords.LowerLeftPos[1] == -1.0f &&
        ndcCoords.UpperLeftPos[0] == -1.0f && ndcCoords.UpperLeftPos[1] == 1.0f &&
        ndcCoords.LowerRightPos[0] == 1.0f && ndcCoords.LowerRightPos[1] == -1.0f &&
        ndcCoords.UpperRightPos[0] == 1.0f && ndcCoords.UpperRightPos[1] == 1.0f) {
        // This is a full screen quad (Assume not messing with Z and W)
        // LOGE("Layer is full screen quad => No Composite Needed!");
        return false;
    }

    // Since it is NOT a full screen quad, see if it is over a certain range.
    // Compositing value
    if (ndcCoords.LowerLeftPos[0] < -gCompositeRadius ||
        ndcCoords.LowerLeftPos[1] < -gCompositeRadius ||
        ndcCoords.UpperLeftPos[0] < -gCompositeRadius ||
        ndcCoords.UpperLeftPos[1] > gCompositeRadius ||
        ndcCoords.LowerRightPos[0] > gCompositeRadius ||
        ndcCoords.LowerRightPos[1] < -gCompositeRadius ||
        ndcCoords.UpperRightPos[0] > gCompositeRadius ||
        ndcCoords.UpperRightPos[1] > gCompositeRadius) {
        // This quad is outside the accepted range and needs to be composited
        // LOGE("Layer is away from center => Composite Needed!");
        return true;
    }

    // Didn't satisfy any of our current requirements for compositing
    // LOGE("Layer is in center => No Composite Needed!");
    return false;
}

//-----------------------------------------------------------------------------
void L_UpdateCompositeBuffer(Svr::SvrRenderTarget &whichTarget, svrFrameParamsInternal *pWarpFrame,
                             uint32_t startLayer, svrEyeMask eyeMask, uint32_t layerFlags)
//-----------------------------------------------------------------------------
{
    // Using gWarpData (SvrAsyncWarpResources)
    SvrAsyncWarpResources *pWarpData = &gWarpData;

    if (gLogEyeRender)
        LOGI("        Updating Compositing buffer 0x%08x...", whichTarget.GetColorAttachment());
    if (gLogEyeRender)
        LOGI("            Start Layer = %d; Eye Mask = %d; Layer flag = 0x%08x", startLayer,
             eyeMask, layerFlags);
    whichTarget.Bind();

    // LOGE("        Viewport: %d, %d, %d, %d", 0, 0, whichTarget.GetWidth(), whichTarget.GetHeight());
    GL(glViewport(0, 0, whichTarget.GetWidth(), whichTarget.GetHeight()));
    GL(glScissor(0, 0, whichTarget.GetWidth(), whichTarget.GetHeight()));

    // Composite buffer is always alpha
    GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // Doesn't currently have a depth buffer

    // 4 points.  Position needs 4 floats per point.  UV needs 2 floats per point
    float quadPos[16];
    float quadUVs[8];

    unsigned int quadIndices[6] = {0, 2, 1, 1, 2, 3};
    int indexCount = sizeof(quadIndices) / sizeof(unsigned int);

    glm::vec4 posScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

    for (uint32_t whichLayer = startLayer; whichLayer < SVR_MAX_RENDER_LAYERS; whichLayer++) {
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle != 0) {
            if (gLogEyeRender)
                LOGI("        Layer %d: Eye Mask = %d; Layer flag = 0x%08x", whichLayer,
                     pWarpFrame->frameParams.renderLayers[whichLayer].eyeMask,
                     pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags);
        }

        // If nothing on this layer then done
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle == 0) {
            if (gLogEyeRender) LOGI("        Skipping Layer %d: No image handle", whichLayer);
            break;
        }

        // If this layer is NOT for this eye then continue
        if ((eyeMask & pWarpFrame->frameParams.renderLayers[whichLayer].eyeMask) == 0) {
            if (gLogEyeRender) LOGI("        Skipping Layer %d: Eye mask disqualifies", whichLayer);
            continue;
        }

        // If this is not the correct loop then don't do it (ALL flags must match)
        if (layerFlags != pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags) {
            if (gLogEyeRender)
                LOGI("        Skipping Layer %d: Layer flag (0x%08x) mismatch (Mask = 0x%08x)",
                     whichLayer, pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags,
                     layerFlags);
            continue;
        }

        // Once we start compositing, don't support cubemaps or equirectangular
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectTexture ||
            pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectImage ||
            pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeCubemapTexture) {
            if (gLogEyeRender)
                LOGI("        Skipping Layer %d: Image type disqualifies", whichLayer);
            continue;
        }

        // Blending is set per layer
        if (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags & kLayerFlagOpaque) {
            if (gLogEyeRender) LOGI("        Layer %d: Blending disabled", whichLayer);
            GL(glDisable(GL_BLEND));
        } else {
            if (gLogEyeRender) LOGI("        Layer %d: Blending enabled", whichLayer);
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            GL(glEnable(GL_BLEND));
        }

        // Update the attribute stream for this quad
        quadPos[0] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[0];
        quadPos[1] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[1];
        quadPos[2] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[2];
        quadPos[3] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[3];
        quadUVs[0] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[0];
        quadUVs[1] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[1];

        quadPos[4] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[0];
        quadPos[5] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[1];
        quadPos[6] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[2];
        quadPos[7] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[3];
        quadUVs[2] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[0];
        quadUVs[3] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[1];

        quadPos[8] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[0];
        quadPos[9] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[1];
        quadPos[10] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[2];
        quadPos[11] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[3];
        quadUVs[4] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[2];
        quadUVs[5] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[3];

        quadPos[12] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[0];
        quadPos[13] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[1];
        quadPos[14] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[2];
        quadPos[15] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[3];
        quadUVs[6] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[2];
        quadUVs[7] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[3];

        GL(glDisable(GL_CULL_FACE));
        GL(glDisable(GL_DEPTH_TEST));

        GL(glBindVertexArray(0));
        GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));


        // Bind the blit shader...
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage) {
            pWarpData->blitImageShader.Bind();
        } else {
            pWarpData->blitShader.Bind();
        }

        // ... set attributes ...
        GL(glEnableVertexAttribArray(kPosition));
        GL(glVertexAttribPointer(kPosition, 4, GL_FLOAT, 0, 0, quadPos));

        GL(glEnableVertexAttribArray(kTexcoord0));
        GL(glVertexAttribPointer(kTexcoord0, 2, GL_FLOAT, 0, 0, quadUVs));

        // ... set uniforms ...
        pWarpData->blitShader.SetUniformVec4(1, posScaleoffset);

        // ... set textures ...
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage) {
            pWarpData->blitShader.SetUniformSampler("srcTex",
                                                    pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle,
                                                    GL_TEXTURE_EXTERNAL_OES, 0);
        } else {
            int imageHandle = 0;

            if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeVulkan) {
                imageHandle = GetVulkanInteropHandle(
                        &pWarpFrame->frameParams.renderLayers[whichLayer]);
            } else {
                imageHandle = pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle;
            }

            pWarpData->blitShader.SetUniformSampler("srcTex", imageHandle, GL_TEXTURE_2D, 0);
        }

        // ... render the quad ...
        GL(glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, quadIndices));

        // ... and disable attributes
        GL(glDisableVertexAttribArray(kPosition));
        GL(glDisableVertexAttribArray(kTexcoord0));

        // Unbind the blit shader...
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage) {
            pWarpData->blitImageShader.Unbind();
        } else {
            pWarpData->blitShader.Unbind();
        }

    }   // Each Layer

    // We turned on blending, so turn it off
    GL(glDisable(GL_BLEND));

    whichTarget.Unbind();
    if (gLogEyeRender)
        LOGI("        Compositing buffer updated (0x%08x)...", whichTarget.GetColorAttachment());
}

enum svrSurfaceSubset {
    kIgnore,
    kEntire,
    kLeftHalf,
    kRightHalf,
    kTopHalf,
    KBottomHalf,
    kLowerLeft,
    kUpperLeft,
    kLowerRight,
    kUpperRight
};

//-----------------------------------------------------------------------------
void L_SetSurfaceScissor(svrSurfaceSubset whichPart)
//-----------------------------------------------------------------------------
{
    switch (whichPart) {
        case kIgnore:
            // No Operations
            break;
        case kEntire:
            GL(glScissor(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                         gAppContext->modeContext->warpRenderSurfaceHeight));
            // LOGI("Scissor: (%d, %d, %d, %d)", 0, 0, gAppContext->modeContext->warpRenderSurfaceWidth, gAppContext->modeContext->warpRenderSurfaceHeight);
            break;
        case kLeftHalf:
            GL(glScissor(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth / 2,
                         gAppContext->modeContext->warpRenderSurfaceHeight));
            // LOGI("Scissor: (%d, %d, %d, %d)", 0, 0, gAppContext->modeContext->warpRenderSurfaceWidth / 2, gAppContext->modeContext->warpRenderSurfaceHeight);
            break;
        case kRightHalf:
            GL(glScissor(gAppContext->modeContext->warpRenderSurfaceWidth / 2, 0,
                         gAppContext->modeContext->warpRenderSurfaceWidth / 2,
                         gAppContext->modeContext->warpRenderSurfaceHeight));
            // LOGI("Scissor: (%d, %d, %d, %d)", gAppContext->modeContext->warpRenderSurfaceWidth / 2, 0, gAppContext->modeContext->warpRenderSurfaceWidth / 2, gAppContext->modeContext->warpRenderSurfaceHeight);
            break;
        case kTopHalf:
            GL(glScissor(0, gAppContext->modeContext->warpRenderSurfaceHeight / 2,
                         gAppContext->modeContext->warpRenderSurfaceWidth,
                         gAppContext->modeContext->warpRenderSurfaceHeight / 2));
            // LOGI("Scissor: (%d, %d, %d, %d)", 0, gAppContext->modeContext->warpRenderSurfaceHeight / 2, gAppContext->modeContext->warpRenderSurfaceWidth, gAppContext->modeContext->warpRenderSurfaceHeight / 2);
            break;
        case KBottomHalf:
            GL(glScissor(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                         gAppContext->modeContext->warpRenderSurfaceHeight / 2));
            // LOGI("Scissor: (%d, %d, %d, %d)", 0, 0, gAppContext->modeContext->warpRenderSurfaceWidth, gAppContext->modeContext->warpRenderSurfaceHeight / 2);
            break;
        case kLowerLeft:
            // LOGI("Scissor: Undefined!");
            break;
        case kUpperLeft:
            // LOGI("Scissor: Undefined!");
            break;
        case kLowerRight:
            // LOGI("Scissor: Undefined!");
            break;
        case kUpperRight:
            // LOGI("Scissor: Undefined!");
            break;
    }
}

//-----------------------------------------------------------------------------
void L_StartTiledArea(svrSurfaceSubset whichPart)
//-----------------------------------------------------------------------------
{
    switch (whichPart) {
        case kEntire:
            GL(glStartTilingQCOM(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                                 gAppContext->modeContext->warpRenderSurfaceHeight, 0));
            break;
        case kLeftHalf:
            GL(glStartTilingQCOM(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth / 2,
                                 gAppContext->modeContext->warpRenderSurfaceHeight, 0));
            break;
        case kRightHalf:
            GL(glStartTilingQCOM(gAppContext->modeContext->warpRenderSurfaceWidth / 2, 0,
                                 gAppContext->modeContext->warpRenderSurfaceWidth / 2,
                                 gAppContext->modeContext->warpRenderSurfaceHeight, 0));
            break;
        case kTopHalf:
            GL(glStartTilingQCOM(0, gAppContext->modeContext->warpRenderSurfaceHeight / 2,
                                 gAppContext->modeContext->warpRenderSurfaceWidth,
                                 gAppContext->modeContext->warpRenderSurfaceHeight / 2, 0));
            break;
        case KBottomHalf:
            GL(glStartTilingQCOM(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                                 gAppContext->modeContext->warpRenderSurfaceHeight / 2, 0));
            break;
        default:
            break;
    }
}

//-----------------------------------------------------------------------------
bool L_ContainsSphereMesh(svrFrameParamsInternal *pWarpFrame)
//-----------------------------------------------------------------------------
{
    for (int whichLayer = 0; whichLayer < SVR_MAX_RENDER_LAYERS; whichLayer++) {
        if ((pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectTexture) ||
            (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectImage) ||
            (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeCubemapTexture)) {
            return true;
        }
    }

    return false;
}

struct ShaderParamStruct {
    Svr::SvrShader *pShader;

    glm::vec4 posScaleoffset;         // Location = 1
    glm::mat4 texWarpMatrix;          // Location = 2
    glm::vec4 eyeBorder;              // Location = 3
    glm::vec4 uvScaleoffset;          // Location = 4
    glm::mat4 modelMatrix;            // Location = 6
    glm::vec4 vignetteParams;         // Location = 7
    glm::mat4 transformMatrix;        // Location = 8
    glm::vec4 arrayLayer;             // Location = 9
    glm::mat2 skewSquashMatrix;       // Location = 10

#ifdef ENABLE_MOTION_VECTORS
    glm::vec4           spacewarpParams;        // Location = 11
    glm::vec4           spacewarpScaleOffset;   // Location = 12
#endif // ENABLE_MOTION_VECTORS
};

enum UniformOptions {
    kEnableEyeBorder = (1 << 0),
    kEnableTexWarpMatrix = (1 << 1),
    kEnableUvOffset = (1 << 2),
    kEnableModelMatrix = (1 << 3),
    kEnableVignette = (1 << 4),
    kEnableTransform = (1 << 5),
    kEnableArrayLayer = (1 << 6),

    // Spacewarp is conditionally compiled.  It is not hooked to a flag
};

//-----------------------------------------------------------------------------
void L_SetShaderUniforms(ShaderParamStruct *pParams, unsigned int flags, unsigned int eyeBuffer,
                         unsigned int samplerType, unsigned int samplerObj, svrEyeMask eyeMask)
//-----------------------------------------------------------------------------
{
    if (gLogShaderUniforms)
        LOGI("********** Setting Shader Parameters (Shader Handle = %d) **********",
             pParams->pShader->GetShaderId());

    if (gLogShaderUniforms)
        LOGI("Setting posScaleOffset: (%0.2f, %0.2f, %0.2f, %0.2f)", pParams->posScaleoffset.x,
             pParams->posScaleoffset.y, pParams->posScaleoffset.z, pParams->posScaleoffset.w);
    pParams->pShader->SetUniformVec4(1, pParams->posScaleoffset);

    if (gLogShaderUniforms)
        LOGI("Setting skewSquashMatrix: (%0.2f, %0.2f, %0.2f, %0.2f)",
             pParams->skewSquashMatrix[0][0], pParams->skewSquashMatrix[0][1],
             pParams->skewSquashMatrix[1][0], pParams->skewSquashMatrix[1][1]);
    pParams->pShader->SetUniformMat2(10, pParams->skewSquashMatrix);

    if (flags & kEnableTexWarpMatrix) {
        if (gLogShaderUniforms) {
            glm::mat4 L = pParams->texWarpMatrix;
            LOGI("Setting texWarpMatrix ");
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][0], L[1][0], L[2][0], L[3][0]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][1], L[1][1], L[2][1], L[3][1]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][2], L[1][2], L[2][2], L[3][2]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][3], L[1][3], L[2][3], L[3][3]);
        }
        pParams->pShader->SetUniformMat4(2, pParams->texWarpMatrix);
    }

    if (flags & kEnableEyeBorder) {
        if (gLogShaderUniforms)
            LOGI("Setting eyeBorder: (%0.2f, %0.2f, %0.2f, %0.2f)", pParams->eyeBorder.x,
                 pParams->eyeBorder.y, pParams->eyeBorder.z, pParams->eyeBorder.w);
        pParams->pShader->SetUniformVec4(3, pParams->eyeBorder);
    }

    if (flags & kEnableUvOffset) {
        if (gLogShaderUniforms)
            LOGI("Setting uvScaleOffset: (%0.2f, %0.2f, %0.2f, %0.2f)", pParams->uvScaleoffset.x,
                 pParams->uvScaleoffset.y, pParams->uvScaleoffset.z, pParams->uvScaleoffset.w);
        pParams->pShader->SetUniformVec4(4, pParams->uvScaleoffset);
    }

    if (flags & kEnableModelMatrix) {
        if (gLogShaderUniforms) {
            glm::mat4 L = pParams->modelMatrix;
            LOGI("Setting modelMatrix ");
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][0], L[1][0], L[2][0], L[3][0]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][1], L[1][1], L[2][1], L[3][1]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][2], L[1][2], L[2][2], L[3][2]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][3], L[1][3], L[2][3], L[3][3]);
        }
        pParams->pShader->SetUniformMat4(6, pParams->modelMatrix);
    }

    if (flags & kEnableVignette) {
        if (gLogShaderUniforms)
            LOGI("Setting vignetteParams: (%0.2f, %0.2f, %0.2f, %0.2f)", pParams->vignetteParams.x,
                 pParams->vignetteParams.y, pParams->vignetteParams.z, pParams->vignetteParams.w);
        pParams->pShader->SetUniformVec4(7, pParams->vignetteParams);
    }

    if (flags & kEnableTransform) {
        if (gLogShaderUniforms) {
            glm::mat4 L = pParams->transformMatrix;
            LOGI("Setting transformMatrix ");
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][0], L[1][0], L[2][0], L[3][0]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][1], L[1][1], L[2][1], L[3][1]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][2], L[1][2], L[2][2], L[3][2]);
            LOGI("    %0.2f, %0.2f, %0.2f, %0.2f", L[0][3], L[1][3], L[2][3], L[3][3]);
        }
        pParams->pShader->SetUniformMat4(8, pParams->transformMatrix);
    }

    if (flags & kEnableArrayLayer) {
        if (gLogShaderUniforms)
            LOGI("Setting arrayLayer: (%0.2f, %0.2f, %0.2f, %0.2f)", pParams->arrayLayer.x,
                 pParams->arrayLayer.y, pParams->arrayLayer.z, pParams->arrayLayer.w);
        pParams->pShader->SetUniformVec4(9, pParams->arrayLayer);
    }

    if (eyeBuffer != 0) {
        if (gLogShaderUniforms) {
            switch (samplerType) {
                case GL_TEXTURE_2D:
                    LOGI("Setting srcTex: %d (GL_TEXTURE_2D)", eyeBuffer);
                    break;
                case GL_TEXTURE_2D_ARRAY:
                    LOGI("Setting srcTex: %d (GL_TEXTURE_2D_ARRAY)", eyeBuffer);
                    break;
                case GL_TEXTURE_CUBE_MAP:
                    LOGI("Setting srcTex: %d (GL_TEXTURE_CUBE_MAP)", eyeBuffer);
                    break;
                case GL_TEXTURE_EXTERNAL_OES:
                    LOGI("Setting srcTex: %d (GL_TEXTURE_EXTERNAL_OES)", eyeBuffer);
                    break;
                default:
                    LOGI("Setting srcTex: %d (0x%x)", eyeBuffer, samplerType);
                    break;
            }
        }
        pParams->pShader->SetUniformSampler("srcTex", eyeBuffer, samplerType, samplerObj);
    }

#ifdef ENABLE_MOTION_VECTORS
    if (gEnableMotionVectors && gpMotionData == NULL)
    {
        LOGI("Unable to set motion vector shader parameters: Motion data has not been created!");
        return;
    }

    // Only apply spacewarp texture if rendering an eye buffer, NOT an overlay buffer.
    // This is kind of an ugly way to test this, but we are prototyping :(
    // Later: Yup!  Burned me! Some sampler is NOT gWarpData.eyeSamplerObj and we were never setting right value
    // if (samplerObj != gWarpData.eyeSamplerObj)
    // {
    //     // This logic could actually be prone to error.  We come in here when compositing.
    //     // For this motion vector prototyping compositing is NOT supported to good luck!
    //     return;
    // }

    // Motion vectors are enabled, which eye are we rendering
    uint32_t whichMotionData = 0;
    const char *pLeftRight = "Unknown";
    if (eyeMask & kEyeMaskLeft)
    {
        pLeftRight = "Left";
        whichMotionData = gMostRecentMotionLeft;
    }
    else if (eyeMask & kEyeMaskRight)
    {
        pLeftRight = "Right";
        whichMotionData = gMostRecentMotionRight;
    }

    if (gEnableMotionVectors)
    {
        // The UV scaling between eye texture and spacewarp texture
        glm::vec4 spacewarpScaleOffset = pParams->spacewarpScaleOffset;
        if (gLogShaderUniforms) LOGI("Setting spacewarpScaleOffset: (%0.2f, %0.2f, %0.2f, %0.2f)", spacewarpScaleOffset.x, spacewarpScaleOffset.y, spacewarpScaleOffset.z, spacewarpScaleOffset.w);
        pParams->pShader->SetUniformVec4(12, spacewarpScaleOffset);

        // The spacewarp result texture
        if (gFrameDoubled && gUseMotionVectors)
        {
            if (gLogMotionVectors) LOGI("%s: Using motion vector result texture %d (Index = %d)", pLeftRight, gpMotionData[whichMotionData].resultTexture, whichMotionData);
            pParams->pShader->SetUniformSampler("spacewarpTex", gpMotionData[whichMotionData].resultTexture, GL_TEXTURE_2D, gWarpData.eyeSamplerObj);
        }
        else
        {
            if (gLogMotionVectors) LOGI("%s: Using motion vector Stub texture", pLeftRight);
            pParams->pShader->SetUniformSampler("spacewarpTex", gStubResultTexture, GL_TEXTURE_2D, gWarpData.eyeSamplerObj);
        }
    }
    if (gEnableMotionVectors && gSmoothMotionVectors && gSmoothMotionVectorsWithGPU)
    {
        SvrMotionData *pData = &gpMotionData[whichMotionData];
        // X: Spacewarp Texture Width
        // Y: Spacewarp Texture Height
        // Z: Spacewarp Texture One Pixel Width
        // W: Spacewarp Texture One Pixel Height
        glm::vec4 spacewarpParams = glm::vec4(pData->meshWidth, pData->meshHeight, 1.0f / pData->meshWidth, 1.0f / pData->meshHeight);
        if (gLogShaderUniforms) LOGI("Setting spacewarpParams: (%0.2f, %0.2f, %0.2f, %0.2f)", spacewarpParams.x, spacewarpParams.y, spacewarpParams.z, spacewarpParams.w);
        pParams->pShader->SetUniformVec4(11, spacewarpParams);
    }
#endif // ENABLE_MOTION_VECTORS
}

void L_SelectOverrideMesh(unsigned int &mesh1A, unsigned int &mesh1B, unsigned int &mesh2A,
                          unsigned int &mesh2B) {
    // Loop through the 4 meshes that could be overridden
    for (int whichMesh = 0; whichMesh < 4; whichMesh++) {
        unsigned int *pOneMesh = &mesh1A;
        switch (whichMesh) {
            case 0:
                pOneMesh = &mesh1A;
                break;
            case 1:
                pOneMesh = &mesh1B;
                break;
            case 2:
                pOneMesh = &mesh2A;
                break;
            case 3:
                pOneMesh = &mesh2B;
                break;
        }

        // Only support override on these meshes
        switch (*pOneMesh) {
            case kMeshLeft:
                if (gWarpData.warpMeshes[kMeshLeft_Override].GetVaoId() != 0) {
                    *pOneMesh = kMeshLeft_Override;
                }
                break;

            case kMeshRight:
                if (gWarpData.warpMeshes[kMeshRight_Override].GetVaoId() != 0) {
                    *pOneMesh = kMeshRight_Override;
                }
                break;

            case kMeshUL:
                if (gWarpData.warpMeshes[kMeshUL_Override].GetVaoId() != 0) {
                    *pOneMesh = kMeshUL_Override;
                }
                break;

            case kMeshUR:
                if (gWarpData.warpMeshes[kMeshUR_Override].GetVaoId() != 0) {
                    *pOneMesh = kMeshUR_Override;
                }
                break;

            case kMeshLL:
                if (gWarpData.warpMeshes[kMeshLL_Override].GetVaoId() != 0) {
                    *pOneMesh = kMeshLL_Override;
                }
                break;

            case kMeshLR:
                if (gWarpData.warpMeshes[kMeshLR_Override].GetVaoId() != 0) {
                    *pOneMesh = kMeshLR_Override;
                }
                break;

            default:
                // Was not one that can be overridden
                continue;
        }
    }
}

//-----------------------------------------------------------------------------
svrFrameParamsInternal *L_GetNextFrameToWarp(uint64_t warpVsyncCount, EGLSyncKHR eglSyncList[])
//-----------------------------------------------------------------------------
{
    unsigned int curSubmitFrameCount = gAppContext->modeContext->submitFrameCount;

    svrFrameParamsInternal *pCheckFrame = NULL;
    svrFrameParamsInternal *pReturnFrame = NULL;
    for (int i = 0; i < NUM_SWAP_FRAMES && pReturnFrame == NULL; i++) {
        // Try the current frame and if it not ready, keep going back in time until we find one that is ready.
        int checkFrameCount = curSubmitFrameCount - i;

        if (checkFrameCount <= 0) {
            //This is the first time through and no frames have been submitted yet so bail out
            return NULL;
        }

        pCheckFrame = &gAppContext->modeContext->frameParams[checkFrameCount % NUM_SWAP_FRAMES];

        if (pCheckFrame->minVSyncCount > warpVsyncCount) {
            continue;
        }

        // Check to see if the frame has already finished on the GPU
        // Determine it is Vulkan by looking at the first eyebuffer.
        if (pCheckFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore != 0) {
            // This is a Vulkan frame
            // Need a GL handle to the semaphore, which comes to us as a fence

            if (pCheckFrame->externalSync == 0) {
                if (eglSyncList[checkFrameCount % NUM_SWAP_FRAMES] != EGL_NO_SYNC_KHR) {
                    eglDestroySyncKHR(gVkConsumer_eglDisplay,
                                      eglSyncList[checkFrameCount % NUM_SWAP_FRAMES]);
                    eglSyncList[checkFrameCount % NUM_SWAP_FRAMES] = EGL_NO_SYNC_KHR;
                }

                // We don't have the handle, get it before we can wait on it
                EGLint vSyncAttribs[] =
                        {
                                EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
                                (EGLint) pCheckFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore,
                                EGL_NONE,
                        };

                // LOGI("Getting sync handle from external fd (%d)...", (int)pCheckFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore);

                EGLSyncKHR externalSync = eglCreateSyncKHR(gVkConsumer_eglDisplay,
                                                           EGL_SYNC_NATIVE_FENCE_ANDROID,
                                                           vSyncAttribs);
                if (externalSync == EGL_NO_SYNC_KHR) {
                    LOGE("Unable to create sync fence from external fd!");
                } else {
                    // LOGI("    Sync handle from external fd %d => 0x%x", (int)pCheckFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore, (int)pCheckFrame->externalSync);
                }

                pCheckFrame->externalSync = externalSync;
                eglSyncList[checkFrameCount % NUM_SWAP_FRAMES] = externalSync;
            }

            // Check to see if it is done now
#if VULKAN_USE_CLIENT_WAIT_SYNC
            EGLint flags = 0;
            EGLTimeKHR timeout = 0ull; // EGL_FOREVER_KHR
            EGLint eglResult = EGL(eglClientWaitSyncKHR(gVkConsumer_eglDisplay, pCheckFrame->externalSync, flags, timeout));
            if (eglResult == EGL_TIMEOUT_EXPIRED_KHR)
            {
                // This frame has not finished
                //LOGI("TIMEWARP: GPU not finished with frame %d", checkFrameCount);
                continue;
            }

            if (eglResult != EGL_CONDITION_SATISFIED_KHR)
            {
                LOGE("TIMEWARP: Error waiting for external sync: %d", eglResult);
                continue;
            }
#else
            // We can only call eglClientWaitSync on the sync object imported from the Vk semaphore once. That seems
            // to be the case even with the special timeout value of 0, which should only check the status. So instead
            // let's use eglGetSyncAttribKHR to get the status.
            EGLint syncStatus = EGL_UNSIGNALED_KHR;
            EGLBoolean eglResult = EGL(eglGetSyncAttribKHR(gVkConsumer_eglDisplay,
                                                           (EGLSyncKHR) pCheckFrame->externalSync,
                                                           EGL_SYNC_STATUS_KHR, &syncStatus));
            if (syncStatus == EGL_UNSIGNALED_KHR) {
                // This frame has not finished
                //LOGI("TIMEWARP: GPU not finished with frame %d", checkFrameCount);
                continue;
            }

            if (eglResult != EGL_TRUE) {
                LOGE("TIMEWARP: Error checking status of external sync: %d", eglResult);
                continue;
            }
#endif
        } else {
            // This is a GL Frame
            // LOGI("Waiting for sync: %d...", (int)pCheckFrame->frameSync);
            GLenum syncResult = GL(
                    glClientWaitSync(pCheckFrame->frameSync, GL_SYNC_FLUSH_COMMANDS_BIT, 0));
            switch (syncResult) {
                case GL_ALREADY_SIGNALED:       // indicates that sync was signaled at the time that glClientWaitSync was called.
                    // The fence indicates it is complete.
                    // LOGI("Frame %d returned GL_ALREADY_SIGNALED for glClientWaitSync", checkFrameCount % NUM_SWAP_FRAMES);
                    break;

                case GL_TIMEOUT_EXPIRED:        // indicates that at least timeout nanoseconds passed and sync did not become signaled.
                    //The current frame hasn't finished on the GPU so keep looking back for a frame that has
                    //LOGI("GPU not finished with frame %d", checkFrameCount);
                    LOGV("Frame %d is NOT finished on the GPU", checkFrameCount % NUM_SWAP_FRAMES);
                    continue;

                case GL_CONDITION_SATISFIED:    // indicates that sync was signaled before the timeout expired.
                    // Technically this can't happen as long as we pass in 0 to glClientWaitSync. This indicates it was signaled before timeout
                    LOGE("Frame %d returned GL_CONDITION_SATISFIED for glClientWaitSync",
                         checkFrameCount % NUM_SWAP_FRAMES);
                    break;

                case GL_WAIT_FAILED:            // indicates that an error occurred. Additionally, an OpenGL error will be generated.
                    LOGE("Frame %d returned GL_WAIT_FAILED for glClientWaitSync",
                         checkFrameCount % NUM_SWAP_FRAMES);
                    continue;
            }
        }

        // We found a frame to warp
        // LOGI("DEBUG!    Warping frame %d", checkFrameCount % NUM_SWAP_FRAMES);
        pReturnFrame = pCheckFrame;
        gAppContext->modeContext->warpFrameCount = checkFrameCount;

        // Was seeing the submit frame count higher than the warp frame but no "No Finished on GPU" message.
        // Later: This is valid.  It just means that app has submitted a frame while TimeWarp is trying to find one to warp
        // if (checkFrameCount != gAppContext->modeContext->submitFrameCount)
        //     LOGI("THREAD COLLISION? Warping frame %d, latest submitted is %d", checkFrameCount % NUM_SWAP_FRAMES, gAppContext->modeContext->submitFrameCount % NUM_SWAP_FRAMES);

        return pReturnFrame;

    }   // Looking through possible frames

    // LOGI("Could not find a valid frame to warp!");
    return NULL;
}

//-----------------------------------------------------------------------------
bool L_IsUvIdentity(svrRenderLayer &renderLayer)
//-----------------------------------------------------------------------------
{
    if (renderLayer.imageCoords.LowerUVs[0] != 0.0f ||
        renderLayer.imageCoords.LowerUVs[1] != 0.0f ||
        renderLayer.imageCoords.LowerUVs[2] != 1.0f ||
        renderLayer.imageCoords.LowerUVs[3] != 0.0f ||
        renderLayer.imageCoords.UpperUVs[0] != 0.0f ||
        renderLayer.imageCoords.UpperUVs[1] != 1.0f ||
        renderLayer.imageCoords.UpperUVs[2] != 1.0f ||
        renderLayer.imageCoords.UpperUVs[3] != 1.0f) {
        return false;
    }

    // Since all conditions are met, it must be identity
    return true;
}

//-----------------------------------------------------------------------------
bool
L_NeedUvScaleOffset(svrRenderLayer &renderLayer, glm::vec4 &uvScaleoffset, glm::vec4 &eyeBorder)
//-----------------------------------------------------------------------------
{
    // If this is full screen then don't need scale and offset
    if (L_IsUvIdentity(renderLayer)) {
        return false;
    }

    // Scale is the difference (Assume right angles at all corners...
    uvScaleoffset.x = renderLayer.imageCoords.LowerUVs[2] - renderLayer.imageCoords.LowerUVs[0];
    uvScaleoffset.y = renderLayer.imageCoords.UpperUVs[1] - renderLayer.imageCoords.LowerUVs[1];

    // ...offset is the desired lowest value...
    uvScaleoffset.z = renderLayer.imageCoords.LowerUVs[0];
    uvScaleoffset.w = renderLayer.imageCoords.LowerUVs[1];

    // LOGI("Setting uvScaleOffset: (%0.2f, %0.2f, %0.2f, %0.2f)", uvScaleoffset.x, uvScaleoffset.y, uvScaleoffset.z, uvScaleoffset.w);

    // ... and border is the min/max
    eyeBorder.x = MIN(renderLayer.imageCoords.LowerUVs[0], renderLayer.imageCoords.LowerUVs[2]);
    eyeBorder.y = MAX(renderLayer.imageCoords.LowerUVs[0], renderLayer.imageCoords.LowerUVs[2]);
    eyeBorder.z = MIN(renderLayer.imageCoords.LowerUVs[1], renderLayer.imageCoords.UpperUVs[1]);
    eyeBorder.w = MAX(renderLayer.imageCoords.LowerUVs[1], renderLayer.imageCoords.UpperUVs[1]);

    // LOGI("            eyeBorder: (%0.2f, %0.2f, %0.2f, %0.2f)", eyeBorder.x, eyeBorder.y, eyeBorder.z, eyeBorder.w);

    return true;
}


//-----------------------------------------------------------------------------
Svr::SvrShader *
L_GetLayerShader(svrFrameParamsInternal *pWarpFrame, uint32_t whichLayer, svrEyeMask eyeMask,
                 unsigned int &paramFlag, ShaderParamStruct &shaderParams,
                 unsigned int &samplerType)
//-----------------------------------------------------------------------------
{
    // Using gWarpData (SvrAsyncWarpResources)
    SvrAsyncWarpResources *pWarpData = &gWarpData;

    bool bTimeWarpEnabled = !((pWarpFrame->frameParams.frameOptions & kDisableReprojection) > 0);
    bool bChromaEnabled = !((pWarpFrame->frameParams.frameOptions & kDisableChromaticCorrection) >
                            0);

    if (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags & kLayerFlagHeadLocked) {
        bTimeWarpEnabled = false;
    }

    // Set up the shader mask, uniform parameter mask, and uniform values.
    uint32_t shaderMask = 0;
    paramFlag = 0;

    // kWarpProjected      = (1 << 0),
    if (bTimeWarpEnabled) {
        // This layer is projected
        shaderMask |= kWarpProjected;
        paramFlag |= kEnableTexWarpMatrix;
        // shaderParams.texWarpMatrix has already been set before coming in here
    }

    // kWarpImage          = (1 << 1),
    samplerType = GL_TEXTURE_2D;
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage ||
        pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectImage) {
        shaderMask |= kWarpImage;
        samplerType = GL_TEXTURE_EXTERNAL_OES;
    }

    // kWarpUvScaleOffset  = (1 << 2), 
    // kWarpUvClamp        = (1 << 3),
    shaderParams.uvScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    if (L_NeedUvScaleOffset(pWarpFrame->frameParams.renderLayers[whichLayer],
                            shaderParams.uvScaleoffset, shaderParams.eyeBorder)) {
        shaderMask |= kWarpUvScaleOffset;
        shaderMask |= kWarpUvClamp;

        paramFlag |= kEnableUvOffset;
        paramFlag |= kEnableEyeBorder;
    }

    // kWarpChroma         = (1 << 4),
    if (bChromaEnabled) {
        shaderMask |= kWarpChroma;
    }

    // kWarpArray          = (1 << 5),
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeTextureArray) {
        shaderMask |= kWarpArray;
        samplerType = GL_TEXTURE_2D_ARRAY;

        paramFlag |= kEnableArrayLayer;

        if (eyeMask & kEyeMaskLeft)
            shaderParams.arrayLayer = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        else if (eyeMask & kEyeMaskRight)
            shaderParams.arrayLayer = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // kWarpVignette       = (1 << 6),
    if (gMeshVignette) {
        shaderMask |= kWarpVignette;
        paramFlag |= kEnableVignette;
        shaderParams.vignetteParams = glm::vec4(gVignetteRadius, gLensPolyUnitRadius,
                                                gLensPolyUnitRadius - gVignetteRadius, 0.0f);
    }

    // kWarpEquirect = (1 << 7),
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectTexture ||
        pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeEquiRectImage) {
        shaderMask |= kWarpEquirect;

        // Need the model matrix...
        paramFlag |= kEnableModelMatrix;

        // View matrix is current pose
        svrHeadPoseState poseState = svrGetPredictedHeadPose(0.0f);
        glm::fquat poseQuat = glm::fquat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                         poseState.pose.rotation.y, poseState.pose.rotation.z);
        glm::mat4 viewMat = glm::mat4_cast(poseQuat);
        glm::mat4 modelMat = inverse(
                viewMat);  // Need the inverse of the pose matrix to be used as a model matrix
        shaderParams.modelMatrix = modelMat;

        // ...transform matrix...
        paramFlag |= kEnableTransform;
        glm::mat4 transformMat = glm::make_mat4(
                pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.TransformMatrix);
        shaderParams.transformMatrix = transformMat;

        // ... and no projection
        shaderMask &= ~kWarpProjected;
        paramFlag &= ~kEnableTexWarpMatrix;
    }

    // kWarpCubemap = (1 << 8),
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeCubemapTexture) {
        shaderMask |= kWarpCubemap;
        samplerType = GL_TEXTURE_CUBE_MAP;

        // Need the model matrix...
        paramFlag |= kEnableModelMatrix;

        // View matrix is current pose
        svrHeadPoseState poseState = svrGetPredictedHeadPose(0.0f);
        glm::fquat poseQuat = glm::fquat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                         poseState.pose.rotation.y, poseState.pose.rotation.z);
        glm::mat4 viewMat = glm::mat4_cast(poseQuat);
        glm::mat4 modelMat = inverse(
                viewMat);  // Need the inverse of the pose matrix to be used as a model matrix
        shaderParams.modelMatrix = modelMat;

        // ...transform matrix...
        paramFlag |= kEnableTransform;
        glm::mat4 transformMat = glm::make_mat4(
                pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.TransformMatrix);
        shaderParams.transformMatrix = transformMat;

        // ... and no projection
        shaderMask &= ~kWarpProjected;
        paramFlag &= ~kEnableTexWarpMatrix;
    }

    if (gDisableLensCorrection) {
        shaderMask |= kWarpARDevice;
    }

#ifdef ENABLE_MOTION_VECTORS
    // Special handling if rendering the motion engine input
    if (gRenderMotionVectors && gRenderMotionInput && gpMotionData != NULL)
    {
        // This is a simple render of an image texture
        shaderMask = 0;
        paramFlag = 0;

        // shaderMask |= kWarpImage;
        // samplerType = GL_TEXTURE_EXTERNAL_OES;
        samplerType = GL_TEXTURE_2D;
        // gFrameDoubled = false;
    }

    // Special handling if rendering the motion engine output
    if (gRenderMotionVectors && gpMotionData != NULL)
    {
        bool replacedEyeBuffer = false;
        if (eyeMask & kEyeMaskLeft)
        {
            replacedEyeBuffer = true;
        }

        if (gGenerateBothEyes && (eyeMask & kEyeMaskRight))
        {
            replacedEyeBuffer = true;
        }

        if (replacedEyeBuffer)
        {
            // This is a simple render of an image texture
            shaderMask = 0;
            paramFlag = 0;

            // shaderMask |= kWarpImage;
            // samplerType = GL_TEXTURE_EXTERNAL_OES;
            samplerType = GL_TEXTURE_2D;
            // gFrameDoubled = false;
        }
    }

#endif // ENABLE_MOTION_VECTORS

    // Now find the shader that matches the mask we are looking for
    for (uint32_t whichShader = 0; whichShader < pWarpData->numWarpShaders; whichShader++) {
        if (pWarpData->pWarpShaderMap[whichShader] == shaderMask) {
            // Found the shader we want. If it has not been compiled yet, compile it now
            if (pWarpData->pWarpShaders[whichShader].GetShaderId() == 0) {
                if (!L_CompileOneWarpShader(whichShader))
                    return NULL;
            }

            // LOGE("DEBUG! Using shader %d, (Mask = 0x%x) for this layer...", whichShader, shaderMask);
            return &pWarpData->pWarpShaders[whichShader];
        }
    }

    // No shader that matches what we want
    LOGE("Could not find shader 0x%08x", shaderMask);

    return NULL;
}

//-----------------------------------------------------------------------------
bool L_NeedCustomMesh(svrRenderLayer &renderLayer)
//-----------------------------------------------------------------------------
{
    if (renderLayer.imageCoords.LowerLeftPos[0] != -1.0f ||
        renderLayer.imageCoords.LowerLeftPos[1] != -1.0f ||
        renderLayer.imageCoords.LowerRightPos[0] != 1.0f ||
        renderLayer.imageCoords.LowerRightPos[1] != -1.0f ||
        renderLayer.imageCoords.UpperLeftPos[0] != -1.0f ||
        renderLayer.imageCoords.UpperLeftPos[1] != 1.0f ||
        renderLayer.imageCoords.UpperRightPos[0] != 1.0f ||
        renderLayer.imageCoords.UpperRightPos[1] != 1.0f) {
        return true;
    }

    // Since all conditions are met, it must be identity
    return false;
}

//-----------------------------------------------------------------------------
void L_BlitOneLayer(svrFrameParamsInternal *pWarpFrame, svrEyeMask eyeMask, uint32_t whichLayer)
//-----------------------------------------------------------------------------
{
#ifdef REMOVE_THIS_FUNCTION
    // Using gWarpData (SvrAsyncWarpResources)
    SvrAsyncWarpResources* pWarpData = &gWarpData;

    // If nothing on this layer then go to the next one (Could break instead :) )
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle == 0)
    {
        // This could be considered an error.  Why call in here without an image?
        return;
    }

    // Scale offset should handle the lens parameter offset
    glm::vec4 posScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

    // Scale so that full screen is really one side
    if (eyeMask & kEyeMaskLeft)
        posScaleoffset = glm::vec4(0.5f, 1.0f, -0.5f + gMeshOffsetLeftX, gMeshOffsetLeftY);
    else if (eyeMask & kEyeMaskRight)
        posScaleoffset = glm::vec4(0.5f, 1.0f, 0.5f + gMeshOffsetRightX, gMeshOffsetRightY);

#ifdef RENDER_ACTUAL_MESH
    // 4 points.  Position needs 4 floats per point.  UV needs 2 floats per point
    float quadPos[16];
    float quadUVs[8];

    unsigned int quadIndices[6] = { 0, 2, 1, 1, 2, 3 };
    int indexCount = sizeof(quadIndices) / sizeof(unsigned int);

    // Update the attribute stream for this quad
    quadPos[0] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[0];
    quadPos[1] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[1];
    quadPos[2] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[2];
    quadPos[3] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerLeftPos[3];
    quadUVs[0] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[0];
    quadUVs[1] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[1];

    quadPos[4] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[0];
    quadPos[5] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[1];
    quadPos[6] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[2];
    quadPos[7] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperLeftPos[3];
    quadUVs[2] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[0];
    quadUVs[3] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[1];

    quadPos[8] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[0];
    quadPos[9] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[1];
    quadPos[10] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[2];
    quadPos[11] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerRightPos[3];
    quadUVs[4] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[2];
    quadUVs[5] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[3];

    quadPos[12] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[0];
    quadPos[13] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[1];
    quadPos[14] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[2];
    quadPos[15] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperRightPos[3];
    quadUVs[6] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[2];
    quadUVs[7] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[3];

    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_DEPTH_TEST));

    GL(glBindVertexArray(0));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
#endif // RENDER_ACTUAL_MESH

    // Bind the blit shader...
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
    {
        pWarpData->blitImageShader.Bind();
    }
    else
    {
        pWarpData->blitShader.Bind();
    }

    // ... set attributes ...
#ifdef RENDER_ACTUAL_MESH
    GL(glEnableVertexAttribArray(kPosition));
    GL(glVertexAttribPointer(kPosition, 4, GL_FLOAT, 0, 0, quadPos));

    GL(glEnableVertexAttribArray(kTexcoord0));
    GL(glVertexAttribPointer(kTexcoord0, 2, GL_FLOAT, 0, 0, quadUVs));
#endif // RENDER_ACTUAL_MESH

    // ... set uniforms ...
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
    {
        pWarpData->blitImageShader.SetUniformVec4(1, posScaleoffset);
    }
    else
    {
        pWarpData->blitShader.SetUniformVec4(1, posScaleoffset);
    }

    // ... set textures ...
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
    {
        pWarpData->blitShader.SetUniformSampler("srcTex", pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle, GL_TEXTURE_EXTERNAL_OES, 0);
    }
    else
    {
        int imageHandle = 0;

        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeVulkan)
        {
            imageHandle = GetVulkanInteropHandle(&pWarpFrame->frameParams.renderLayers[whichLayer]);
        }
        else
        {
            imageHandle = pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle;
        }

        pWarpData->blitShader.SetUniformSampler("srcTex", imageHandle, GL_TEXTURE_2D, 0);
    }

#ifdef RENDER_ACTUAL_MESH
    // ... render the quad ...
    GL(glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, quadIndices));

    // ... and disable attributes
    GL(glDisableVertexAttribArray(kPosition));
    GL(glDisableVertexAttribArray(kTexcoord0));
#endif // RENDER_ACTUAL_MESH

    // Update the overlay mesh and render it
    svrLayoutCoords *pImageCoords = &pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords;

    svrMeshLayout meshLayout;
    meshLayout.LL_Pos = glm::vec4(pImageCoords->LowerLeftPos[0], pImageCoords->LowerLeftPos[1], pImageCoords->LowerLeftPos[2], pImageCoords->LowerLeftPos[3]);
    meshLayout.LR_Pos = glm::vec4(pImageCoords->LowerRightPos[0], pImageCoords->LowerRightPos[1], pImageCoords->LowerRightPos[2], pImageCoords->LowerRightPos[3]);
    meshLayout.UL_Pos = glm::vec4(pImageCoords->UpperLeftPos[0], pImageCoords->UpperLeftPos[1], pImageCoords->UpperLeftPos[2], pImageCoords->UpperLeftPos[3]);
    meshLayout.UR_Pos = glm::vec4(pImageCoords->UpperRightPos[0], pImageCoords->UpperRightPos[1], pImageCoords->UpperRightPos[2], pImageCoords->UpperRightPos[3]);
    meshLayout.LL_UV = glm::vec2(pImageCoords->LowerUVs[0], pImageCoords->LowerUVs[1]);
    meshLayout.LR_UV = glm::vec2(pImageCoords->LowerUVs[2], pImageCoords->LowerUVs[3]);
    meshLayout.UL_UV = glm::vec2(pImageCoords->UpperUVs[0], pImageCoords->UpperUVs[1]);
    meshLayout.UR_UV = glm::vec2(pImageCoords->UpperUVs[2], pImageCoords->UpperUVs[3]);

    // Since shader is handling the position scale/offset, don't also add it to the mesh
    posScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    UpdateOverlayGeometry(meshLayout, posScaleoffset);
    gWarpData.warpMeshes[kMeshOverlay].Submit(&gMeshAttribs[0], NUM_VERT_ATTRIBS);

    // Unbind the blit shader...
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
    {
        pWarpData->blitImageShader.Unbind();
    }
    else
    {
        pWarpData->blitShader.Unbind();
    }

    // We turned on blending, so turn it off
    GL(glDisable(GL_BLEND));
#endif // REMOVE_THIS_FUNCTION
}

//-----------------------------------------------------------------------------
void L_RenderOneEye(svrFrameParamsInternal *pWarpFrame, svrEyeMask eyeMask,
                    svrSurfaceSubset whichSurface, uint32_t whichMesh, uint32_t whichMeshSphere,
                    glm::mat4 &warpMatrix, glm::mat2 &skewSquashMatrix)
//-----------------------------------------------------------------------------
{
    // Verify Parameters
    if (eyeMask == kEyeMaskBoth) {
        LOGE("Invalid eye mask passed to function. Defaulting to left eye.");
        eyeMask = kEyeMaskLeft;
    }

    if ((gTimeWarpEnabledEyeMask & eyeMask) == 0) {
        // Config file says not supposed to render this eye

        // Don't want to spam, so only send the message every so often.
        static uint64_t lastEyeMaskEventTime = 0;
        uint64_t timeNow = Svr::GetTimeNano();
        if (((timeNow - lastEyeMaskEventTime) * NANOSECONDS_TO_MILLISECONDS) > 2000.0f) {
            LOGI("gTimeWarpEnabledEyeMask preventing at least one eye from rendering...");
            lastEyeMaskEventTime = timeNow;
        }

        return;
    }

    // May need a border
    float onePixelWidth = 0.0f;
    if (gAppContext->deviceInfo.targetEyeWidthPixels != 0)
        onePixelWidth = 1.0f / (float) gAppContext->deviceInfo.targetEyeWidthPixels;

    float onePixelHeight = 0.0f;
    if (gAppContext->deviceInfo.targetEyeHeightPixels != 0)
        onePixelHeight = 1.0f / (float) gAppContext->deviceInfo.targetEyeHeightPixels;

    // The scissor can be set at the beginning of the loop
    L_SetSurfaceScissor(whichSurface);

    if (gLogEyeRender) LOGI("Rendering Eye %d...", eyeMask);

    GL(glDisable(GL_CULL_FACE));

    GL(glDisable(GL_DEPTH_TEST));
    GL(glDepthMask(GL_FALSE));
    GL(glEnable(GL_SCISSOR_TEST));

    // Need to loop through and do the warped layers, then the head locked layers
    for (uint32_t headLockLoop = 0; headLockLoop < 2; headLockLoop++) {
        if (gLogEyeRender) LOGI("Headlock Layer: %d", headLockLoop);
        // Loop through the render layers and render this eye
        ShaderParamStruct shaderParams;
        unsigned int samplerType = GL_TEXTURE_2D;
        Svr::SvrShader *pCurrentShader = NULL;
        bool isComposite = false;
        uint32_t whichCompositeEye = kLeftEye;
        for (uint32_t whichLayer = 0; whichLayer < SVR_MAX_RENDER_LAYERS; whichLayer++) {
            if (isComposite) {
                if (gLogEyeRender) LOGI("    Handled composite layer so done rendering");
                break;
            }

            if (pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle == 0) {
                if (gLogEyeRender) LOGI("    Layer %d is empty so done rendering!", whichLayer);
                break;
            }

            if (!(eyeMask & pWarpFrame->frameParams.renderLayers[whichLayer].eyeMask)) {
                // This layer is NOT for this eye
                if (gLogEyeRender)
                    LOGI("    Rendering Layer %d is NOT for this eye...", whichLayer);
                continue;
            }

            // If we are warped, do those on first loop.  Do head locked on second loop
            if (headLockLoop == 0 && (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags &
                                      kLayerFlagHeadLocked)) {
                // This is warped loop but this is head locked layer
                if (gLogEyeRender)
                    LOGI("    Rendering Layer %d is head locked so skipping this round...",
                         whichLayer);
                continue;
            }

            if (headLockLoop == 1 && !(pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags &
                                       kLayerFlagHeadLocked)) {
                // This is head locked loop but this is warped layer
                if (gLogEyeRender)
                    LOGI("    Rendering Layer %d is warped so skipping this round...", whichLayer);
                continue;
            }

            // If this layer needs to be composited, we composite the rest of them
            isComposite = L_NeedsCompositeBuffer(pWarpFrame, whichLayer);
            if (isComposite) {
                // LOGE("Need Composite Buffer starting at layer %d...", whichLayer);
                // Need to update the composite buffer and use that for the rest of the layers
                whichCompositeEye = kLeftEye;
                if (eyeMask & kEyeMaskRight) {
                    if (gLogEyeRender) LOGI("    Compositing Right eye...");
                    whichCompositeEye = kRightEye;
                } else {
                    if (gLogEyeRender) LOGI("    Compositing Left eye...");
                    whichCompositeEye = kLeftEye;
                }

                if (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags &
                    kLayerFlagHeadLocked) {
                    // Unbind current shader before doing composite step
                    if (pCurrentShader != NULL)
                        pCurrentShader->Unbind();
                    pCurrentShader = NULL;

                    // This is overlay composite
                    if (gLogEyeRender) LOGI("    Compositing as Head-Locked...");
                    L_UpdateCompositeBuffer(gWarpData.overlayTarget[whichCompositeEye], pWarpFrame,
                                            whichLayer, eyeMask,
                                            pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags);

                    // We have updated the composite buffer so fix viewport and scissor
                    GL(glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                                  gAppContext->modeContext->warpRenderSurfaceHeight));
                    L_SetSurfaceScissor(kEntire);
                } else {
                    // Unbind current shader before doing composite step
                    if (pCurrentShader != NULL)
                        pCurrentShader->Unbind();
                    pCurrentShader = NULL;

                    // This is eye layer composite
                    if (gLogEyeRender) LOGI("    Compositing as warped...");
                    L_UpdateCompositeBuffer(gWarpData.eyeTarget[whichCompositeEye], pWarpFrame,
                                            whichLayer, eyeMask,
                                            pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags);

                    // We have updated the composite buffer so fix viewport and scissor
                    GL(glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                                  gAppContext->modeContext->warpRenderSurfaceHeight));
                    L_SetSurfaceScissor(kEntire);
                }
            }   // Is composited

            // Blending is set per layer
            if (!isComposite &&
                (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags & kLayerFlagOpaque)) {
                GL(glDisable(GL_BLEND));
            } else {
                GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                GL(glEnable(GL_BLEND));
            }

            if (gLogEyeRender) LOGI("    Rendering Layer %d...", whichLayer);

            // Reset the shader parameters
            memset(&shaderParams, 0, sizeof(shaderParams));

            if (eyeMask & kEyeMaskLeft)
                shaderParams.posScaleoffset = glm::vec4(1.0f, 1.0f, gMeshOffsetLeftX,
                                                        gMeshOffsetLeftY);
            else if (eyeMask & kEyeMaskRight)
                shaderParams.posScaleoffset = glm::vec4(1.0f, 1.0f, gMeshOffsetRightX,
                                                        gMeshOffsetRightY);
            shaderParams.texWarpMatrix = warpMatrix;
            shaderParams.eyeBorder = glm::vec4(0.0f + onePixelWidth, 0.5f - onePixelHeight, 0.0f,
                                               0.0f);
            shaderParams.uvScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            shaderParams.skewSquashMatrix = skewSquashMatrix;

#ifdef ENABLE_MOTION_VECTORS
            if (gEnableMotionVectors)
            {
                // Spacewarp scale and offset are a function of texture coordinates
                // Later: The correct coordinates are handled in the shader.  Not scale and offset needed.
                // svrLayoutCoords *pOneLayout = &pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords;
                shaderParams.spacewarpScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

#ifdef SPACEWARP_COORDS_ARE_SCALED
                // if (eyeMask & kEyeMaskLeft)
                //     LOGI("DEBUG! Left Eye Spacewarp UV ScaleOffset:");
                // else if (eyeMask & kEyeMaskRight)
                //     LOGI("DEBUG! Right Eye Spacewarp UV ScaleOffset:");
                // 
                // LOGI("DEBUG!     LL = [%0.2f, %0.2f]; LR = [%0.2f, %0.2f]; UL = [%0.2f, %0.2f]; UR = [%0.2f, %0.2f]",   pOneLayout->LowerUVs[0], pOneLayout->LowerUVs[1],
                //                                                                                                         pOneLayout->LowerUVs[2], pOneLayout->LowerUVs[3],
                //                                                                                                         pOneLayout->UpperUVs[0], pOneLayout->UpperUVs[1],
                //                                                                                                         pOneLayout->UpperUVs[2], pOneLayout->UpperUVs[3]);

                float scaleX = (pOneLayout->LowerUVs[2] - pOneLayout->LowerUVs[0]);
                float scaleY = (pOneLayout->UpperUVs[1] - pOneLayout->LowerUVs[1]);
                // LOGI("DEBUG!     Scale X/Y: (%0.2f, %0.2f)", scaleX, scaleY);

                // The base size is 1. The correction for this inverse of whatever is calculated
                if (scaleX != 0.0f)
                    scaleX = 1.0f / scaleX;
                if (scaleY != 0.0f)
                    scaleY = 1.0f / scaleY;
                // LOGI("DEBUG!     Inverse Scale X/Y: (%0.2f, %0.2f)", scaleX, scaleY);

                // Offset is from the center of the two points
                float offsetX = -scaleX * pOneLayout->LowerUVs[0];
                float offsetY = -scaleY * pOneLayout->LowerUVs[1];
                // LOGI("DEBUG!     Offset X/Y: (%0.2f, %0.2f)", offsetX, offsetY);

                // shaderParams.spacewarpScaleOffset.x = scaleX;
                // shaderParams.spacewarpScaleOffset.y = scaleY;
                // shaderParams.spacewarpScaleOffset.z = offsetX;
                // shaderParams.spacewarpScaleOffset.w = offsetY;
                // LOGI("DEBUG!     Final: (%0.2f, %0.2f, %0.2f, %0.2f)", shaderParams.spacewarpScaleOffset.x, shaderParams.spacewarpScaleOffset.y, shaderParams.spacewarpScaleOffset.z, shaderParams.spacewarpScaleOffset.w);
#endif // SPACEWARP_COORDS_ARE_SCALED
            }   // gEnableMotionVectors
#endif // ENABLE_MOTION_VECTORS


            // Get the shader for this layer and see if we have to switch
            unsigned int paramFlag;
            Svr::SvrShader *pThisShader = NULL;
            if (isComposite) {
                // Get the shader to handle composite buffer
                SvrAsyncWarpResources *pWarpData = &gWarpData;

                bool bTimeWarpEnabled = !(
                        (pWarpFrame->frameParams.frameOptions & kDisableReprojection) > 0);
                bool bChromaEnabled = !(
                        (pWarpFrame->frameParams.frameOptions & kDisableChromaticCorrection) > 0);

                // Do NOT handle multiple layers that are composited and mixed for head locked!
                if (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags &
                    kLayerFlagHeadLocked) {
                    bTimeWarpEnabled = false;
                }

                // Set up the shader mask, uniform parameter mask, and uniform values.
                uint32_t shaderMask = 0;
                paramFlag = 0;

                // kWarpProjected      = (1 << 0),
                if (bTimeWarpEnabled) {
                    shaderMask |= kWarpProjected;
                    paramFlag |= kEnableTexWarpMatrix;
                }

                // Since composite layer, sampler type is always GL_TEXTURE_2D
                samplerType = GL_TEXTURE_2D;

                // Never UV scaled or clamped
                shaderParams.uvScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

                // kWarpChroma         = (1 << 4),
                if (bChromaEnabled) {
                    shaderMask |= kWarpChroma;
                }

                // kWarpVignette       = (1 << 6),
                if (gMeshVignette) {
                    shaderMask |= kWarpVignette;
                    paramFlag |= kEnableVignette;
                    shaderParams.vignetteParams = glm::vec4(gVignetteRadius, gLensPolyUnitRadius,
                                                            gLensPolyUnitRadius - gVignetteRadius,
                                                            0.0f);
                }

                // Now find the shader that matches the mask we are looking for
                for (uint32_t whichShader = 0;
                     whichShader < pWarpData->numWarpShaders; whichShader++) {
                    if (pWarpData->pWarpShaderMap[whichShader] == shaderMask) {
                        // Found the shader we want. If it has not been compiled yet, compile it now
                        if (pWarpData->pWarpShaders[whichShader].GetShaderId() == 0) {
                            // No error checking because fail results in NULL and is checked below
                            L_CompileOneWarpShader(whichShader);
                        }

                        pThisShader = &pWarpData->pWarpShaders[whichShader];
                        // LOGE("DEBUG! Using shader %d, (Mask = 0x%x; Handle = %d) for this composite layer...", whichShader, shaderMask, pThisShader->GetShaderId());
                        break;
                    }
                }
            }   // Is Composite
            else {
                // No composite, so get shader for this layer
                pThisShader = L_GetLayerShader(pWarpFrame, whichLayer, eyeMask, paramFlag,
                                               shaderParams, samplerType);
            }

            if (pThisShader == NULL) {
                LOGE("Could not find a shader to handle render layer %d (Mask = 0x%08x)",
                     whichLayer, 0xFFFFFFFF);
                continue;
            }

            if (pCurrentShader != pThisShader) {
                // Different shader for this layer
                if (pCurrentShader != NULL)
                    pCurrentShader->Unbind();

                pCurrentShader = pThisShader;
                pCurrentShader->Bind();
            }

            // Safety Net
            if (pCurrentShader == NULL) {
                LOGE("Current shader to handle render layer %d is NULL (Mask = 0x%08x)", whichLayer,
                     0xFFFFFFFF);
                continue;
            }

            // Need handle to eye buffer.
            int imageHandle = 0;
            if (isComposite) {
                if (pWarpFrame->frameParams.renderLayers[whichLayer].layerFlags &
                    kLayerFlagHeadLocked) {
                    // This is overlay composite
                    imageHandle = gWarpData.overlayTarget[whichCompositeEye].GetColorAttachment();
                    if (gLogEyeRender) LOGI("    Composite overlay handle: 0x%08x", imageHandle);
                } else {
                    // This is eye layer composite
                    imageHandle = gWarpData.eyeTarget[whichCompositeEye].GetColorAttachment();
                    if (gLogEyeRender) LOGI("    Composite eye handle: 0x%08x", imageHandle);
                }
            } else if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeVulkan) {
                imageHandle = GetVulkanInteropHandle(
                        &pWarpFrame->frameParams.renderLayers[whichLayer]);
            } else {
                imageHandle = pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle;
            }

#ifdef ENABLE_MOTION_VECTORS
            // Handle render of the motion vector results
            if (gRenderMotionVectors && gpMotionData != NULL)
            {
                bool replacedEyeBuffer = false;
                if (gRenderMotionInput)
                {
                    SvrMotionData *pThisData = &gpMotionData[gMostRecentMotionLeft];

                    replacedEyeBuffer = true;

                    // Left and right are current (warped) and previous
                    if (eyeMask & kEyeMaskLeft)
                    {
                        // Ideally we should use pThisData->oldBuffer to figure out which one to show.
                        // However, this value can be changed in thread and we could end up with two
                        // images that don't go together.  This was showing up as double images as the 
                        // device was moved.  Therefore, just used the first set (left eye) for the debug
                        // display. 
                        // imageHandle = pThisData->debugBuffers[pThisData->oldBuffer][kMotionTypeStatic].imageTexture;
                        imageHandle = pThisData->debugBuffers[0][kMotionTypeStatic].imageTexture;
                        // LOGI("DEBUG! Motion Input (Left) = %d", imageHandle);
                    }
                    if (eyeMask & kEyeMaskRight)
                    {
                        // imageHandle = pThisData->debugBuffers[pThisData->newBuffer][kMotionTypeWarped].imageTexture;
                        imageHandle = pThisData->debugBuffers[1][kMotionTypeWarped].imageTexture;
                        // LOGI("DEBUG! Motion Input (Right) = %d", imageHandle);
                    }
                }
                else
                {
                    if (eyeMask & kEyeMaskLeft)
                    {
                        // LOGI("DEBUG! Replacing left eye with result texture");
                        replacedEyeBuffer = true;
                        imageHandle = gpMotionData[gMostRecentMotionLeft].resultTexture;
                    }

                    if (gGenerateBothEyes && (eyeMask & kEyeMaskRight))
                    {
                        // LOGI("DEBUG! Replacing right eye with result texture");
                        replacedEyeBuffer = true;
                        imageHandle = gpMotionData[gMostRecentMotionRight].resultTexture;
                    }
                }

                if (replacedEyeBuffer)
                {
                    // Since rendering a texture that is NEVER double wide, make sure texture coordinates are correct
                    shaderParams.uvScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
                    shaderParams.eyeBorder = glm::vec4(0.0f + onePixelWidth, 1.0f - onePixelHeight, 0.0f, 0.0f);
                    svrLayoutCoords *pOneLayout = &pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords;

                    //!< [0,1] = Lower Left UV values; [2,3] = Lower Right UV values
                    pOneLayout->LowerUVs[0] = 0.0f;
                    pOneLayout->LowerUVs[1] = 0.0f;
                    pOneLayout->LowerUVs[2] = 1.0f;
                    pOneLayout->LowerUVs[3] = 0.0f;

                    //!< [0,1] = Upper Left UV values; [2,3] = Upper Right UV values
                    pOneLayout->UpperUVs[0] = 0.0f;
                    pOneLayout->UpperUVs[1] = 1.0f;
                    pOneLayout->UpperUVs[2] = 1.0f;
                    pOneLayout->UpperUVs[3] = 1.0f;

                    // They are always standard textures
                    samplerType = GL_TEXTURE_2D;
                    // LOGI("DEBUG! replacedEyeBuffer = true");
                }
                else
                {
                    // LOGI("DEBUG! replacedEyeBuffer = false");
                }
            }
#endif // ENABLE_MOTION_VECTORS

            if (gWarpFrameCount > gSensorInitializeFrames) {
                // Is this a standard warp mesh or a simple blit?
                if (!isComposite &&
                    L_NeedCustomMesh(pWarpFrame->frameParams.renderLayers[whichLayer])) {
                    // LOGE("Blitting layer %d (eyeMask = 0x%08x)", whichLayer, eyeMask);
                    // L_BlitOneLayer(pWarpFrame, eyeMask, whichLayer);
                    shaderParams.pShader = pCurrentShader;
                    if (headLockLoop == 0)
                        L_SetShaderUniforms(&shaderParams, paramFlag, imageHandle, samplerType,
                                            gWarpData.eyeSamplerObj, eyeMask);
                    else
                        L_SetShaderUniforms(&shaderParams, paramFlag, imageHandle, samplerType,
                                            gWarpData.overlaySamplerObj, eyeMask);

                    // Update the overlay mesh and render it
                    svrLayoutCoords *pImageCoords = &pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords;

                    svrMeshLayout meshLayout;

                    if (true) {
                        svrLayoutCoords ndcCoords;
                        L_MakeNdcCoords(ndcCoords, *pImageCoords);
                        meshLayout.LL_Pos = glm::vec4(ndcCoords.LowerLeftPos[0],
                                                      ndcCoords.LowerLeftPos[1],
                                                      ndcCoords.LowerLeftPos[2],
                                                      ndcCoords.LowerLeftPos[3]);
                        meshLayout.LR_Pos = glm::vec4(ndcCoords.LowerRightPos[0],
                                                      ndcCoords.LowerRightPos[1],
                                                      ndcCoords.LowerRightPos[2],
                                                      ndcCoords.LowerRightPos[3]);
                        meshLayout.UL_Pos = glm::vec4(ndcCoords.UpperLeftPos[0],
                                                      ndcCoords.UpperLeftPos[1],
                                                      ndcCoords.UpperLeftPos[2],
                                                      ndcCoords.UpperLeftPos[3]);
                        meshLayout.UR_Pos = glm::vec4(ndcCoords.UpperRightPos[0],
                                                      ndcCoords.UpperRightPos[1],
                                                      ndcCoords.UpperRightPos[2],
                                                      ndcCoords.UpperRightPos[3]);
                    } else {
                        meshLayout.LL_Pos = glm::vec4(pImageCoords->LowerLeftPos[0],
                                                      pImageCoords->LowerLeftPos[1],
                                                      pImageCoords->LowerLeftPos[2],
                                                      pImageCoords->LowerLeftPos[3]);
                        meshLayout.LR_Pos = glm::vec4(pImageCoords->LowerRightPos[0],
                                                      pImageCoords->LowerRightPos[1],
                                                      pImageCoords->LowerRightPos[2],
                                                      pImageCoords->LowerRightPos[3]);
                        meshLayout.UL_Pos = glm::vec4(pImageCoords->UpperLeftPos[0],
                                                      pImageCoords->UpperLeftPos[1],
                                                      pImageCoords->UpperLeftPos[2],
                                                      pImageCoords->UpperLeftPos[3]);
                        meshLayout.UR_Pos = glm::vec4(pImageCoords->UpperRightPos[0],
                                                      pImageCoords->UpperRightPos[1],
                                                      pImageCoords->UpperRightPos[2],
                                                      pImageCoords->UpperRightPos[3]);
                    }

                    meshLayout.LL_UV = glm::vec2(pImageCoords->LowerUVs[0],
                                                 pImageCoords->LowerUVs[1]);
                    meshLayout.LR_UV = glm::vec2(pImageCoords->LowerUVs[2],
                                                 pImageCoords->LowerUVs[3]);
                    meshLayout.UL_UV = glm::vec2(pImageCoords->UpperUVs[0],
                                                 pImageCoords->UpperUVs[1]);
                    meshLayout.UR_UV = glm::vec2(pImageCoords->UpperUVs[2],
                                                 pImageCoords->UpperUVs[3]);

                    // Since shader is handling the position scale/offset, don't also add it to the mesh
                    glm::vec4 posScaleoffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
                    if (eyeMask & kEyeMaskLeft)
                        posScaleoffset = glm::vec4(0.5f, 1.0f, -0.5f + gMeshOffsetLeftX,
                                                   gMeshOffsetLeftY);
                    else if (eyeMask & kEyeMaskRight)
                        posScaleoffset = glm::vec4(0.5f, 1.0f, 0.5f + gMeshOffsetRightX,
                                                   gMeshOffsetRightY);
                    UpdateOverlayGeometry(meshLayout, posScaleoffset);

                    gWarpData.warpMeshes[kMeshOverlay].Submit(&gMeshAttribs[0], NUM_VERT_ATTRIBS);
                } else {
                    // Set the shader uniforms and render the correct mesh
                    shaderParams.pShader = pCurrentShader;
                    if (headLockLoop == 0 && !isComposite)
                        L_SetShaderUniforms(&shaderParams, paramFlag, imageHandle, samplerType,
                                            gWarpData.eyeSamplerObj, eyeMask);
                    else
                        L_SetShaderUniforms(&shaderParams, paramFlag, imageHandle, samplerType,
                                            gWarpData.overlaySamplerObj, eyeMask);

                    // Render the sphere mesh or not
                    if ((pWarpFrame->frameParams.renderLayers[whichLayer].imageType ==
                         kTypeEquiRectTexture) ||
                        (pWarpFrame->frameParams.renderLayers[whichLayer].imageType ==
                         kTypeEquiRectImage) ||
                        (pWarpFrame->frameParams.renderLayers[whichLayer].imageType ==
                         kTypeCubemapTexture)) {
                        gWarpData.warpMeshes[whichMeshSphere].Submit(&gMeshAttribs[0],
                                                                     NUM_VERT_ATTRIBS);
                    } else {
                        gWarpData.warpMeshes[whichMesh].Submit(&gMeshAttribs[0], NUM_VERT_ATTRIBS);
                    }
                }
            }
        }   // Looking through each layer

        // If there is still a shader bound then unbind it now
        if (pCurrentShader != NULL)
            pCurrentShader->Unbind();

    }   // Warped and head locked layers

    // We turned on blending, so turn it off
    GL(glDisable(GL_BLEND));
}

//-----------------------------------------------------------------------------
double L_MilliSecondsToNextVSync()
//-----------------------------------------------------------------------------
{
    // Need number of nanoseconds per VSync interval for later calculations
    uint64_t nanosPerVSync = 1000000000LL / (uint64_t) gAppContext->deviceInfo.displayRefreshRateHz;

    // Need mutex around reading gAppContext->modeContext->vsyncTimeNano
    pthread_mutex_lock(&gAppContext->modeContext->vsyncMutex);
    uint64_t timestamp = Svr::GetTimeNano(CLOCK_MONOTONIC);
    uint64_t latestVsyncTimeNano = gAppContext->modeContext->vsyncTimeNano;
    pthread_mutex_unlock(&gAppContext->modeContext->vsyncMutex);

    if (latestVsyncTimeNano > timestamp) {
        LOGE("MilliSecondsToNextVSync(): Error! VSync time is in the future by %0.4f ms!",
             (float) (latestVsyncTimeNano - timestamp) * 0.000001f);
        return 0.0;
    }

    // How long ago was the last VSync?
    uint64_t timeSinceVsync = timestamp - latestVsyncTimeNano;
    while (timeSinceVsync > nanosPerVSync) {
        double timeSinceVsyncMs = (double) timeSinceVsync * NANOSECONDS_TO_MILLISECONDS;
        double msPerVSync = (double) nanosPerVSync * NANOSECONDS_TO_MILLISECONDS;

        if (gLogPrediction)
            LOGE("Missing VSync time!  %0.4f milliseconds have passed but VSync interval is %0.4f!",
                 timeSinceVsyncMs, msPerVSync);

        // Since more than a vsync time has passed, assume time for last one.
        // This is NOT a while loop to subtract because in case the timeSinceVsync is huge we spin forever.
        timeSinceVsync -= nanosPerVSync;

        timeSinceVsyncMs = (double) timeSinceVsync * NANOSECONDS_TO_MILLISECONDS;
        if (gLogPrediction)
            LOGE("    Adjusted time since last VSync: %0.4f", timeSinceVsyncMs);

    }

    // How long until the next VSync?
    uint64_t timeToVsync = nanosPerVSync - timeSinceVsync;

    double returnTime = (double) timeToVsync * NANOSECONDS_TO_MILLISECONDS;

    return returnTime;
}

//-----------------------------------------------------------------------------
float L_MilliSecondsSinceVrStart()
//-----------------------------------------------------------------------------
{
    uint64_t timeNowNano = Svr::GetTimeNano();
    uint64_t diffTimeNano = timeNowNano - gVrStartTimeNano;
    float diffTimeMs = (float) diffTimeNano * NANOSECONDS_TO_MILLISECONDS;

    return diffTimeMs;
}

//-----------------------------------------------------------------------------
void *WarpThreadMain(void *arg)
//-----------------------------------------------------------------------------
{
    unsigned int prevWarpFrameCount = 0;
    mBHasData = false;

    LOGI("willie_test WarpThreadMain Entered");

    gAppContext->modeContext->warpThreadId = gettid();

    // Clean up the map to vulkan functions
    if (gVulkanMap.mcapacity != MAX_VULKAN_MAP) {
        gVulkanMap.Destroy();
        gVulkanMap.Init(MAX_VULKAN_MAP);
    }

    svrEventQueue *pEventQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    PROFILE_THREAD_NAME(0, 0, "TimeWarp");


    if (gUseQvrPerfModule && gAppContext->qvrHelper) {
        LOGI("Calling SetThreadAttributesByType for WARP thread...");
        QVRSERVICE_THREAD_TYPE threadType = gEnableWarpThreadFifo ? WARP : NORMAL;
        int32_t qRes = QVRServiceClient_SetThreadAttributesByType(gAppContext->qvrHelper,
                                                                  gAppContext->modeContext->warpThreadId,
                                                                  threadType);
        if (qRes != QVR_SUCCESS) {
            LOGE("QVRServiceClient_SetThreadAttributesByType failed for WARP thread.");
        }
    } else {
        if (gEnableWarpThreadFifo) {
            L_SetThreadPriority("TimeWarp Thread", SCHED_FIFO, gFifoPriorityWarp);
        }

        if (gTimeWarpThreadCore >= 0) {
            LOGI("Setting TimeWarp Affinity to %d", gTimeWarpThreadCore);
            svrSetThreadAffinity(gTimeWarpThreadCore);
        }
    }

    // Want the mesh type in an enumeration
    if (gWarpMeshType == 0)
        gMeshOrderEnum = kMeshOrderLeftToRight;
    else if (gWarpMeshType == 1)
        gMeshOrderEnum = kMeshOrderRightToLeft;
    else if (gWarpMeshType == 2)
        gMeshOrderEnum = kMeshOrderTopToBottom;
    else if (gWarpMeshType == 3)
        gMeshOrderEnum = kMeshOrderBottomToTop;

    // Correct to RightToLeft if orientation is LandscapeRight and set to render LeftToRight.
    // This is because the orientation sets the left eye when the right side is rastering out.
    // Later: Remove this because the Meta1 device actually is LandcapeRight but for some reason
    // the raster is LeftToRight!  Makes no sense, but there is tearing if RightToLeft!
    // if (gAppContext->deviceInfo.displayOrientation == 270 && gMeshOrderEnum == kMeshOrderLeftToRight)
    // {
    //     LOGI("Because application is orientated as Landscape-Left, setting mesh type to Right-To-Left");
    //     gMeshOrderEnum = kMeshOrderRightToLeft;
    // }

    svrCreateWarpContext();

    InitializeAsyncWarpData(&gWarpData);

    GL(glDisable(GL_DEPTH_TEST));
    GL(glDepthMask(GL_FALSE));
    GL(glEnable(GL_SCISSOR_TEST));

    if (gDirectModeWarp) {
        LOGI("Rendering in direct mode");
        GL(glEnable(GL_BINNING_CONTROL_HINT_QCOM));
        GL(glHint(GL_BINNING_CONTROL_HINT_QCOM, GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM));
    }

    GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    GLint nReadColorSpace = GL_NONE;
    GL(glReadBuffer(GL_BACK));
    GL(glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_BACK,
                                             GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                                             &nReadColorSpace));
    if (nReadColorSpace == GL_SRGB) {
        LOGI("Display Encoding sRGB");
    } else if (nReadColorSpace == GL_LINEAR) {
        LOGI("Display Encoding Linear");
    } else {
        LOGI("Display Encoding None");
    }

    // Call eglSwapBuffers once to mutate the surface to a single buffer surface.  Subsequent
    // calls are not necessary
    eglSwapBuffers(display, gAppContext->modeContext->warpRenderSurface);

    float clearColor[4] = {gTimeWarpClearColorR, gTimeWarpClearColorG, gTimeWarpClearColorB,
                           gTimeWarpClearColorA};

    // While waiting for the sensors to come up, wait a set number of frames
    gWarpFrameCount = 0;

    EGLSyncKHR eglSyncList[NUM_SWAP_FRAMES];
    memset(&eglSyncList, 0, sizeof(EGLSyncKHR) * NUM_SWAP_FRAMES);

    uint64_t lastLoopTime = Svr::GetTimeNano();

    if (gUseLinePtr) {
        gAppContext->modeContext->vsyncTimeNano = 0;
        gAppContext->modeContext->vsyncCount = 0;
    }

    // Verify vignette settings
    if (gMeshVignette) {
        if (gVignetteRadius >= 1.0f) {
            gMeshVignette = false;
            gVignetteRadius = 1.0f;
            LOGE("Vignette parameters are not correct.  Disabling Vignette!");
        }

        float endVignette = gLensPolyUnitRadius;
        if (gVignetteRadius >= endVignette) {
            gMeshVignette = false;
            gVignetteRadius = 1.0f;
            endVignette = 1.0f;
            LOGE("Vignette parameters will not work for this lens.  Disabling Vignette!");
        }
    }

#ifdef ENABLE_MOTION_VECTORS
    LOGI("****************************************");
    LOGI("Initializing motion vectors (%s)...", gEnableMotionVectors ? "True" : "False");
    LOGI("****************************************");
    if (gpMotionData != NULL)
        TerminateMotionVectors();

    if(gEnableMotionVectors)
        InitializeMotionVectors();
#endif // ENABLE_MOTION_VECTORS

    bool rightEyeHasWait = false;
    while (true) {
        PROFILE_SCOPE_DEFAULT(GROUP_TIMEWARP);

        //Check if the main thread has requested an exit
        if (gAppContext->modeContext->warpThreadExit) {
            LOGI("WarpThreadMain exit flag set");
            break;
        }

        // Simulate Timewarp not able to keep up.
        if (gTimeWarpMinLoopTime > 0) {
            uint64_t thisLoopTime = Svr::GetTimeNano();
            uint64_t loopDiffTime = thisLoopTime - lastLoopTime;

            uint64_t minLoopTime = 1000000 * (uint64_t) gTimeWarpMinLoopTime;

            // LOGI("gTimeWarpMinLoopTime != 0, Timewarp loop took %llu", loopDiffTime);
            // LOGI("gTimeWarpMinLoopTime != 0: Timewarp loop took %0.2f ms", (float)(loopDiffTime * NANOSECONDS_TO_MILLISECONDS));
            if (minLoopTime > loopDiffTime) {
                uint64_t diffTimeNano = minLoopTime - loopDiffTime;
                // float diffTimeMs = diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
                // LOGI("    Min time is %0.2f. Timewarp Sleeping for %0.2f ms...", (float)gTimeWarpMinLoopTime, diffTimeMs);
                NanoSleep(diffTimeNano);
            }

            lastLoopTime = thisLoopTime;
        }

        // Bump the count here
        gWarpFrameCount++;
        // if (gWarpFrameCount < gSensorInitializeFrames)
        //     LOGI("Frame not displayed while waiting for sensors to initialize (%d / %d)", gWarpFrameCount, gSensorInitializeFrames);

        // If Vr has ended (gAppContext->inVrMode) but we don't know yet, force a black screen
        if (!gAppContext->inVrMode) {
            gWarpFrameCount = -100;
            LOGI("Left Frame not displayed since not in VR mode");
            break;

            GL(glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                          gAppContext->modeContext->warpRenderSurfaceHeight));
            L_SetSurfaceScissor(kEntire);
            GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
            GL(glClear(GL_COLOR_BUFFER_BIT));
        }

        //Wait until the raster is at the halfway point, find a set of eye buffers and render the left eye
        double framePeriodNano = 1e9 / gAppContext->deviceInfo.displayRefreshRateHz;
        uint64_t warpTimeBiasNano = (uint64_t) (gTimeWarpWaitBias * MILLISECONDS_TO_NANOSECONDS);
        uint64_t waitTime;
        double framePct;

        uint64_t timestamp = Svr::GetTimeNano(CLOCK_MONOTONIC);

        //If we are using line pointer interrupts for tracking vSync then we need to query for the most recent vsync time stamp
        pthread_mutex_lock(&gAppContext->modeContext->vsyncMutex);

        if (gUseLinePtr) {
            uint64_t qvrTimeStamp = 0;
            if(gAppContext->mUseIvSlam){
                qvrTimeStamp = A11GDeviceClient_GetLastVsync(gAppContext->a11GdeviceClientHelper);
            }else {
                qvrservice_ts_t *qvrStamp;
                QVRServiceClient_GetDisplayInterruptTimestamp(gAppContext->qvrHelper,
                                                              DISP_INTERRUPT_LINEPTR, &qvrStamp);
                qvrTimeStamp = qvrStamp->ts;
            }
            // Have not ever seen it, but want to verify that the qvr timestamp is actually in the past
            if (qvrTimeStamp > timestamp) {
                LOGE("QVR VSync timestamp is %0.2f in the future!",
                     (float) (qvrTimeStamp - timestamp) * 0.000001f);
                qvrTimeStamp = timestamp;
            }

            if (gLogVSyncData) {
                LOGI("QVR last VSync Time: %0.4f ms ago!",
                     (float) (timestamp - qvrTimeStamp) * 0.000001f);
            }

            if (((int64_t) (timestamp - qvrTimeStamp)) > framePeriodNano) {
                //We've managed to get here prior to the next vsync occuring so set it based on the previous (interrupt may have been delayed)
                gAppContext->modeContext->vsyncTimeNano = qvrTimeStamp + framePeriodNano;

                if (gLogVSyncData) {
                    LOGI("    Adding frame period time (%0.2f) to vsync time",
                         framePeriodNano * 0.000001f);
                    LOGI("    Last VSync was %0.4f ms ago!",
                         (float) (timestamp - gAppContext->modeContext->vsyncTimeNano) * 0.000001f);
                }
            } else {
                gAppContext->modeContext->vsyncTimeNano = qvrTimeStamp;
            }

            gAppContext->modeContext->vsyncCount++;
        }

        framePct = (double) gAppContext->modeContext->vsyncCount +
                   ((double) timestamp - gAppContext->modeContext->vsyncTimeNano) /
                   (framePeriodNano);

        pthread_mutex_unlock(&gAppContext->modeContext->vsyncMutex);


        uint64_t warpVsyncCount = ceil(framePct);

        double fractFrame = framePct - ((long) framePct);

        if (fractFrame < 0.5) {
            //We are currently in the first half of the display so wait until we hit the halfway point
            waitTime = (uint64_t) ((0.5 - fractFrame) * framePeriodNano);
        } else {
            //We are currently past the halfway point in the display so wait until halfway through the next vsync cycle
            waitTime = (uint64_t) ((1.5 - fractFrame) * framePeriodNano);
        }

        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Left EyeBuffer Wait : %f",
                      (double) (waitTime + warpTimeBiasNano) * 1e-6);

        float refreshPeriodMs = 1000 / gAppContext->deviceInfo.displayRefreshRateHz;
        NanoSleep(waitTime + warpTimeBiasNano);

        PROFILE_EXIT(GROUP_TIMEWARP); // Left EyeBuffer Wait

        uint64_t postLeftWaitTimeStamp = Svr::GetTimeNano(CLOCK_MONOTONIC);

        //Get the frame parameters for the frame we will be warping
        svrFrameParamsInternal *pWarpFrame = L_GetNextFrameToWarp(warpVsyncCount, eglSyncList);
        LOGV("Warping %d (Ring = %d) [vc : %llu]", gAppContext->modeContext->warpFrameCount,
             gAppContext->modeContext->warpFrameCount % NUM_SWAP_FRAMES,
             gAppContext->modeContext->vsyncCount);

        //Signal the eye render thread that it can continue on
        if (!pthread_mutex_trylock(&gAppContext->modeContext->warpBufferConsumedMutex)) {
            pthread_cond_signal(&gAppContext->modeContext->warpBufferConsumedCv);
            pthread_mutex_unlock(&gAppContext->modeContext->warpBufferConsumedMutex);
        }

        if (pWarpFrame == NULL) {
            //No frame to warp (just started rendering??) so start over...
            LOGV("No valid frame to warp...");
            continue;
        }

        gFrameDoubled = 0;
        if (gAppContext->modeContext->warpFrameCount == prevWarpFrameCount) {
            gFrameDoubled = 1;
        }
        if (gLogFrameDoubles && gFrameDoubled) {
            LOGI("Frame doubled on %d [%llu,%llu]", gAppContext->modeContext->warpFrameCount,
                 (long long unsigned int) warpVsyncCount,
                 (long long unsigned int) gAppContext->modeContext->vsyncCount);
        }
        prevWarpFrameCount = gAppContext->modeContext->warpFrameCount;

        if (pWarpFrame->warpFrameLeftTimeStamp == 0) {
            pWarpFrame->warpFrameLeftTimeStamp = postLeftWaitTimeStamp;
        }

        // See if we have dropped a submitted frame (not the same as doubled)
        if (gLogDroppedFrames && gFrameDoubled == 0 && gLastFrameIndx != 0) {
            if (pWarpFrame->frameParams.frameIndex != gLastFrameIndx + 1) {
                LOGE("Dropped Frame: Expected to warp frame %d, but got frame %d",
                     gLastFrameIndx + 1, pWarpFrame->frameParams.frameIndex);
            }
        }
        gLastFrameIndx = pWarpFrame->frameParams.frameIndex;


        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Warp Submission : VSync %d Frame %d Doubled %d",
                      (int) (gAppContext->modeContext->vsyncCount) % 8 + 1,
                      (int) (gAppContext->modeContext->warpFrameCount) % 8 + 1,
                      gFrameDoubled
        );

        glm::fquat origQuat, latestQuat;

        // We want a one pixel border applied to "eyeBorder"
        float onePixelWidth = 0.0f;
        if (gAppContext->deviceInfo.targetEyeWidthPixels != 0)
            onePixelWidth = 1.0f / (float) gAppContext->deviceInfo.targetEyeWidthPixels;

        // Set up the shader parameters
        glm::mat4 leftTextureMatrix = glm::mat4(1.0f);
        glm::mat4 rightTextureMatrix = glm::mat4(1.0f);
        glm::mat2 skewSquashMatrix = glm::mat2(1.0f);

        bool bDistortionEnabled = !(
                (pWarpFrame->frameParams.frameOptions & kDisableDistortionCorrection) > 0);
        bool bTimeWarpEnabled = !((pWarpFrame->frameParams.frameOptions & kDisableReprojection) >
                                  0);

        // If distortion is disabled then there is no reason to do chromatic aberration
        if (!bDistortionEnabled) {
            pWarpFrame->frameParams.frameOptions &= kDisableChromaticCorrection;
        }

        if (gRecenterTransition < gRecenterFrames) {
            // Need to disable reprojection for a few frames after recentering
            gRecenterTransition++;
            //LOGE("Reprojection Disabled for this frame (Left): %d", gRecenterTransition);
            pWarpFrame->frameParams.frameOptions &= kDisableReprojection;
            bTimeWarpEnabled = false;
        }

        // We have a predicted time that was used to render each eye.  Look at what we predicted and
        // where we are now and adjust the warp matrix to correct.
        // This is done by CalculateWarpMatrix() so no special code is needed.
        // Check and see if the predicted time is close to where we are now.
        uint64_t timeNowNano = Svr::GetTimeNano();

        float adjustedTimeMs = 0.0f;
        if (3 == gPredictVersion) { //QVR3.0 Prediction
            if (!gDisablePredictedTime &&
                !(pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton)) {
                // Predict ahead
                double timeToVsyncMs = L_MilliSecondsToNextVSync();

                // Start with how long to next Vsync...
                adjustedTimeMs = (float) timeToVsyncMs;

                // ... then we add time to half exposure (already in milliseconds)
                adjustedTimeMs += gTimeToHalfExposure;
                // willie change start 2019-3-21
                if (gMeshOrderEnum == kMeshOrderLeftToRight ||
                    gMeshOrderEnum == kMeshOrderRightToLeft) {
                    adjustedTimeMs += gExtraAdjustPeriodRatio * refreshPeriodMs;
                }
                // willie change end 2019-3-21
            }
        } else if (2 == gPredictVersion) { //QVR2.0 Prediction
            if (!gDisablePredictedTime &&
                !(pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton)) {
                //Predict ahead
                adjustedTimeMs = (0.5 * framePeriodNano) * 1e-6;
            }

            // If mesh type is not "Left to Right" then TimeWarp is disabled, but we need to use the latest
            // predicted time for both eyes.  The current adjusted time is only for the first half so we double
            // it to get where the second half should be
            if (gMeshOrderEnum == kMeshOrderTopToBottom ||
                gMeshOrderEnum == kMeshOrderBottomToTop) {
                adjustedTimeMs *= 2.0f;
            }
        }

        // TMP start 
        // float tmpPredict30 = svrGetPredictedDisplayTimePipelined(1);
        // double tmpTimeToVsyncMs = L_MilliSecondsToNextVSync();
        // // Start with how long to next Vsync...
        // float tmpAdjustedTimeMs = (float) tmpTimeToVsyncMs;
        // // ... then we add time to half exposure (already in milliseconds)
        // tmpAdjustedTimeMs += gTimeToHalfExposure;
        // LOGI("willie_test svrApiTimeWarp.cpp WarpThreadMain adjustedTimeMs=%f fractFrame=%f tmpPredict30=%f", adjustedTimeMs, fractFrame, tmpAdjustedTimeMs);
        // TMP end

        uint64_t adjustedTimeNano = (uint64_t) (adjustedTimeMs * MILLISECONDS_TO_NANOSECONDS);
        float leftAdjustTimeMs = adjustedTimeMs + timeNowNano * 1.0e-6;

        if (gHeuristicPredictedTime) {
            // Only the first time gets added to the heuristics
            uint64_t leftWarpTime = timeNowNano + adjustedTimeNano;
            uint64_t poseFetchTime = pWarpFrame->frameParams.headPoseState.poseFetchTimeNs;
            L_AddHeuristicPredictData(leftWarpTime - poseFetchTime);
        }

        if (gLogPrediction) {
            LOGI("Warp Left adjustedTimeMs=%f", adjustedTimeMs);
            uint64_t leftWarpTime = timeNowNano + adjustedTimeNano;
            uint64_t expectedTime = pWarpFrame->frameParams.headPoseState.expectedDisplayTimeNs;
            float timeSinceVrStart = L_MilliSecondsSinceVrStart();
            if (leftWarpTime > expectedTime) {
                // The expected time was not far enough out
                uint64_t diffTimeNano = leftWarpTime - expectedTime;
                float diffTimeMs = (float) diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
                LOGI("    (%0.3f ms) Left eye (Fame %d) predicted display time is %0.4f ms before warp expected display time!",
                     timeSinceVrStart, pWarpFrame->frameParams.frameIndex, diffTimeMs);
            } else {
                // The expected time was too far out
                uint64_t diffTimeNano = expectedTime - leftWarpTime;
                float diffTimeMs = (float) diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
                LOGI("    (%0.3f ms) Left eye (Fame %d) predicted display time is %0.4f ms after warp expected display time!",
                     timeSinceVrStart, pWarpFrame->frameParams.frameIndex, diffTimeMs);
            }
        }

        int64_t timeToLeftEye = 0L;
        int64_t timeToRightEye = 0L;

        // Up to 4 meshes are rendered each pass.  If Left/Right order then 2 are rendered (Each half of screen)
        // If Top/Bottom order then 4 are rendered (Each corner of the screen).
        // The logic for which mesh is rendered when gets complicated so define them here.
        unsigned int mesh1A = 0;    // First part of "Left" eye
        unsigned int mesh1B = 0;    // Second part of "Left" eye
        unsigned int mesh2A = 0;    // First part of "Right" eye
        unsigned int mesh2B = 0;    // Second part of "Right" eye

        unsigned int mesh1A_Sphere = 0;    // First part of "Left" eye (Sphere Mesh)
        unsigned int mesh1B_Sphere = 0;    // Second part of "Left" eye (Sphere Mesh)
        unsigned int mesh2A_Sphere = 0;    // First part of "Right" eye (Sphere Mesh)
        unsigned int mesh2B_Sphere = 0;    // Second part of "Right" eye (Sphere Mesh)

        switch (gMeshOrderEnum) {
            case kMeshOrderLeftToRight:
                mesh1A = bDistortionEnabled ? kMeshLeft : kMeshLeft_Low;
                mesh1B = 0;
                mesh2A = bDistortionEnabled ? kMeshRight : kMeshRight_Low;
                mesh2B = 0;

                mesh1A_Sphere = kMeshLeft_Sphere;
                mesh1B_Sphere = 0;
                mesh2A_Sphere = kMeshRight_Sphere;
                mesh2B_Sphere = 0;
                break;

            case kMeshOrderRightToLeft:
                mesh1A = bDistortionEnabled ? kMeshRight : kMeshRight_Low;
                mesh1B = 0;
                mesh2A = bDistortionEnabled ? kMeshLeft : kMeshLeft_Low;
                mesh2B = 0;

                mesh1A_Sphere = kMeshRight_Sphere;
                mesh1B_Sphere = 0;
                mesh2A_Sphere = kMeshLeft_Sphere;
                mesh2B_Sphere = 0;
                break;

            case kMeshOrderTopToBottom:
                mesh1A = bDistortionEnabled ? kMeshUL : kMeshUL_Low;
                mesh1B = bDistortionEnabled ? kMeshUR : kMeshUR_Low;
                mesh2A = bDistortionEnabled ? kMeshLL : kMeshLL_Low;
                mesh2B = bDistortionEnabled ? kMeshLR : kMeshLR_Low;

                mesh1A_Sphere = kMeshUL_Sphere;
                mesh1B_Sphere = kMeshUR_Sphere;
                mesh2A_Sphere = kMeshLL_Sphere;
                mesh2B_Sphere = kMeshLR_Sphere;
                break;

            case kMeshOrderBottomToTop:
                mesh1A = bDistortionEnabled ? kMeshLL : kMeshLL_Low;
                mesh1B = bDistortionEnabled ? kMeshLR : kMeshLR_Low;
                mesh2A = bDistortionEnabled ? kMeshUL : kMeshUL_Low;
                mesh2B = bDistortionEnabled ? kMeshUR : kMeshUR_Low;

                mesh1A_Sphere = kMeshLL_Sphere;
                mesh1B_Sphere = kMeshLR_Sphere;
                mesh2A_Sphere = kMeshUL_Sphere;
                mesh2B_Sphere = kMeshUR_Sphere;
                break;
        }

        // Handle mesh overrides
        L_SelectOverrideMesh(mesh1A, mesh1B, mesh2A, mesh2B);

        // LOGI("Warping Eye Buffers: Left = %d; Right = %d", leftEyeBuffer, rightEyeBuffer);

        // TODO: Does "gAppContext->deviceInfo.displayOrientation == 0|180" and Top/Bottom mesh order require special handling?

        // LOGI("Viewport: %d, %d, %d, %d", 0, 0, gAppContext->modeContext->warpRenderSurfaceWidth, gAppContext->modeContext->warpRenderSurfaceHeight);
        GL(glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                      gAppContext->modeContext->warpRenderSurfaceHeight));
        L_SetSurfaceScissor(kEntire);

        glm::mat4 eyeProjectionMatrix = CalculateProjectionMatrix();
        if (gDisableLensCorrection) {
            eyeProjectionMatrix = glm::mat4(1);
        }

        glm::fquat displayQuat, vsyncStartQuat, vsyncEndQuat;

        //*******************
        //Left Eye
        //*******************
//        if (gWarpFrameCount % 150 == 0) {
//            LOGI("TEST fetch cloud point start");
//            svrGetPointCloudData();
//            LOGI("TEST fetch cloud point done");
//        }

        glm::mat4 leftWarpMatrix = glm::mat4(1.0f);
        if (gDisableLensCorrection) {
            leftWarpMatrix[3][2] = 1.0f;
        }
        float deltaYaw = 0;
        float deltaPitch = 0;
        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Left Eye - Pose Update");
        if (bTimeWarpEnabled) {
            //Get the updated sensor state
            //LOGI("TimeWarp left eye predicted time: %0.2f ms", adjustedTimeMs);
            float timeToVsyncMs = (float) L_MilliSecondsToNextVSync();

            svrHeadPoseState vsyncStartPoseState = svrGetPredictedHeadPose(timeToVsyncMs);
            svrHeadPoseState vsyncEndPoseState = svrGetPredictedHeadPose(
                    timeToVsyncMs + (1000.0f / gAppContext->deviceInfo.displayRefreshRateHz));

            svrHeadPoseState displayPoseState = svrGetPredictedHeadPose(adjustedTimeMs);
            svrHeadPoseState nextDisplayPoseState = svrGetPredictedHeadPose(adjustedTimeMs + (1000.0f / gAppContext->deviceInfo.displayRefreshRateHz));

            origQuat = glmQuatFromSvrQuat(pWarpFrame->frameParams.headPoseState.pose.rotation);

            vsyncStartQuat = glmQuatFromSvrQuat(vsyncStartPoseState.pose.rotation);
            vsyncEndQuat = glmQuatFromSvrQuat(vsyncEndPoseState.pose.rotation);

            displayQuat = glmQuatFromSvrQuat(displayPoseState.pose.rotation);
            glm::quat nextDisplayQuat = glmQuatFromSvrQuat(nextDisplayPoseState.pose.rotation);
            glm::mat4 nextDisplayR = glm::mat4_cast(nextDisplayQuat);

            if (gDebugWithProperty) {
                char value[20] = {0x00};
                __system_property_get("svr.preTwist", value);
                if ('\0' != value[0]) {
                    gPreTwistRatioMode = atoi(value);
                }
                __system_property_get("svr.gPreTwistRatio", value);
                if ('\0' != value[0]) {
                    gPreTwistRatio = atof(value);
                }
                __system_property_get("svr.gPreTwistRatioY", value);
                if ('\0' != value[0]) {
                    gPreTwistRatioY = atof(value);
                }
            }

            if (gMeshOrderEnum == kMeshOrderTopToBottom ||
                gMeshOrderEnum == kMeshOrderBottomToTop) {
                if (gEnablePreTwist) {
                    glm::mat4 currR = glm::mat4_cast(displayQuat);
                    if (mBHasData) {
                        glm::mat4 deltaMtx = mLastR * glm::transpose(currR);
                        if (1 == gPreTwistRatioMode) {
                            deltaMtx = currR * glm::transpose(nextDisplayR);
                        } else if (gPreTwistRatioMode >= 2) {
                            deltaMtx = glm::mat4(1);
                        }
                        deltaYaw = deltaMtx[2][0] / deltaMtx[2][2] * gPreTwistRatio;
                        float yaw = atan2(deltaMtx[2][0], deltaMtx[2][2]);
                        deltaPitch = -deltaMtx[2][1] / deltaMtx[2][2] * glm::cos(yaw) * gPreTwistRatioY / gLeftFrustum_Top * gLeftFrustum_Near;
//                        LOGI("TEST deltaYaw=%f, deltaPitch=%f", deltaYaw, deltaPitch);
                    } else {
                        mBHasData = true;
                    }
                    mLastR = currR;
                }
            }

            leftWarpMatrix = CalculateWarpMatrix(origQuat, displayQuat, deltaYaw, deltaPitch);
            timeToLeftEye = displayPoseState.poseTimeStampNs -
                            pWarpFrame->frameParams.headPoseState.poseTimeStampNs;

            glm::fquat diff = vsyncStartQuat * glm::inverse(vsyncEndQuat);
            glm::vec3 diffEuler = glm::eulerAngles(diff);

            float diffYaw = diffEuler.y;
            float diffPitch = diffEuler.x;

            skewSquashMatrix[0][0] = 1.0f;
            skewSquashMatrix[1][1] = 1.0f;

            if (gApplyDisplaySquash) {
                float fovYmeters = (gLeftFrustum_Top + fabs(gLeftFrustum_Bottom)) * 1000.0f;

                if (gWarpMeshType == 0) //Columns left->right
                {
                    float horizontalScale =
                            -((gLeftFrustum_Near * 1000.0f) * tan(diffYaw)) / fovYmeters;
                    skewSquashMatrix[0][0] = 1.0f + (gSquashScaleFactor * horizontalScale);
                } else if (gWarpMeshType == 1) //Columns right->left
                {
                    float horizontalScale =
                            ((gLeftFrustum_Near * 1000.0f) * tan(diffYaw)) / fovYmeters;
                    skewSquashMatrix[0][0] = 1.0f + (gSquashScaleFactor * horizontalScale);
                } else if (gWarpMeshType == 2) //Rows top->bottom
                {
                    float verticalScale =
                            -((gLeftFrustum_Near * 1000.0f) * tan(diffPitch)) / fovYmeters;
                    skewSquashMatrix[1][1] = 1.0f + (gSquashScaleFactor * verticalScale);
                } else if (gWarpMeshType == 3) {
                    float verticalScale =
                            ((gLeftFrustum_Near * 1000.0f) * tan(diffPitch)) / fovYmeters;
                    skewSquashMatrix[1][1] = 1.0f + (gSquashScaleFactor * verticalScale);
                }
            }

            if (gApplyDisplaySkew) {
                //Only apply skew in the case of top->bottom display
                if (gWarpMeshType == 2) //Rows top->bottom
                {
                    skewSquashMatrix[0][1] = gSkewScaleFactor * diffYaw;
                }
            }
        }

        // Need to set both since they are used based on mesh type
        leftTextureMatrix = leftWarpMatrix * eyeProjectionMatrix;
        rightTextureMatrix = leftWarpMatrix * eyeProjectionMatrix;

        PROFILE_EXIT(GROUP_TIMEWARP); //Left Eye - Pose Update

#ifdef MOTION_TO_PHOTON_TEST
        if (pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton)
        {
            // Compute the difference of quaternions between the current sample and the last
            glm::fquat inverseLast = glm::inverse(gMotionToPhotonLast);
            glm::fquat diffValue = displayQuat * inverseLast;
            gMotionToPhotonW = diffValue.w;
            if (gMotionToPhotonW < 0)
            {
                gMotionToPhotonW = -gMotionToPhotonW;
            }

            //LOGI("gMotionToPhotonW %f  %f", gMotionToPhotonW,gMotionToPhotonAccThreshold);

            if (gMotionToPhotonAccThreshold > 0.0f)
            {
                clearColor[0] = clearColor[1] = clearColor[2] = 0.0f;

                if (bRightFlash)
                {
                    //During the right eye render motion was picked up so automatically
                    //flash for this eye then clear the flags
                    clearColor[0] = clearColor[1] = clearColor[2] = 1.0f;
                    bRightFlash = false;
                    bLeftFlash = false;
                    PROFILE_ENTER(GROUP_TIMEWARP, 0, "MTP_FLASH_CLR|1");
                    PROFILE_EXIT(GROUP_TIMEWARP);
                }
                else if (gMotionToPhotonW < gMotionToPhotonAccThreshold)
                {
                    uint64_t motoPhoTimeStamp = Svr::GetTimeNano();
                    if (((motoPhoTimeStamp - gMotionToPhotonTimeStamp) * NANOSECONDS_TO_MILLISECONDS) > 500.0f)
                    {
                        //It's been more than 1/2s since the last time we flashed
                        clearColor[0] = clearColor[1] = clearColor[2] = 1.0f;
                        gMotionToPhotonTimeStamp = motoPhoTimeStamp;
                        bLeftFlash = true;
                        PROFILE_ENTER(GROUP_TIMEWARP, 0, "MTP_FLASH_CLR|2");
                        PROFILE_EXIT(GROUP_TIMEWARP);
                    }
                }
            }
            else
            {
                float factor = gMotionToPhotonC * gMotionToPhotonW;
                if (factor > 1.0f)
                    factor = 1.0f;

                clearColor[0] = factor;
                clearColor[1] = factor;
                clearColor[2] = factor;
            }

            gMotionToPhotonLast = displayQuat;
        }
#endif // MOTION_TO_PHOTON_TEST

        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Left Eye Submit");

        if (gTimeWarpClearBuffer ||
            gOverrideMeshClearLeft ||
            (pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton) ||
            !gDirectModeWarp) {
            LOGI("willie_test before glClearColor");
            GL(glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f));

            switch (gMeshOrderEnum) {
                case kMeshOrderLeftToRight:
                    gDirectModeWarp ? L_SetSurfaceScissor(kLeftHalf) : L_StartTiledArea(kLeftHalf);
                    break;

                case kMeshOrderRightToLeft:
                    gDirectModeWarp ? L_SetSurfaceScissor(kRightHalf) : L_StartTiledArea(
                            kRightHalf);
                    break;

                case kMeshOrderTopToBottom:
                    gDirectModeWarp ? L_SetSurfaceScissor(kTopHalf) : L_StartTiledArea(kTopHalf);
                    break;

                case kMeshOrderBottomToTop:
                    gDirectModeWarp ? L_SetSurfaceScissor(KBottomHalf) : L_StartTiledArea(
                            KBottomHalf);
                    break;
            }

            GL(glClear(GL_COLOR_BUFFER_BIT));
        }

        // Have cleared this side, so reset the flag
        gOverrideMeshClearLeft = false;

#ifdef MOTION_TO_PHOTON_TEST
        if (!(pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton))
        {
#endif // MOTION_TO_PHOTON_TEST

        switch (gMeshOrderEnum) {
            case kMeshOrderLeftToRight:
                L_RenderOneEye(pWarpFrame, kEyeMaskLeft, kLeftHalf, mesh1A, mesh1A_Sphere,
                               leftTextureMatrix, skewSquashMatrix);
                break;

            case kMeshOrderRightToLeft:
                L_RenderOneEye(pWarpFrame, kEyeMaskRight, kRightHalf, mesh1A, mesh1A_Sphere,
                               rightTextureMatrix, skewSquashMatrix);
                break;

            case kMeshOrderTopToBottom:
                L_RenderOneEye(pWarpFrame, kEyeMaskLeft, kLeftHalf, mesh1A, mesh1A_Sphere,
                               leftTextureMatrix, skewSquashMatrix);
                L_RenderOneEye(pWarpFrame, kEyeMaskRight, kRightHalf, mesh1B, mesh1B_Sphere,
                               rightTextureMatrix, skewSquashMatrix);
                break;

            case kMeshOrderBottomToTop:
                L_RenderOneEye(pWarpFrame, kEyeMaskLeft, kLeftHalf, mesh1A, mesh1A_Sphere,
                               leftTextureMatrix, skewSquashMatrix);
                L_RenderOneEye(pWarpFrame, kEyeMaskRight, kRightHalf, mesh1B, mesh1B_Sphere,
                               rightTextureMatrix, skewSquashMatrix);
                break;
        }   // switch (gMeshOrderEnum)
#ifdef MOTION_TO_PHOTON_TEST
        }   // Not motion to photon text
#endif // MOTION_TO_PHOTON_TEST

        PROFILE_EXIT(GROUP_TIMEWARP); //Left Eye Submit

        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Left Eye Finalize");
        if (!gDirectModeWarp) {
            GL(glEndTilingQCOM(GL_COLOR_BUFFER_BIT0_QCOM));
        }
        if (gSingleBufferWindow) {
            GL(glFlush());
        }
        PROFILE_EXIT(GROUP_TIMEWARP); // Left Eye Finalize

        //Wait for the time point when the raster should be at the left edge of the display to render the right eye
        if (gSingleBufferWindow) {
            uint64_t rightTimestamp = Svr::GetTimeNano(CLOCK_MONOTONIC);


            //We started the left eye half way through vsync, figure out how long
            //we have left in the half frame until the raster starts over so we can render the right eye
            uint64_t delta = rightTimestamp - postLeftWaitTimeStamp;
            uint64_t waitTime = 0;
            if (delta < ((uint64_t) (framePeriodNano / 2.0 - gRightEyeAdjustBias*1000000))) {
                waitTime = ((uint64_t) (framePeriodNano / 2.0-gRightEyeAdjustBias*1000000)) - delta;

                // if (gWarpMeshType == 0) //Rows top->bottom
                {
                    PROFILE_ENTER(GROUP_TIMEWARP, 0, "Right EyeBuffer Wait : %f",
                                  (double) waitTime * 1e-6);
                    NanoSleep(waitTime);
                    rightEyeHasWait = true;
                }
                PROFILE_EXIT(GROUP_TIMEWARP); // Right EyeBuffer Wait
            } else {
                //The left eye took longer than 1/2 the refresh so the raster has already wrapped around and is
                //in the left half of the screen.  Skip the wait and get right on rendering the right eye.
                LOGE("Left Eye took too long!!! ( %2.3f ms )", delta * NANOSECONDS_TO_MILLISECONDS);
                PROFILE_MESSAGE(GROUP_TIMEWARP, 0, "Left Eye took too long!!! ( %2.3f ms )",
                                delta * NANOSECONDS_TO_MILLISECONDS);
            }
        }

        if (pWarpFrame->warpFrameRightTimeStamp == 0) {
            pWarpFrame->warpFrameRightTimeStamp = GetTimeNano();
        }

        //*******************
        //Right Eye
        //*******************

        // If Vr has ended (gAppContext->inVrMode) but we don't know yet, force a black screen
        if (!gAppContext->inVrMode) {
            gWarpFrameCount = -100;
            LOGI("Right Frame not displayed since not in VR mode");
            break;

            GL(glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                          gAppContext->modeContext->warpRenderSurfaceHeight));
            L_SetSurfaceScissor(kEntire);
            GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
            GL(glClear(GL_COLOR_BUFFER_BIT));
        }

        // As the meshes are moved using gMeshOffset[Left|Right] they can stomp on each other.
        GL(glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                      gAppContext->modeContext->warpRenderSurfaceHeight));
        L_SetSurfaceScissor(kEntire);

        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Right Eye Pose Update");

        // Update the time warp adjustment
        timeNowNano = Svr::GetTimeNano();

        adjustedTimeMs = 0.0f;
        if (3 == gPredictVersion) {
            if (!gDisablePredictedTime &&
                !(pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton)) {
                // Predict ahead
                double timeToVsyncMs = L_MilliSecondsToNextVSync();

                // Start with how long to next Vsync...
                adjustedTimeMs = (float) timeToVsyncMs;

                // ... then we add time to half exposure (already in milliseconds)
                adjustedTimeMs += gTimeToHalfExposure;

                // willie change start 2019-3-6
                float timeToVsyncRatio = timeToVsyncMs / refreshPeriodMs;
//                LOGI("willie_test gWarpFrameCount=%d Right timeToVsyncMs=%f timeToVsyncRatio=%f adjustedTimeMs=%f",
//                     gWarpFrameCount, timeToVsyncMs, timeToVsyncRatio, adjustedTimeMs);
                // willie change end 2019-3-6

                // willie change start 2019-3-6
//            float extraAdjustRightMs = 0;
//            __system_property_get("persist.sys.adjust_right", value);
//            if ('\0' != value[0]) {
//                extraAdjustRightMs = atof(value);
//            }
//            adjustedTimeMs += extraAdjustRightMs;
                // willie change end 2019-3-6
            }
        } else if (2 == gPredictVersion) {
            if (!gDisablePredictedTime &&
                !(pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton)) {
                adjustedTimeMs = (0.5 * framePeriodNano) * 1e-6;
            }
        }
        adjustedTimeNano = (uint64_t) (adjustedTimeMs * MILLISECONDS_TO_NANOSECONDS);

        if (gLogPrediction) {
            uint64_t rightWarpTime = timeNowNano + adjustedTimeNano;
            uint64_t expectedTime = pWarpFrame->frameParams.headPoseState.expectedDisplayTimeNs;
            float timeSinceVrStart = L_MilliSecondsSinceVrStart();
            if (rightWarpTime > expectedTime) {
                // The expected time was not far enough out
                uint64_t diffTimeNano = rightWarpTime - expectedTime;
                float diffTimeMs = (float) diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
                LOGI("    (%0.3f ms) Right eye (Fame %d) predicted display time is %0.4f ms before warp expected display time!",
                     timeSinceVrStart, pWarpFrame->frameParams.frameIndex, diffTimeMs);
            } else {
                // The expected time was too far out
                uint64_t diffTimeNano = expectedTime - rightWarpTime;
                float diffTimeMs = (float) diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
                LOGI("    (%0.3f ms) Right eye (Fame %d) predicted display time is %0.4f ms after warp expected display time!",
                     timeSinceVrStart, pWarpFrame->frameParams.frameIndex, diffTimeMs);
            }
        }


        // It is possible that the recenter event came in between eyes so whe need to check again
        if (bTimeWarpEnabled && gRecenterTransition < gRecenterFrames) {
            // Need to disable reprojection for a few frames after recentering
            gRecenterTransition++;
            //LOGE("Reprojection Disabled for this frame (Right): %d", gRecenterTransition);
            bTimeWarpEnabled = false;
        }

        glm::mat4 rightWarpMatrix = glm::mat4(1.0);
        if (gDisableLensCorrection) {
            rightWarpMatrix[3][2] = 1.0f;
        }

        if (bTimeWarpEnabled) {
            svrHeadPoseState displayPoseState;

            if (gMeshOrderEnum == kMeshOrderTopToBottom ||
                gMeshOrderEnum == kMeshOrderBottomToTop) {
                rightWarpMatrix = leftWarpMatrix;
            } else {

                //Get the updated sensor state
                // LOGI("TimeWarp right eye predicted time: %0.2f ms", adjustedTimeMs);
                displayPoseState = svrGetPredictedHeadPose(adjustedTimeMs);
                origQuat = glmQuatFromSvrQuat(pWarpFrame->frameParams.headPoseState.pose.rotation);
                displayQuat = glmQuatFromSvrQuat(displayPoseState.pose.rotation);
                rightWarpMatrix = CalculateWarpMatrix(origQuat, displayQuat, deltaYaw, deltaPitch);
            }

            timeToRightEye = displayPoseState.poseTimeStampNs -
                             pWarpFrame->frameParams.headPoseState.poseTimeStampNs;
        }

        // Need to set both since they are used based on mesh type
        leftTextureMatrix = rightWarpMatrix * eyeProjectionMatrix;
        rightTextureMatrix = rightWarpMatrix * eyeProjectionMatrix;

        PROFILE_EXIT(GROUP_TIMEWARP); // Right Eye Pose Update

#ifdef MOTION_TO_PHOTON_TEST
        if (pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton)
        {
            // Compute the difference of quaternions between the current sample and the last
            glm::fquat inverseLast = glm::inverse(gMotionToPhotonLast);
            glm::fquat diffValue = displayQuat * inverseLast;
            gMotionToPhotonW = diffValue.w;
            if (gMotionToPhotonW < 0)
            {
                gMotionToPhotonW = -gMotionToPhotonW;
            }

            //LOGI("gMotionToPhotonW %f  %f", gMotionToPhotonW,gMotionToPhotonAccThreshold);

            if (gMotionToPhotonAccThreshold > 0.0f)
            {
                clearColor[0] = clearColor[1] = clearColor[2] = 0.0f;

                if (bLeftFlash)
                {
                    //During the left eye render motion was picked up so automatically
                    //flash for this eye then clear the flags
                    clearColor[0] = clearColor[1] = clearColor[2] = 1.0f;
                    PROFILE_ENTER(GROUP_TIMEWARP, 0, "MTP_FLASH_CLR|3");
                    PROFILE_EXIT(GROUP_TIMEWARP);
                    bRightFlash = false;
                    bLeftFlash = false;
                }
                else if (gMotionToPhotonW < gMotionToPhotonAccThreshold)
                {
                    uint64_t motoPhoTimeStamp = Svr::GetTimeNano();
                    if (((motoPhoTimeStamp - gMotionToPhotonTimeStamp) * NANOSECONDS_TO_MILLISECONDS) > 500.0f)
                    {
                        //It's been more than 1/2s since the last time we flashed
                        clearColor[0] = clearColor[1] = clearColor[2] = 1.0f;
                        gMotionToPhotonTimeStamp = motoPhoTimeStamp;
                        bRightFlash = true;
                        PROFILE_ENTER(GROUP_TIMEWARP, 0, "MTP_FLASH_CLR|4");
                        PROFILE_EXIT(GROUP_TIMEWARP);
                    }
                }
            }
            else
            {
                float factor = gMotionToPhotonC * gMotionToPhotonW;
                if (factor > 1.0f)
                    factor = 1.0f;

                clearColor[0] = factor;
                clearColor[1] = factor;
                clearColor[2] = factor;
            }

            gMotionToPhotonLast = displayQuat;
        }
#endif // MOTION_TO_PHOTON_TEST

        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Right Eye Submit");

        if (gTimeWarpClearBuffer ||
            gOverrideMeshClearRight ||
            (pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton) ||
            !gDirectModeWarp) {
            GL(glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f));

            switch (gMeshOrderEnum) {
                case kMeshOrderLeftToRight:
                    gDirectModeWarp ? L_SetSurfaceScissor(kRightHalf) : L_StartTiledArea(
                            kRightHalf);
                    break;

                case kMeshOrderRightToLeft:
                    gDirectModeWarp ? L_SetSurfaceScissor(kLeftHalf) : L_StartTiledArea(kLeftHalf);
                    break;

                case kMeshOrderTopToBottom:
                    gDirectModeWarp ? L_SetSurfaceScissor(KBottomHalf) : L_StartTiledArea(
                            KBottomHalf);
                    break;

                case kMeshOrderBottomToTop:
                    gDirectModeWarp ? L_SetSurfaceScissor(kTopHalf) : L_StartTiledArea(kTopHalf);
                    break;
            }

            GL(glClear(GL_COLOR_BUFFER_BIT));
        }

        // Have cleared this side, so reset the flag
        gOverrideMeshClearRight = false;


#ifdef MOTION_TO_PHOTON_TEST
        if (!(pWarpFrame->frameParams.frameOptions & kEnableMotionToPhoton))
        {
#endif // MOTION_TO_PHOTON_TEST

        switch (gMeshOrderEnum) {
            case kMeshOrderLeftToRight:
                L_RenderOneEye(pWarpFrame, kEyeMaskRight, kRightHalf, mesh2A, mesh2A_Sphere,
                               rightTextureMatrix, skewSquashMatrix);
                break;

            case kMeshOrderRightToLeft:
                L_RenderOneEye(pWarpFrame, kEyeMaskLeft, kLeftHalf, mesh2A, mesh2A_Sphere,
                               leftTextureMatrix, skewSquashMatrix);
                break;

            case kMeshOrderTopToBottom:
                L_RenderOneEye(pWarpFrame, kEyeMaskLeft, kLeftHalf, mesh2A, mesh2A_Sphere,
                               leftTextureMatrix, skewSquashMatrix);
                L_RenderOneEye(pWarpFrame, kEyeMaskRight, kRightHalf, mesh2B, mesh2B_Sphere,
                               rightTextureMatrix, skewSquashMatrix);
                break;

            case kMeshOrderBottomToTop:
                L_RenderOneEye(pWarpFrame, kEyeMaskLeft, kLeftHalf, mesh2A, mesh2A_Sphere,
                               leftTextureMatrix, skewSquashMatrix);
                L_RenderOneEye(pWarpFrame, kEyeMaskRight, kRightHalf, mesh2B, mesh2B_Sphere,
                               rightTextureMatrix, skewSquashMatrix);
                break;
        }   // switch (gMeshOrderEnum)

#ifdef MOTION_TO_PHOTON_TEST
        }   // Not motion to photon
#endif // MOTION_TO_PHOTON_TEST

        // As the meshes are moved using gMeshOffset[Left|Right] they can stomp on each other.
        L_SetSurfaceScissor(kEntire);

        //LOGE("TimeToLeftEye = %lld ns; TimeToRightEye = %lld ns", timeToLeftEye, timeToRightEye);
        //LOGE("TimeToLeftEye = %0.2f ms; TimeToRightEye = %0.2f ms", (float)timeToLeftEye / 1000000.0f, (float)timeToRightEye / 1000000.0f);

        PROFILE_EXIT(GROUP_TIMEWARP); //Right Eye Submit

        PROFILE_ENTER(GROUP_TIMEWARP, 0, "Right Eye Finalize");
        if (!gDirectModeWarp) {
            GL(glEndTilingQCOM(GL_COLOR_BUFFER_BIT0_QCOM));
        }

        if (gSingleBufferWindow) {
            GL(glFlush());
            if(rightEyeHasWait){
                NanoSleep(gRightEyeAdjustBias * 1000000);
            }
        } else {
            eglSwapBuffers(display, gAppContext->modeContext->warpRenderSurface);
        }

        PROFILE_EXIT(GROUP_TIMEWARP); //Right Eye Finalize

        PROFILE_EXIT(GROUP_TIMEWARP); // Warp Submission

        PROFILE_EXIT(GROUP_TIMEWARP); //PROFILE_SCOPE_DEFAULT(GROUP_TIMEWARP);
    }   // while(true)

    LOGI("WarpThreadMain while loop exited");

    // Clear the display
    {
        LOGI("Clearing Display Buffer");
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        // LOGI("Viewport: %d, %d, %d, %d", 0, 0, gAppContext->modeContext->warpRenderSurfaceWidth, gAppContext->modeContext->warpRenderSurfaceHeight);
        glViewport(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                   gAppContext->modeContext->warpRenderSurfaceHeight);
        glScissor(0, 0, gAppContext->modeContext->warpRenderSurfaceWidth,
                  gAppContext->modeContext->warpRenderSurfaceHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        eglSwapBuffers(display, gAppContext->modeContext->warpRenderSurface);
    }

    gAppContext->modeContext->eventManager->ReleaseEventQueue(pEventQueue);

    if (!gUseQvrPerfModule) {
        if (gTimeWarpThreadCore >= 0) {
            LOGI("Clearing TimeWarp Affinity");
            svrClearThreadAffinity();
        }

        if (gEnableWarpThreadFifo) {
            L_SetThreadPriority("TimeWarp Thread", SCHED_NORMAL, gNormalPriorityWarp);
        }
    }

    DestroyAsyncWarpData(&gWarpData);

    // Clean up the vulkan texture map
    for (unsigned int whichMap = 0; whichMap < gVulkanMap.mcapacity; whichMap++) {
        if (gVulkanMap.mtable[whichMap] != NULL) {
            vulkanTexture *pOneTexture = &gVulkanMap.mtable[whichMap]->data;
            if (pOneTexture != NULL) {
                glDeleteTextures(1, &pOneTexture->hTexture);
                glDeleteMemoryObjectsKHR(1, &pOneTexture->hMemory);

                memset(pOneTexture, 0, sizeof(vulkanTexture));
            }
        }
    }
    gVulkanMap.Destroy();

#ifdef ENABLE_MOTION_VECTORS
    TerminateMotionVectors();
#endif // ENABLE_MOTION_VECTORS

    for (uint32_t idx = 0; idx < NUM_SWAP_FRAMES; idx++) {
        if (eglSyncList[idx] != EGL_NO_SYNC_KHR) {
            eglDestroySyncKHR(gVkConsumer_eglDisplay, eglSyncList[idx]);
            eglSyncList[idx] = EGL_NO_SYNC_KHR;
        }
    }

    svrDestroyWarpContext();

    LOGI("WarpThreadMain cleaned up.  Exiting");

    return 0;
}

//-----------------------------------------------------------------------------
bool svrBeginTimeWarp()
//-----------------------------------------------------------------------------
{
    LOGI("svrBeginTimeWarp");

    // Vulkan: Need to create a new surface
    // If the application is Vulkan then it has not created the surfaces.
    gAppContext->modeContext->eyeRenderContext = eglGetCurrentContext();
    gAppContext->modeContext->eyeRenderWarpSurface = eglGetCurrentSurface(EGL_DRAW);

    if (gAppContext->modeContext->eyeRenderContext == 0 &&
        gAppContext->modeContext->eyeRenderWarpSurface == 0) {
        // Surfaces have not been created. Need to create them 
        // Later: Creating the context/surface needs to be on the warp thread
        // if (!svrCreateVulkanConsumerContextSurface())
        // {
        //     return false;
        // }
    } else {
        // Surfaces have already been created, take over
        if (!svrUpdateEyeContextSurface()) {
            return false;
        }
    }


    int status = 0;

    // In case we need to change some attributes of the thread
    pthread_attr_t threadAttribs;
    status = pthread_attr_init(&threadAttribs);
    if (status != 0) {
        LOGE("svrBeginTimeWarp: Failed to initialize warp thread attributes");
    }

    // For some reason, under 64-bit, we get this "warning" if we try to set the priority: 
    //  "W libc    : pthread_create sched_setscheduler call failed: Operation not permitted"
    // It comes in as a warning, but the thread is NOT created!
#if defined(__ARM_ARCH_7A__)
    if (gEnableWarpThreadFifo) {
        status = pthread_attr_setschedpolicy(&threadAttribs, SCHED_FIFO);
        if (status != 0) {
            LOGE("svrBeginTimeWarp: Failed to set warp thread attribute: SCHED_FIFO");
        }

        sched_param schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.sched_priority = gFifoPriorityWarp;
        status = pthread_attr_setschedparam(&threadAttribs, &schedParam);
        if (status != 0) {
            LOGE("svrBeginTimeWarp: Failed to set warp thread attribute: Priority");
        }

    }
#endif  // defined(__ARM_ARCH_7A__)

    status = pthread_create(&gAppContext->modeContext->warpThread, &threadAttribs, &WarpThreadMain,
                            NULL);
    if (status != 0) {
        LOGE("svrBeginTimeWarp: Failed to create warp thread");
    }
    pthread_setname_np(gAppContext->modeContext->warpThread, "svrWarp");

    // No longer need the thread attributes
    status = pthread_attr_destroy(&threadAttribs);
    if (status != 0) {
        LOGE("svrBeginTimeWarp: Failed to destroy warp thread attributes");
    }


    //Wait until the warp thread is created and the context has been created before continuing
    LOGI("Waiting for timewarp context creation...");
    pthread_mutex_lock(&gAppContext->modeContext->warpThreadContextMutex);
    while (!gAppContext->modeContext->warpContextCreated) {
        pthread_cond_wait(&gAppContext->modeContext->warpThreadContextCv,
                          &gAppContext->modeContext->warpThreadContextMutex);
    }
    pthread_mutex_unlock(&gAppContext->modeContext->warpThreadContextMutex);

    return true;
}

//-----------------------------------------------------------------------------
bool svrEndTimeWarp()
//-----------------------------------------------------------------------------
{
    LOGI("svrEndTimeWarp");

    // Check global contexts
    if (gAppContext == NULL) {
        LOGE("Unable to end TimeWarp! Application context has been released!");
        return false;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("Unable to end TimeWarp: Called when not in VR mode!");
        return false;
    }

    // Clean up any stencil assets (This is the thread to do it)
    SvrAsyncWarpResources *pWarpData = &gWarpData;
    pWarpData->stencilMeshGeomLeft.Destroy();
    pWarpData->stencilMeshGeomRight.Destroy();
    pWarpData->stencilShader.Destroy();
    pWarpData->stencilArrayShader.Destroy();

    // Clean up the Boundary assets
    for (uint32_t whichMesh = 0; whichMesh < kNumBoundaryMeshes; whichMesh++) {
        pWarpData->BoundaryMeshGeom[whichMesh].Destroy();
    }
    pWarpData->BoundaryMeshShader.Destroy();
    pWarpData->BoundaryMeshArrayShader.Destroy();

    //Stop the warp thread
    gAppContext->modeContext->warpThreadExit = true;
    pthread_join(gAppContext->modeContext->warpThread, NULL);

    return true;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT SvrResult
svrSetWarpMesh(svrWarpMeshEnum
whichMesh,
void *pVertexData,
int vertexSize,
int nVertices,
unsigned int *pIndices,
int nIndices
)
//-----------------------------------------------------------------------------
{
// Which override mesh are we talking about?
unsigned int targetMesh = kMeshLeft_Override;
switch (whichMesh)
{
case kMeshEnumLeft:
targetMesh = kMeshLeft_Override;
break;
case kMeshEnumRight:
targetMesh = kMeshRight_Override;
break;
case kMeshEnumUL:
targetMesh = kMeshUL_Override;
break;
case kMeshEnumUR:
targetMesh = kMeshUR_Override;
break;
case kMeshEnumLL:
targetMesh = kMeshLL_Override;
break;
case kMeshEnumLR:
targetMesh = kMeshLR_Override;
break;

default:
LOGE("svrSetWarpMesh() given data with invalid mesh enumeration:  %d", whichMesh);
return
SVR_ERROR_UNSUPPORTED;
}

// A NULL pointer means to revert back to default TimeWarp mesh
if (pVertexData == NULL || pIndices == NULL)
{
// LOGE("svrSetWarpMesh() given NULL data");
gWarpData.warpMeshes[targetMesh].

Destroy();

return
SVR_ERROR_NONE;
}

// Verify the data size is correct.
int expectedSize = nVertices * sizeof(warpMeshVert);
if (expectedSize != vertexSize)
{
LOGE("svrSetWarpMesh() given data with incorrect vertex data size.  Expected %d, passed in %d", expectedSize, vertexSize);
return
SVR_ERROR_UNSUPPORTED;
}

// If we already have this override, just update the data.
if (gWarpData.warpMeshes[targetMesh].

GetVaoId()

!= 0)
{
// Already created, update the data
if (gWarpData.warpMeshes[targetMesh].

GetVertexCount()

!= nVertices ||
gWarpData.warpMeshes[targetMesh].

GetIndexCount()

!= nIndices)
{
LOGE("Unable to update warp mesh!  New mesh is not the same size as previous mesh!");
}
else
{
gWarpData.warpMeshes[targetMesh].
Update(pVertexData, vertexSize, nVertices
);
// LOGI("DEBUG!    Mesh Updated: Vb = %d; Ib = %d; Vao = %d", gWarpData.warpMeshes[targetMesh].GetVbId(), gWarpData.warpMeshes[targetMesh].GetIbId(), gWarpData.warpMeshes[targetMesh].GetVaoId());
}

}
else
{
// Has not been created.
gWarpData.warpMeshes[targetMesh].
Initialize(&gMeshAttribs[0],
NUM_VERT_ATTRIBS,
pIndices, nIndices,
pVertexData, vertexSize, nVertices);
// LOGI("DEBUG!    Mesh Created: Vb = %d; Ib = %d; Vao = %d", gWarpData.warpMeshes[targetMesh].GetVbId(), gWarpData.warpMeshes[targetMesh].GetIbId(), gWarpData.warpMeshes[targetMesh].GetVaoId());
}

gOverrideMeshClearLeft = true;
gOverrideMeshClearRight = true;

return
SVR_ERROR_NONE;
}
