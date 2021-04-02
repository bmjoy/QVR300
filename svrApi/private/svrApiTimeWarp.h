//=============================================================================
// FILE: svrApiTimeWarp.h
//
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#ifndef _SVR_API_TIMEWARP_H_
#define _SVR_API_TIMEWARP_H_

#include "svrGeometry.h"
#include "svrShader.h"
#include "svrRenderTarget.h"

#include "svrCpuTimer.h"
#include "svrGpuTimer.h"

#include "private/svrApiCore.h"

#include <semaphore.h>  // For sem_t

#ifdef ENABLE_MOTION_VECTORS
#include "MotionEngine.h"
#endif // ENABLE_MOTION_VECTORS

bool svrBeginTimeWarp();
bool svrEndTimeWarp();
void NanoSleep(uint64_t sleepTimeNano);
unsigned int GetVulkanInteropHandle(svrRenderLayer *pRenderLayer);

// Uber Shader Defines.
// The First line MUST be the Version string.
// The Second line MUST be the "#extension" string (If it exists)
// The Third line MUST be the Precision string
// Then come the "#define" strings
#define UBER_VERSION_STRING         "#version 300 es\n"
#define UBER_IMAGE_EXT_STRING       "#extension GL_OES_EGL_image_external_essl3 : require\n"
#define UBER_PRECISION_STRING       "precision highp float;\n"

#define UBER_PROJECT_STRING         "#define PROJECTION_SUPPORT\n"          // kWarpProjected = (1 << 0),
#define UBER_IMAGE_STRING           "#define IMAGE_SUPPORT\n"               // kWarpImage = (1 << 1),
#define UBER_UV_SCALE_STRING        "#define UV_SCALE_SUPPORT\n"            // kWarpUvScaleOffset = (1 << 2),
#define UBER_UV_CLAMP_STRING        "#define UV_CLAMP_SUPPORT\n"            // kWarpUvClamp = (1 << 3),
#define UBER_CHROMA_STRING          "#define CHROMATIC_SUPPORT\n"           // kWarpChroma = (1 << 4),
#define UBER_ARRAY_STRING           "#define ARRAY_SUPPORT\n"               // kWarpArray = (1 << 5),
#define UBER_VIGNETTE_STRING        "#define VIGNETTE_SUPPORT\n"            // kWarpVignette = (1 << 6),
#define UBER_SPHERE_STRING          "#define SUPPORT_SPHERE_MESH\n"         // Sphere mesh for kWarpEquirect and kWarpCubemap
#define UBER_EQR_STRING             "#define SUPPORT_EQUIRECTANGULAR\n"     // kWarpEquirect = (1 << 7),
#define UBER_CUBEMAP_STRING         "#define SUPPORT_CUBEMAP\n"             // kWarpCubemap = (1 << 8),

#ifdef ENABLE_MOTION_VECTORS
#define UBER_SPACEWARP_STRING       "#define SPACEWARP_SUPPORT\n"           // Global. Not hooked to a flag
#define UBER_SPACEWARP_GPU_STRING   "#define SPACEWARP_GPU_SUPPORT\n"       // Global. Not hooked to a flag
#endif // ENABLE_MOTION_VECTORS

// Added in for blit shader
#define UBER_YUV_TARGET_EXT_STRING  "#extension GL_EXT_YUV_target : require\n"
#define UBER_TRANSFROM_UV_STRING    "#define TRANSFORM_UV_SUPPORT\n"
#define UBER_YUV_TARGET_STRING      "#define YUV_TARGET_SUPPORT\n"

enum svrSensorPosition
{
    kSensorLandscapeLeft = 0,
    kSensorLandscapeRight
};

enum svrWarpMeshOrder
{
    kMeshOrderLeftToRight = 0,
    kMeshOrderRightToLeft,
    kMeshOrderTopToBottom,
    kMeshOrderBottomToTop
};

// Due to svrSetWarpMesh some of these meshes are now defined in svrApi.h. 
// CAUTION: Make sure the first set here matches what is in svrWarpMeshEnum
enum svrWarpMeshArea
{
    kMeshLeft = 0,  // Columns Left to Right [-1, 0]
    kMeshRight,     // Columns Left to Right [0, 1]
    kMeshUL,        // Rows Top to Bottom [Upper Left]
    kMeshUR,        // Rows Top to Bottom [Upper Right]
    kMeshLL,        // Rows Top to Bottom [Lower Left]
    kMeshLR,        // Rows Top to Bottom [Lower Right]

    // Override Versions
    kMeshLeft_Override,
    kMeshRight_Override,
    kMeshUL_Override,
    kMeshUR_Override,
    kMeshLL_Override,
    kMeshLR_Override,

    // Low resolution undistorted versions
    kMeshLeft_Low,
    kMeshRight_Low,
    kMeshUL_Low,
    kMeshUR_Low,
    kMeshLL_Low,
    kMeshLR_Low,

    // Inverted Versions
    kMeshLeft_Inv,
    kMeshRight_Inv,
    kMeshUL_Inv,
    kMeshUR_Inv,
    kMeshLL_Inv,
    kMeshLR_Inv,

    // EquiRectangular versions (Sphere Projection)
    kMeshLeft_Sphere,
    kMeshRight_Sphere,
    kMeshUL_Sphere,
    kMeshUR_Sphere,
    kMeshLL_Sphere,
    kMeshLR_Sphere,

    // Overlay Mesh
    kMeshOverlay,

    kNumWarpMeshAreas
};

enum svrWarpShaderFlag
{
    // These are a bit mask index into an array of possible shaders
    kWarpProjected      = (1 << 0),
    kWarpImage          = (1 << 1),
    kWarpUvScaleOffset  = (1 << 2),
    kWarpUvClamp        = (1 << 3),
    kWarpChroma         = (1 << 4),
    kWarpArray          = (1 << 5),
    kWarpVignette       = (1 << 6),
    kWarpEquirect       = (1 << 7),
    kWarpCubemap        = (1 << 8),
};

enum svrBoundaryMesh
{
    kBoundaryMeshPX = 0,    // Side of a cube mesh: +X
    kBoundaryMeshNX,        // Side of a cube mesh: -X

    kBoundaryMeshPY,        // Side of a cube mesh: +Y
    kBoundaryMeshNY,        // Side of a cube mesh: -Y

    kBoundaryMeshPZ,        // Side of a cube mesh: +Z
    kBoundaryMeshNZ,        // Side of a cube mesh: -Z

    kNumBoundaryMeshes
};

struct SvrAsyncWarpResources
{
    Svr::SvrGeometry        warpMeshes[kNumWarpMeshAreas];

    uint32_t                numWarpShaders;
    uint32_t *              pWarpShaderMap;
    Svr::SvrShader *        pWarpShaders;

    Svr::SvrShader          blitShader;
    Svr::SvrShader          blitYuvShader;
    Svr::SvrShader          blitImageShader;
    Svr::SvrShader          blitYuvImageShader;

    Svr::SvrGeometry        stencilMeshGeomLeft;
    Svr::SvrGeometry        stencilMeshGeomRight;
    Svr::SvrShader          stencilShader;
    Svr::SvrShader          stencilArrayShader;

    Svr::SvrGeometry        BoundaryMeshGeom[kNumBoundaryMeshes];
    Svr::SvrShader          BoundaryMeshShader;
    Svr::SvrShader          BoundaryMeshArrayShader;

    Svr::SvrRenderTarget    eyeTarget[kNumEyes];
    Svr::SvrRenderTarget    overlayTarget[kNumEyes];

    unsigned int            eyeSamplerObj;
    unsigned int            overlaySamplerObj;
};

#ifdef ENABLE_MOTION_VECTORS
#ifndef EGL_NEW_IMAGE_QCOM
#define EGL_NEW_IMAGE_QCOM              0x3120
#endif // EGL_NEW_IMAGE_QCOM

#ifndef EGL_IMAGE_FORMAT_QCOM
#define EGL_IMAGE_FORMAT_QCOM           0x3121
#endif // EGL_IMAGE_FORMAT_QCOM

#ifndef EGL_FORMAT_RGBA_8888_QCOM
#define EGL_FORMAT_RGBA_8888_QCOM       0x3122
#endif // EGL_FORMAT_RGBA_8888_QCOM

// The encoder only considers luma, so technically it could use EGL_FORMAT_NV12_QCOM or EGL_FORMAT_NV21_QCOM
#ifndef EGL_FORMAT_NV12_QCOM
#define EGL_FORMAT_NV12_QCOM            0x31C2
#endif // EGL_FORMAT_NV12_QCOM

struct svrMotionInput
{
    // The Motion Engine input buffer
    EGLClientBuffer     inputBuffer;
    GLuint              imageFrameBuffer;
    GLuint              imageTexture;
    EGLImageKHR         eglImage;

    // The head pose used to render this input buffer
    svrHeadPoseState    headPoseState;
};

// WARNING! This count times NUM_DUAL_BUFFERS is number of ME_GetInputBuffer() calls made.
// We know there is a limit of 5 so be careful changing these values!
#define NUM_DUAL_BUFFERS            2   // DUAL == 2 is obvious, but need loop variable and magic number protection!
enum svrMotionInputType
{
    kMotionTypeStatic = 0,
    kMotionTypeWarped,
    kNumMotionTypes
};

// This only needs to be a maximum of one thread for each eye
#define NUM_MOTION_VECTOR_BUFFERS   2   // If this is 5 then ME_GetInputBuffer() hangs and never comes back!
struct SvrMotionData
{
    // What is the index in the entire list
    uint32_t            listIndex;

    // Handle to MotionEngine
    ME_Handle           hInstance;

    // State Variables
    bool                waitingForCallback;
    bool                dataIsNew;
    Svr::SvrCpuTimer    timer;

    // Motion Vectors
    unsigned int        meshWidth;
    unsigned int        meshHeight;
    unsigned int        motionVectorSize;
    ME_MotionVector     *motionVectors;
    ME_MotionVector     *smoothedVectors;

    // Image Buffer Input. There are two of these, old and new.
    int                 imageWidth;
    int                 imageHeight;
    int                 newBuffer;
    int                 oldBuffer;
    svrMotionInput      dualBuffers[NUM_DUAL_BUFFERS][kNumMotionTypes];

    // Same as dualBuffers, but only used if gRenderMotionInput is true
    svrMotionInput      debugBuffers[NUM_DUAL_BUFFERS][kNumMotionTypes];

    // Final Results Texture
    GLuint              resultTexture;

    // Thread control
    pthread_t           hThread;
    pid_t	            threadId;
    bool                threadShouldExit;
    bool                threadClosed;
    sem_t               semAvailable;
    sem_t               semDoWork;
    sem_t               semDone;

    // Surface and Context
    EGLint              surfaceWidth;
    EGLint              surfaceHeight;
    EGLDisplay          eglDisplay;
    EGLSurface          eglSurface;
    EGLContext          eglContext;

    Svr::SvrShader      blitYuvShader;
    Svr::SvrShader      blitYuvArrayShader;
    Svr::SvrShader      blitYuvImageShader;
    Svr::SvrShader      blitYuvWarpShader;
    Svr::SvrShader      blitYuvWarpArrayShader;
    Svr::SvrShader      blitYuvWarpImageShader;

    // Warp Frame
    Svr::svrFrameParamsInternal* pWarpFrame;

    // Which eye should we look at?
    svrEyeMask          eyeMask;
};

// Motion Vector Functions
void InitializeMotionVectors();
void TerminateMotionVectors();
void ProcessMotionData();
void GenerateMotionData(Svr::svrFrameParamsInternal* pWarpFrame);

#endif // ENABLE_MOTION_VECTORS

#endif //_SVR_API_TIMEWARP_H_