//=============================================================================
// FILE: svrApiCore.cpp
//
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <jni.h>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "svrApi.h"
#include "svrCpuTimer.h"
#include "svrProfile.h"
#include "svrRenderExt.h"
#include "svrUtil.h"

#include "svrConfig.h"

#include "private/svrApiCore.h"
#include "private/svrApiDebugServer.h"
#include "private/svrApiHelper.h"
#include "private/svrApiPredictiveSensor.h"
#include "private/svrApiTimeWarp.h"

#include "private/Invision/inc/IvApiCore.h"

#include <fstream>
#include <sstream>
#include <string>
#include <set>

// willie change start 2020-8-20
#include <sys/stat.h>
#include <SCMediatorHelper.h>
// willie change end 2020-8-20

#define QVRSERVICE_SDK_CONFIG_FILE  "sdk-config-file"

using namespace Svr;

#include <LayerFetcher.h>

using namespace UnityLauncher;

// Surface Properties
VAR(int, gEyeBufferWidth, 1024,
    kVariableNonpersistent);            //Value returned as recommended eye buffer width in svrDeviceInfo
VAR(int, gEyeBufferHeight, 1024,
    kVariableNonpersistent);           //Value returned as recommended eye buffer height in svrDeviceInfo
VAR(float, gEyeBufferFovX, 90.0f,
    kVariableNonpersistent);          //Value returned as recommended FOV X in svrDeviceInfo
VAR(float, gEyeBufferFovY, 90.0f,
    kVariableNonpersistent);          //Value returned as recommended FOV Y in svrDeviceInfo

// Projection Properties
VAR(float, gFrustum_Convergence, 0.0f,
    kVariableNonpersistent);     //Value returned as recommended eye convergence in svrDeviceInfo
VAR(float, gFrustum_Pitch, 0.0f,
    kVariableNonpersistent);           //Value returned as recommended eye pitch in svrDeviceInfo

VAR(float, gLeftFrustum_Near, 0.0508,
    kVariableNonpersistent);      //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gLeftFrustum_Far, 100.0,
    kVariableNonpersistent);        //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gLeftFrustum_Left, -0.031,
    kVariableNonpersistent);      //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gLeftFrustum_Right, 0.031,
    kVariableNonpersistent);      //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gLeftFrustum_Top, 0.031,
    kVariableNonpersistent);        //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gLeftFrustum_Bottom, -0.031,
    kVariableNonpersistent);    //Value returned as recommended view frustum size in svrDeviceInfo

VAR(float, gRightFrustum_Near, 0.0508,
    kVariableNonpersistent);     //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gRightFrustum_Far, 100.0,
    kVariableNonpersistent);       //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gRightFrustum_Left, -0.031,
    kVariableNonpersistent);     //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gRightFrustum_Right, 0.031,
    kVariableNonpersistent);     //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gRightFrustum_Top, 0.031,
    kVariableNonpersistent);       //Value returned as recommended view frustum size in svrDeviceInfo
VAR(float, gRightFrustum_Bottom, -0.031,
    kVariableNonpersistent);   //Value returned as recommended view frustum size in svrDeviceInfo

// TimeWarp Properties
VAR(bool, gEnableTimeWarp, true, kVariableNonpersistent);           //Override to disable TimeWarp
VAR(bool, gDisableReprojection, false,
    kVariableNonpersistent);     //Override to disable reprojection
VAR(bool, gDisablePredictedTime, false,
    kVariableNonpersistent);    //Forces svrGetPredictedDisplayTime to return 0.0
VAR(int, gRenderThreadCore, 3,
    kVariableNonpersistent);            //Core id to set render thread affinity for (-1 disables affinity), ignored if the QVR Perf module is active.
VAR(bool, gEnableRenderThreadFifo, false,
    kVariableNonpersistent);  //Enable/disable setting SCHED_FIFO scheduling policy on the render thread thread, ignored if the QVR Perf module is active.
VAR(int, gForceMinVsync, 0,
    kVariableNonpersistent);               //Override for svrFrameParams minVsync option (0:override disabled, 1 or 2 forced value)
VAR(float, gGpuBusyCheckPeriod, 3.0, kVariableNonpersistent);
VAR(float, gGpuBusyThreshold1, 60, kVariableNonpersistent);
VAR(float, gGpuBusyThreshold2, 80, kVariableNonpersistent);

VAR(bool, gUseLinePtr, true,
    kVariableNonpersistent);               //Override for using the linePtr interrupt, if set to false Choreographer will be used instead

// Display Setting
VAR(float, gTimeToHalfExposure, 8.33f,
    kVariableNonpersistent);     // Time (milliseconds) to get to Half Exposure on the display. This usually T/2, but this property can be used to adjust for OLED vs LCD, getting to 3T/4, additional display delay time, etc.
VAR(float, gTimeToMidEyeWarp, 4.16f,
    kVariableNonpersistent);       // Time (milliseconds) between warping each eye.
VAR(float, gExtraAdjustPeriodRatio, 1.0f, kVariableNonpersistent);

// Heuristic Predicted Time
VAR(bool, gHeuristicPredictedTime, false,
    kVariableNonpersistent);  // Whether to use a heuristic predicted time
VAR(int, gNumHeuristicEntries, 25,
    kVariableNonpersistent);         // How many entries to average to get heuristic predicted time
VAR(float, gHeuristicOffset, 0.0,
    kVariableNonpersistent);         // Offset added to the heuristic predicted time

//Power brackets for performance levels
VAR(bool, gUseQvrPerfModule, true,
    kVariableNonpersistent);            //Enable/disable the QVR Performance module.  If active all thread affinities, priorities and HW clocks will be based on the QVR perf module configuration rather than the SDK configuration

VAR(int, gForceCpuLevel, -1,
    kVariableNonpersistent);               //Override to force CPU performance level (-1: app defined, 0:system defined/off, 1/2/3 for min,medium,max)
VAR(int, gCpuLvl1Min, 30,
    kVariableNonpersistent);                  //Lower CPU frequency (percentage) bound for min performance level, ignored if the QVR Perf module is active.
VAR(int, gCpuLvl1Max, 50,
    kVariableNonpersistent);                  //Upper CPU frequency (percentage) bound for min performance level, ignored if the QVR Perf module is active.
VAR(int, gCpuLvl2Min, 51,
    kVariableNonpersistent);                  //Lower CPU frequency (percentage) bound for medium performance level, ignored if the QVR Perf module is active.
VAR(int, gCpuLvl2Max, 80,
    kVariableNonpersistent);                  //Upper CPU frequency (percentage) bound for medium performance level, ignored if the QVR Perf module is active.
VAR(int, gCpuLvl3Min, 81,
    kVariableNonpersistent);                  //Lower CPU frequency (percentage) bound for max performance level, ignored if the QVR Perf module is active.
VAR(int, gCpuLvl3Max, 100,
    kVariableNonpersistent);                 //Upper CPU frequency (percentage) bound for max performance level  , ignored if the QVR Perf module is active.

VAR(int, gForceGpuLevel, -1,
    kVariableNonpersistent);               //Override to force GPU performance level (-1: app defined, 0:system defined/off, 1/2/3 for min,medium,max)
VAR(int, gGpuLvl1Min, 30,
    kVariableNonpersistent);                  //Lower GPU frequency (percentage) bound for min performance level, ignored if the QVR Perf module is active.
VAR(int, gGpuLvl1Max, 50,
    kVariableNonpersistent);                  //Upper GPU frequency (percentage) bound for min performance level, ignored if the QVR Perf module is active.
VAR(int, gGpuLvl2Min, 51,
    kVariableNonpersistent);                  //Lower GPU frequency (percentage) bound for medium performance level, ignored if the QVR Perf module is active.
VAR(int, gGpuLvl2Max, 80,
    kVariableNonpersistent);                  //Upper GPU frequency (percentage) bound for medium performance level, ignored if the QVR Perf module is active.
VAR(int, gGpuLvl3Min, 81,
    kVariableNonpersistent);                  //Lower GPU frequency (percentage) bound for max performance level, ignored if the QVR Perf module is active.
VAR(int, gGpuLvl3Max, 100,
    kVariableNonpersistent);                 //Upper GPU frequency (percentage) bound for max performance level, ignored if the QVR Perf module is active.

//Tracking overrides
VAR(int, gForceTrackingMode, 0,
    kVariableNonpersistent);            //Force a specific tracking mode 1 = rotational 3 = rotational & positional
VAR(bool, gUseMagneticRotationFlag, true,
    kVariableNonpersistent);  //If using roational data only, use version that is magnetically corrected


//Log options
VAR(float, gLogLinePtrDelay, 0.0,
    kVariableNonpersistent);          //Log line ptr delays longer greater than this value (0.0 = disabled)
VAR(bool, gLogSubmitFps, false,
    kVariableNonpersistent);            //Enables output of submit FPS to LogCat

VAR(bool, gLogPoseVelocity, false, kVariableNonpersistent);         //Log out the tracking velocity
VAR(float, gMaxAngVel, 450.0f,
    kVariableNonpersistent);             //Detected angular velocity larger than this value will be considered an error in the tracking system (degrees / sec)
VAR(float, gMaxLinearVel, 6.0f,
    kVariableNonpersistent);            //Detected linear/translational velocity larger than this value will be considered an error in the tracking system (meters / sec)

VAR(bool, gLogSubmitFrame, false,
    kVariableNonpersistent);          //Log svrSubmitFrame() parameters

//Debug Server options
VAR(bool, gEnableDebugServer, false,
    kVariableNonpersistent);       //Enables a very basic json-rpc server for interacting with vr while running

//Debug toggles
VAR(bool, gDisableFrameSubmit, false,
    kVariableNonpersistent);      //Debug flag that will prevent the eye buffer render thread from submitted frames to time warp

//Controller options
VAR(char*, gControllerService, " ", kVariableNonpersistent);
VAR(int, gControllerRingBufferSize, 80, kVariableNonpersistent);

//Temporary eye calibration
VAR(float, gSensorEyeOffsetX, 0, kVariableNonpersistent);
VAR(float, gSensorEyeOffsetY, 0, kVariableNonpersistent);
VAR(float, gSensorEyeScaleX, 1, kVariableNonpersistent);
VAR(float, gSensorEyeScaleY, 1, kVariableNonpersistent);
VAR(bool, gUseFoveatWithoutEyeTracking, false, kVariableNonpersistent);

VAR(int, gPredictVersion, 3, kVariableNonpersistent);

VAR(float, gDeflection, 0, kVariableNonpersistent);

VAR(bool, gUseQVRCamera, false, kVariableNonpersistent);  //Whether using QVR Camera or not

VAR(bool, gUseSCCamera, false, kVariableNonpersistent);        //Whether using SC Camera or not

VAR(bool, gDebugWithProperty, false, kVariableNonpersistent);

EXTERN_VAR(int, gWarpMeshType);
EXTERN_VAR(bool, gEnableWarpThreadFifo);
EXTERN_VAR(bool, gLogRawSensorData);
EXTERN_VAR(bool, gLogVSyncData);
EXTERN_VAR(bool, gLogPrediction);

EXTERN_VAR(float, gMaxPredictedTime);
EXTERN_VAR(bool, gLogMaxPredictedTime);

#ifdef ENABLE_MOTION_VECTORS
EXTERN_VAR(bool, gEnableMotionVectors);
EXTERN_VAR(bool, gForceAppEnableMotionVectors);
#endif // ENABLE_MOTION_VECTORS

EXTERN_VAR(float, gSensorOrientationCorrectX)
EXTERN_VAR(float, gSensorOrientationCorrectY)
EXTERN_VAR(float, gSensorOrientationCorrectZ)
EXTERN_VAR(float, gSensorHeadOffsetX)
EXTERN_VAR(float, gSensorHeadOffsetY)
EXTERN_VAR(float, gSensorHeadOffsetZ)


int gFifoPriorityRender = 96;
int gNormalPriorityRender = 0;

int gRecenterTransition = 1000;

// Offset from the time Android has to what QVR has
int64_t gQTimeToAndroidBoot = 0LL;

// Want to timeline things
uint64_t gVrStartTimeNano = 0;

// Variables needed for heuristic predicted time
float *gpHeuristicPredictData = NULL;
int gHeuristicWriteIndx = 0;

std::ifstream gGpuBusyFStream;
uint64_t gGpuBusyLastCheckTimestamp = 0;
float gMaxGpuBusyRate = 0;
int gCurrMinVSync = 0;
const std::string QCOM_GPU_BUSY_FILE_PATH = "/sys/class/kgsl/kgsl-3d0/gpubusy";

// Need this common file from svrApiTimeWarp.cpp
double L_MilliSecondsToNextVSync();

float L_MilliSecondsSinceVrStart();

// willie change start 2020-8-20
SCMediatorHelper *gMediatorHelper = nullptr;
// willie change end 2020-8-20

float mPanelInfo[68 * 20] = {
        0}; // max 20 panel, one panel struct (id nPoints 3xmaxpoints(20) createPoint normal)
int mPanelSize = 0; // max 20 panel
namespace Svr {
    SvrAppContext *gAppContext = NULL;
    constexpr int CAMERA_BUFFER_NUM = 2;
    int gCameraFrameIndexArray[CAMERA_BUFFER_NUM] = {-1};
    CameraFrameData gCameraFrameDataArray[CAMERA_BUFFER_NUM];
    int gCameraFrameIndex = 0;
}
struct LayerVertex {
    float vertexPosition[12];
    float vertexUV[8];
};

LayerFetcher *mLayerFetcher = nullptr;
std::vector<LayerData> mLayerDataVector;
std::vector<LayerVertex> mLayerVertexVector;
std::vector<uint> mLayerTextureIdVector;
glm::mat4 mViewMatrix{1};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

extern "C"
{
//add zengchuiguo 20200724 (start)
static GESTURE_CHANGE_CALLBACK mGestureChangeCB = nullptr;
static void destroyIvGesture();
//add zengchuiguo 20200724 (end)


void Java_com_qualcomm_svrapi_SvrApi_nativeVsync(JNIEnv *jni, jclass clazz, jlong frameTimeNanos) {
    if (gAppContext != NULL && gAppContext->modeContext != NULL) {
        const double periodNano = 1e9 / gAppContext->deviceInfo.displayRefreshRateHz;

        pthread_mutex_lock(&gAppContext->modeContext->vsyncMutex);

        uint64_t vsyncTimeStamp = frameTimeNanos;

        if (gAppContext->modeContext->vsyncTimeNano == 0) {
            //Don't count the first time through
            gAppContext->modeContext->vsyncTimeNano = vsyncTimeStamp;
            gAppContext->modeContext->vsyncCount = 1;
        } else {
            unsigned int nVsync = floor(0.5 + ((double) (vsyncTimeStamp -
                                                         gAppContext->modeContext->vsyncTimeNano) /
                                               periodNano));
            gAppContext->modeContext->vsyncCount += nVsync;
            gAppContext->modeContext->vsyncTimeNano = vsyncTimeStamp;
        }

        pthread_mutex_unlock(&gAppContext->modeContext->vsyncMutex);

    }   // gAppContext != NULL
}
}

//-----------------------------------------------------------------------------
void L_LogQvrError(const char *pSource, int32_t qvrReturn)
//-----------------------------------------------------------------------------
{
    switch (qvrReturn) {
        case QVR_ERROR:
            LOGE("Error from %s: QVR_ERROR", pSource);
            break;
        case QVR_CALLBACK_NOT_SUPPORTED:
            LOGE("Error from %s: QVR_CALLBACK_NOT_SUPPORTED", pSource);
            break;
        case QVR_API_NOT_SUPPORTED:
            LOGE("Error from %s: QVR_API_NOT_SUPPORTED", pSource);
            break;
        case QVR_INVALID_PARAM:
            LOGE("Error from %s: QVR_INVALID_PARAM", pSource);
            break;
        default:
            LOGE("Error from %s: Unknown = %d", pSource, qvrReturn);
            break;
    }
}

//-----------------------------------------------------------------------------
static inline svrThermalLevel QvrThermalToSvrThermal(uint32_t qvrThermal)
//-----------------------------------------------------------------------------
{
    switch (qvrThermal) {
        case TEMP_SAFE:
            return kSafe;
        case TEMP_LEVEL_1:
            return kLevel1;
        case TEMP_LEVEL_2:
            return kLevel2;
        case TEMP_LEVEL_3:
            return kLevel3;
        case TEMP_CRITICAL:
            return kCritical;
        default:
            return kSafe;
    }
}

//-----------------------------------------------------------------------------
static inline QVRSERVICE_PERF_LEVEL SvrPerfLevelToQvrPerfLevel(svrPerfLevel svrLevel)
//-----------------------------------------------------------------------------
{
    switch (svrLevel) {
        case kPerfSystem:
            return PERF_LEVEL_DEFAULT;
        case kPerfMinimum:
            return PERF_LEVEL_1;
        case kPerfMedium:
            return PERF_LEVEL_2;
        case kPerfMaximum:
            return PERF_LEVEL_3;
        default:
            return PERF_LEVEL_DEFAULT;
    }
}

//-----------------------------------------------------------------------------
static void
qvrClientStatusCallback(void *pCtx, QVRSERVICE_CLIENT_STATUS status, uint32_t arg1, uint32_t arg2)
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL || gAppContext->modeContext == NULL) {
        return;
    }

    if (gAppContext->modeContext->eventManager != NULL) {
        svrEventQueue *pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
        if (pQueue != NULL) {
            svrEventData eventData;

            switch (status) {
                case STATUS_DISCONNECTED:
                    break;
                case STATUS_STATE_CHANGED:
                    //uint32_t newState = arg1;
                    //uint32_t prevState = arg2;
                    break;
                case STATUS_SENSOR_ERROR:
                    pQueue->SubmitEvent(kEventSensorError, eventData);
                    break;
                default:
                    break;
            }
        } else {
            LOGE("qvrClientStatusCallback, failed to acquire event queue");
        }
    }
}

//-----------------------------------------------------------------------------
void qvrClientThermalNotificationCallback(void *pCtx, QVRSERVICE_CLIENT_NOTIFICATION notification,
                                          void *payload, uint32_t payload_length)
//-----------------------------------------------------------------------------
{
    if (payload_length != sizeof(qvrservice_therm_notify_payload_t)) {
        LOGE("ThermalNotifcation payload length %d doesn't match expected payload length of %d",
             payload_length, (int) sizeof(qvrservice_therm_notify_payload_t));
        return;
    }

    if (gAppContext == NULL || gAppContext->modeContext == NULL) {
        return;
    }

    if (gAppContext->modeContext->eventManager != NULL) {
        svrEventData eventData;

        svrEventQueue *pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
        if (pQueue != NULL) {
            qvrservice_therm_notify_payload_t *pThermalPayload = (qvrservice_therm_notify_payload_t *) payload;
            switch (pThermalPayload->hw_type) {
                case HW_TYPE_CPU:
                    eventData.thermal.zone = kCpu;
                    break;
                case HW_TYPE_GPU:
                    eventData.thermal.zone = kGpu;
                    break;
                case HW_TYPE_SKIN:
                    eventData.thermal.zone = kSkin;
                    break;
                default:
                    LOGE("qvrClientThermalNotificationCallback: Unknown zone %d",
                         pThermalPayload->hw_type);
                    eventData.thermal.zone = kNumThermalZones;
            }

            switch (pThermalPayload->temp_level) {
                case TEMP_SAFE:
                    eventData.thermal.level = kSafe;
                    break;
                case TEMP_LEVEL_1:
                    eventData.thermal.level = kLevel1;
                    break;
                case TEMP_LEVEL_2:
                    eventData.thermal.level = kLevel2;
                    break;
                case TEMP_LEVEL_3:
                    eventData.thermal.level = kLevel3;
                    break;
                case TEMP_CRITICAL:
                    eventData.thermal.level = kCritical;
                    break;
            }

            pQueue->SubmitEvent(kEventThermal, eventData);
        }
    }
}


//-----------------------------------------------------------------------------
void svrNotifyFailedQvrService()
//-----------------------------------------------------------------------------
{
    // Find the method ...	
    JNIEnv *pThreadJEnv;
    if (gAppContext->javaVm->AttachCurrentThread(&pThreadJEnv, NULL) != JNI_OK) {
        LOGE("svrInitialize AttachCurrentThread failed.");
    }

    jclass ParentClass = gAppContext->javaSvrApiClass;
    jmethodID MethodId = pThreadJEnv->GetStaticMethodID(ParentClass, "NotifyNoVr",
                                                        "(Landroid/app/Activity;)V");
    if (MethodId == NULL) {
        LOGE("Unable to find Method: SvrNativeActivity::NotifyNoVr()");
        return;
    }

    // ... call the method
    LOGI("Method Called: SvrNativeActivity::NotifyNoVr()...");
    pThreadJEnv->CallStaticVoidMethod(ParentClass, MethodId, gAppContext->javaActivityObject);
}

//-----------------------------------------------------------------------------
int svrGetAndroidOSVersion()
//-----------------------------------------------------------------------------
{
    char fileBuf[256];

    FILE *pFile = popen("getprop ro.build.version.sdk", "r");
    if (!pFile) {
        LOGE("Failed to getprop on 'ro.build.version.sdk'");
        return -1;
    }
    fgets(fileBuf, 256, pFile);

    int version;
    sscanf(fileBuf, "%d", &version);
    pclose(pFile);

    return version;
}

//-----------------------------------------------------------------------------
SvrResult svrSetPerformanceLevelsInternal(svrPerfLevel cpuPerfLevel, svrPerfLevel gpuPerfLevel)
//-----------------------------------------------------------------------------
{
    if (!gAppContext) {
        LOGE("QVRServiceClient unavailable, svrSetPerformanceLevels failed.");
        return SVR_ERROR_QVR_SERVICE_UNAVAILABLE;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper) {
            if (gAppContext->modeContext) {
                if (gAppContext->modeContext->renderThreadId > 0) {
                    IvSlamClient_SetThreadAttributesByType(gAppContext->ivslamHelper,
                                                           gAppContext->modeContext->renderThreadId,
                                                           IVSLAM_THREAD_TYPE::IV_RENDER);
                }
                if (gAppContext->modeContext->warpThreadId > 0) {
                    IvSlamClient_SetThreadAttributesByType(gAppContext->ivslamHelper,
                                                           gAppContext->modeContext->warpThreadId,
                                                           IVSLAM_THREAD_TYPE::IV_WARP);
                }
            }
        }
        //LOGE("%s, the API was not implemented",__FUNCTION__);
        return SVR_ERROR_NONE;
    } else {
        if (gAppContext->qvrHelper) {
            qvrservice_perf_level_t qvrLevels[2];

            qvrLevels[0].hw_type = HW_TYPE_CPU;
            qvrLevels[0].perf_level = SvrPerfLevelToQvrPerfLevel(cpuPerfLevel);

            qvrLevels[1].hw_type = HW_TYPE_GPU;
            qvrLevels[1].perf_level = SvrPerfLevelToQvrPerfLevel(gpuPerfLevel);

            LOGI("Attempting to set perf to (%d, %d)", qvrLevels[0].perf_level,
                 qvrLevels[1].perf_level);
            int res = QVRServiceClient_SetOperatingLevel(gAppContext->qvrHelper, &qvrLevels[0], 2,
                                                         NULL,
                                                         NULL);
            if (res != QVR_SUCCESS) {
                L_LogQvrError("QVRServiceClient_SetOperatingLevel", res);
            } else {
                LOGI("svrSetPerformanceLevels CPU Level : %d",
                     (unsigned int) qvrLevels[0].perf_level);
                LOGI("svrSetPerformanceLevels GPU Level : %d",
                     (unsigned int) qvrLevels[1].perf_level);
            }

            if (gAppContext->modeContext) {
                int32_t qRes;

                if (gAppContext->modeContext->renderThreadId > 0) {
                    QVRSERVICE_THREAD_TYPE threadType = gEnableRenderThreadFifo ? RENDER : NORMAL;
                    qRes = QVRServiceClient_SetThreadAttributesByType(gAppContext->qvrHelper,
                                                                      gAppContext->modeContext->renderThreadId,
                                                                      threadType);
                    LOGI("Calling SetThreadAttributesByType for RENDER thread..., type = %s, ret = %d",(gEnableRenderThreadFifo ? "RENDER" : "NORMAL"),qRes);
                    if (qRes != QVR_SUCCESS) {
                        L_LogQvrError("QVRServiceClient_SetThreadAttributesByType", qRes);
                    }
                }
                if (gAppContext->modeContext->warpThreadId > 0) {
                    QVRSERVICE_THREAD_TYPE threadType = gEnableWarpThreadFifo ? WARP : NORMAL;
                    qRes = QVRServiceClient_SetThreadAttributesByType(gAppContext->qvrHelper,
                                                                      gAppContext->modeContext->warpThreadId,
                                                                      threadType);
                    LOGI("Calling SetThreadAttributesByType for WARP thread..., type = %s, ret = %d",(gEnableWarpThreadFifo ? "WARP" : "NORMAL"),qRes);
                    if (qRes != QVR_SUCCESS) {
                        L_LogQvrError("QVRServiceClient_SetThreadAttributesByType", qRes);
                    }
                }
            }
        } else {
            LOGE("QVRServiceClient unavailable, svrSetPerformanceLevels failed.");
            return SVR_ERROR_QVR_SERVICE_UNAVAILABLE;
        }
    }

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
void L_SetThreadPriority(const char *pName, int policy, int priority)
//-----------------------------------------------------------------------------
{

    // What is the current thread policy?
    int oldPolicy = sched_getscheduler(gettid());
    switch (oldPolicy) {
        case SCHED_NORMAL:      // 0
            LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_NORMAL", pName, (int) gettid(),
                 (int) gettid());
            break;

        case SCHED_FIFO:        // 1
            LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_FIFO", pName, (int) gettid(),
                 (int) gettid());
            break;

        case SCHED_FIFO | SCHED_RESET_ON_FORK:        // 1
            LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_FIFO | SCHED_RESET_ON_FORK",
                 pName, (int) gettid(), (int) gettid());
            break;

        case SCHED_RR:          // 2
            LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_RR", pName, (int) gettid(),
                 (int) gettid());
            break;

        case SCHED_BATCH:       // 3
            LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_BATCH", pName, (int) gettid(),
                 (int) gettid());
            break;

            // case SCHED_ISO:         // 4
            //     LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_ISO", pName, (int)gettid(), (int)gettid());
            //     break;

        case SCHED_IDLE:        // 5
            LOGI("Current %s (0x%x = %d) Scheduling policy: SCHED_IDLE", pName, (int) gettid(),
                 (int) gettid());
            break;

        default:
            LOGI("Current %s (0x%x = %d) Scheduling policy: %d", pName, (int) gettid(),
                 (int) gettid(), oldPolicy);
            break;
    }

    // Where is it going?
    switch (policy) {
        case SCHED_NORMAL:      // 0
            LOGI("    Setting => SCHED_NORMAL");
            break;

        case SCHED_FIFO:        // 1
            LOGI("    Setting => SCHED_FIFO");
            break;

        case SCHED_FIFO | SCHED_RESET_ON_FORK:        // 1
            LOGI("    Setting => SCHED_FIFO | SCHED_RESET_ON_FORK");
            break;

        case SCHED_RR:          // 2
            LOGI("    Setting => SCHED_RR");
            break;

        case SCHED_BATCH:       // 3
            LOGI("    Setting => SCHED_BATCH");
            break;

            // case SCHED_ISO:         // 4
            //     LOGI("    Setting => SCHED_ISO");
            //     break;

        case SCHED_IDLE:        // 5
            LOGI("    Setting => SCHED_IDLE");
            break;

        default:
            LOGI("    Setting => UNKNOWN! (%d)", policy);
            break;
    }

    if (gAppContext->mUseIvSlam) {
        //TODO:Add set thread priority API for IvSlam?
    } else {
        int qRes = QVRServiceClient_SetThreadPriority(gAppContext->qvrHelper, gettid(), policy,
                                                      priority);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_SetThreadPriority", qRes);
        }
    }

    // What was the result?
    int newPolicy = sched_getscheduler(gettid());
    switch (newPolicy) {
        case SCHED_NORMAL:      // 0
            LOGI("    Result => SCHED_NORMAL");
            break;

        case SCHED_FIFO:        // 1
            LOGI("    Result => SCHED_FIFO");
            break;

        case SCHED_FIFO | SCHED_RESET_ON_FORK:        // 1
            LOGI("    Result => SCHED_FIFO | SCHED_RESET_ON_FORK");
            break;

        case SCHED_RR:          // 2
            LOGI("    Result => SCHED_RR");
            break;

        case SCHED_BATCH:       // 3
            LOGI("    Result => SCHED_BATCH");
            break;

            // case SCHED_ISO:         // 4
            //     LOGI("    Result => SCHED_ISO");
            //     break;

        case SCHED_IDLE:        // 5
            LOGI("    Result => SCHED_IDLE");
            break;

        default:
            LOGI("    Result => UNKNOWN! (%d)", newPolicy);
            break;
    }
}

void fetchingQVRCameraFrame() {
    LOGI("fetchingQVRCameraFrame start");
    bool bCameraOpen = false;
    QVRCAMERA_CAMERA_STATUS cameraStatus = QVRCAMERA_CAMERA_ERROR;
    int currQVRFrameIndex = 0;
    qvrcamera_frame_t currQVRFrame;
    std::chrono::high_resolution_clock::time_point lastFetchNano;
    constexpr long INTERVAL_NANO = 1e9 / 60;
    int res = 0;
    bool bGoodFrame = true;
    while (!gAppContext->qvrCameraTerminalSignal.isSignaled()) {
        if (!gAppContext->qvrCameraStartSignal.isSignaled()) {
            LOGI("fetchingQVRCameraFrame before sleep");
            gAppContext->qvrCameraStartSignal.waitSignal(Vera::VeraSignal::SIGNAL_TIMEOUT_INFINITE);
            LOGI("fetchingQVRCameraFrame after sleep");

            if(gAppContext->qvrCameraTerminalSignal.isSignaled()){
                break;
            }
        }

        auto startNano = std::chrono::high_resolution_clock::now();
        auto deltaNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                startNano - lastFetchNano).count();
        if (deltaNanos > INTERVAL_NANO) {
            res = QVRCameraDevice_GetCurrentFrameNumber(gAppContext->qvrDeviceClient,
                                                        &currQVRFrameIndex);
            bGoodFrame = (res == QVR_CAM_SUCCESS) && (currQVRFrameIndex >= 0);
            if (bGoodFrame) {
                bGoodFrame = gAppContext->newCameraBufferIdx < 0 ||
                             (gAppContext->newCameraBufferIdx >= 0 &&
                              gCameraFrameIndexArray[gAppContext->newCameraBufferIdx] !=
                              currQVRFrameIndex);
                if (bGoodFrame) {
                    res = QVRCameraDevice_GetFrame(gAppContext->qvrDeviceClient,
                                                   &currQVRFrameIndex,
                                                   QVRCAMERA_MODE_BLOCKING,
                                                   QVRCAMERA_MODE_NEWER_IF_AVAILABLE,
                                                   &currQVRFrame);
                    bGoodFrame = res == QVR_CAM_SUCCESS;
                }
            }
            if (bGoodFrame) {
                gAppContext->newCameraBufferIdx =
                        (gAppContext->newCameraBufferIdx + 1) % CAMERA_BUFFER_NUM;
                memcpy(gCameraFrameDataArray[gAppContext->newCameraBufferIdx].dataArray,
                       currQVRFrame.buffer,
                       currQVRFrame.len);
                gCameraFrameDataArray[gAppContext->newCameraBufferIdx].frameIndex = currQVRFrame.fn;
                gCameraFrameDataArray[gAppContext->newCameraBufferIdx].exposureNano = currQVRFrame.start_of_exposure_ts;
                gCameraFrameIndexArray[gAppContext->newCameraBufferIdx] = currQVRFrameIndex;
                {
                    std::unique_lock<std::mutex> autoLock(gAppContext->cameraMutex);
                    gAppContext->availableCameraBufferIdx = gAppContext->newCameraBufferIdx;
                }
                QVRCameraDevice_ReleaseFrame(gAppContext->qvrDeviceClient,
                                             currQVRFrameIndex);
            } else {
                Vera::CpuTimer::getInstance()->sleep(1e6);
            }
            lastFetchNano = startNano;
        }
    }
    LOGI("fetchingQVRCameraFrame out of loop");
}


SvrResult svrInitQVRCamera() {
    LOGI("svrInitQVRCamera start gUseQVRCamera=%d", gUseQVRCamera);
    if (!gUseQVRCamera) {
        LOGE("svrInitQVRCamera return directly for gUseQVRCamera=false");
        return SVR_ERROR_NONE;
    }
    if (nullptr == gAppContext->qvrCameraClient) {
        gAppContext->qvrCameraClient = QVRCameraClient_Create();
        if (nullptr == gAppContext->qvrCameraClient) {
            LOGE("svrInitQVRCamera Failed for QVRCameraClient_Create");
            return SVR_ERROR_UNKNOWN;
        }
    }
    gAppContext->qvrCameraApiVersion = QVRCameraClient_GetVersion(gAppContext->qvrCameraClient);
    LOGI("svrInitQVRCamera qvrCameraApiVersion=%d", gAppContext->qvrCameraApiVersion);

    gAppContext->qvrDeviceClient = QVRCameraClient_AttachCamera(gAppContext->qvrCameraClient,
                                                                QVRSERVICE_CAMERA_NAME_TRACKING);
    if (nullptr == gAppContext->qvrDeviceClient) {
        LOGE("svrInitQVRCamera Failed for QVRCameraClient_AttachCamera");
        return SVR_ERROR_UNKNOWN;
    }

    QVRCAMERA_CAMERA_STATUS cameraStatus = QVRCAMERA_CAMERA_ERROR;
    int32_t res = QVRCameraDevice_GetCameraState(gAppContext->qvrDeviceClient, &cameraStatus);
    if (QVR_CAM_SUCCESS != res) {
        LOGE("svrInitQVRCamera Failed for QVRCameraDevice_GetCameraState res=%d", res);
        return SVR_ERROR_UNKNOWN;
    }
    LOGI("svrInitQVRCamera cameraStatus=%d", cameraStatus);

    char paramVal[2] = {0};
    res = QVRCameraDevice_GetParamNum(gAppContext->qvrDeviceClient,
                                      QVR_CAMDEVICE_UINT16_CAMERA_FRAME_WIDTH,
                                      QVRCAMERA_PARAM_NUM_TYPE_UINT16,
                                      sizeof(uint16_t),
                                      paramVal);
    if (QVR_CAM_SUCCESS != res) {
        LOGE("svrInitQVRCamera Failed for QVRCameraDevice_GetParamNum FRAME_WIDTH res=%d", res);
        return SVR_ERROR_UNKNOWN;
    }
    gAppContext->cameraWidth = *reinterpret_cast<uint16_t *>(paramVal);
    res = QVRCameraDevice_GetParamNum(gAppContext->qvrDeviceClient,
                                      QVR_CAMDEVICE_UINT16_CAMERA_FRAME_HEIGHT,
                                      QVRCAMERA_PARAM_NUM_TYPE_UINT16,
                                      sizeof(uint16_t),
                                      paramVal);
    if (QVR_CAM_SUCCESS != res) {
        LOGE("svrInitQVRCamera Failed for QVRCameraDevice_GetParamNum FRAME_WIDTH res=%d", res);
        return SVR_ERROR_UNKNOWN;
    }
    gAppContext->cameraHeight = *reinterpret_cast<uint16_t *>(paramVal);
    LOGI("svrInitQVRCamera cameraWidth=%u, cameraHeight=%u", gAppContext->cameraWidth,
         gAppContext->cameraHeight);
    for (int bufferIdx = 0; bufferIdx != CAMERA_BUFFER_NUM; ++bufferIdx) {
        gCameraFrameDataArray[bufferIdx].dataArray = new uint8_t[gAppContext->cameraWidth *
                                                                 gAppContext->cameraHeight];
        gCameraFrameIndexArray[bufferIdx] = -1;
    }
    gAppContext->qvrCameraThread = std::thread(fetchingQVRCameraFrame);
    LOGI("svrInitQVRCamera done");
    return SVR_ERROR_NONE;
}


SvrResult svrDestroyQVRCamera() {
    LOGI("svrDestroyQVRCamera start gUseQVRCamera=%d", gUseQVRCamera);
    if (!gUseQVRCamera) {
        return SVR_ERROR_NONE;
    }

    gAppContext->qvrCameraTerminalSignal.raiseSignal();
    gAppContext->qvrCameraStartSignal.raiseSignal();
    if(gAppContext->qvrCameraThread.joinable()){
        LOGI("%s, qvr Camera Thread was joinable", __FUNCTION__ );
        gAppContext->qvrCameraThread.join();
    } else {
        LOGE("%s, qvr Camera Thread was not joinable", __FUNCTION__ );
    }

    QVRCameraDevice_DetachCamera(gAppContext->qvrDeviceClient);
    gAppContext->qvrDeviceClient = nullptr;
    QVRCameraClient_Destroy(gAppContext->qvrCameraClient);
    gAppContext->qvrCameraClient = nullptr;
    // willie change end 2020-8-12
    LOGI("svrDestroyQVRCamera done");
    return SVR_ERROR_NONE;
}

SvrResult svrGetLatestCameraDataForUnity(bool &outBUpdate, uint32_t &outCurrFrameIndex,
                                         uint64_t &outFrameExposureNano, uint8_t *outFrameData,
                                         float *outTRDataArray) {
    outBUpdate = false;
    int availableIndex = -1;
    {
        std::unique_lock<std::mutex> autoLock(gAppContext->cameraMutex);
        availableIndex = gAppContext->availableCameraBufferIdx;
    }
    if (-1 == availableIndex || -1 == gCameraFrameIndexArray[availableIndex]) {
        LOGI("sxrGetLatestCameraFrameData skip for availableIndex=%d", availableIndex);
        return SVR_ERROR_NONE;
    }
    if (outCurrFrameIndex != gCameraFrameIndexArray[availableIndex]) {
        // copy left half qvr camera data
        int halfWidth = gAppContext->cameraWidth / 2;
        for (int idx = 0; idx != gAppContext->cameraHeight; ++idx) {
            memcpy(outFrameData + idx * halfWidth,
                   gCameraFrameDataArray[availableIndex].dataArray +
                   idx * gAppContext->cameraWidth, sizeof(bool) * halfWidth);
        }
        // copy the whole qvr camera array data
//        memcpy(outFrameData, gCameraFrameDataArray[availableIndex].dataArray,
//               gAppContext->cameraWidth * gAppContext->cameraHeight);
        outCurrFrameIndex = gCameraFrameIndexArray[availableIndex];
        outFrameExposureNano = gCameraFrameDataArray[availableIndex].exposureNano;
        if (gAppContext->mUseIvSlam) {
            float ivpose[16] = {0};
            double exposureSecond = outFrameExposureNano * 1.0e-9;
            IvSlamClient_GetPose(gAppContext->ivslamHelper, ivpose, -1, exposureSecond);
            glm::mat4 poseMatrix = glm::make_mat4(ivpose);
            glm::quat outData_rotationQuaternion = glm::quat_cast(poseMatrix);
            outTRDataArray[0] = outData_rotationQuaternion.x;
            outTRDataArray[1] = outData_rotationQuaternion.y;
            outTRDataArray[2] = outData_rotationQuaternion.z;
            outTRDataArray[3] = outData_rotationQuaternion.w;
            outTRDataArray[4] = poseMatrix[3][0];
            outTRDataArray[5] = poseMatrix[3][1];
            outTRDataArray[6] = poseMatrix[3][2];
        } else {
            auto historyPose = svrGetHistoricHeadPose(outFrameExposureNano, true).pose;
            outTRDataArray[0] = historyPose.rotation.x;
            outTRDataArray[1] = historyPose.rotation.y;
            outTRDataArray[2] = historyPose.rotation.z;
            outTRDataArray[3] = historyPose.rotation.w;
            outTRDataArray[4] = historyPose.position.x;
            outTRDataArray[5] = historyPose.position.y;
            outTRDataArray[6] = historyPose.position.z;
        }

        // modify orientation.z and position.xy
        outTRDataArray[2] = -outTRDataArray[2];
        outTRDataArray[4] = -outTRDataArray[4];
        outTRDataArray[5] = -outTRDataArray[5];

        outBUpdate = true;
    }
    return SVR_ERROR_NONE;
}


SvrResult
svrGetLatestCameraDataForUnityNoTransform(bool &outBUpdate, uint32_t &outCurrFrameIndex,
                                          uint64_t &outFrameExposureNano, uint8_t *outFrameData,
                                          float *outTRDataArray) {
    outBUpdate = false;
    int availableIndex = -1;
    {
        std::unique_lock<std::mutex> autoLock(gAppContext->cameraMutex);
        availableIndex = gAppContext->availableCameraBufferIdx;
    }
    if (-1 == availableIndex || -1 == gCameraFrameIndexArray[availableIndex]) {
        LOGI("sxrGetLatestCameraFrameData skip for availableIndex=%d", availableIndex);
        return SVR_ERROR_NONE;
    }
    if (outCurrFrameIndex != gCameraFrameIndexArray[availableIndex]) {
        // copy left half qvr camera data
        int halfWidth = gAppContext->cameraWidth / 2;
        for (int idx = 0; idx != gAppContext->cameraHeight; ++idx) {
            memcpy(outFrameData + idx * halfWidth,
                   gCameraFrameDataArray[availableIndex].dataArray +
                   idx * gAppContext->cameraWidth, sizeof(bool) * halfWidth);
        }
        // copy the whole qvr camera array data
//        memcpy(outFrameData, gCameraFrameDataArray[availableIndex].dataArray,
//               gAppContext->cameraWidth * gAppContext->cameraHeight);
        outCurrFrameIndex = gCameraFrameIndexArray[availableIndex];
        outFrameExposureNano = gCameraFrameDataArray[availableIndex].exposureNano;
        float rawCorrectX = gSensorOrientationCorrectX;
        float rawCorrectY = gSensorOrientationCorrectY;
        float rawCorrectZ = gSensorOrientationCorrectZ;
        float rawHeadOffsetX = gSensorHeadOffsetX;
        float rawHeadOffsetY = gSensorHeadOffsetY;
        float rawHeadOffsetZ = gSensorHeadOffsetZ;
        gSensorOrientationCorrectX = 0;
        gSensorOrientationCorrectY = 0;
        gSensorOrientationCorrectZ = 0;
        gSensorHeadOffsetX = 0;
        gSensorHeadOffsetY = 0;
        gSensorHeadOffsetZ = 0;
        if (gAppContext->mUseIvSlam) {
            float ivpose[16] = {0};
            double exposureSecond = outFrameExposureNano * 1.0e-9;
            IvSlamClient_GetPose(gAppContext->ivslamHelper, ivpose, -1, exposureSecond);
            glm::mat4 poseMatrix = glm::make_mat4(ivpose);
            glm::quat outData_rotationQuaternion = glm::quat_cast(poseMatrix);
            outTRDataArray[0] = outData_rotationQuaternion.x;
            outTRDataArray[1] = outData_rotationQuaternion.y;
            outTRDataArray[2] = outData_rotationQuaternion.z;
            outTRDataArray[3] = outData_rotationQuaternion.w;
            outTRDataArray[4] = poseMatrix[3][0];
            outTRDataArray[5] = poseMatrix[3][1];
            outTRDataArray[6] = poseMatrix[3][2];
        } else {
            auto historyPose = svrGetHistoricHeadPose(outFrameExposureNano, true).pose;
            outTRDataArray[0] = historyPose.rotation.x;
            outTRDataArray[1] = historyPose.rotation.y;
            outTRDataArray[2] = historyPose.rotation.z;
            outTRDataArray[3] = historyPose.rotation.w;
            outTRDataArray[4] = historyPose.position.x;
            outTRDataArray[5] = historyPose.position.y;
            outTRDataArray[6] = historyPose.position.z;
        }
        gSensorOrientationCorrectX = rawCorrectX;
        gSensorOrientationCorrectY = rawCorrectY;
        gSensorOrientationCorrectZ = rawCorrectZ;
        gSensorHeadOffsetX = rawHeadOffsetX;
        gSensorHeadOffsetY = rawHeadOffsetY;
        gSensorHeadOffsetZ = rawHeadOffsetZ;


        // modify orientation.z and position.xy
        outTRDataArray[2] = -outTRDataArray[2];
        outTRDataArray[4] = -outTRDataArray[4];
        outTRDataArray[5] = -outTRDataArray[5];

        outBUpdate = true;
    }
    return SVR_ERROR_NONE;
}


void a11CameraCallback(void *data, uint64_t ts, u_short gain, u_short exp) {
    LOGI("a11CameraCallback start exp=%u", exp);
    if (exp >= 1500) {
        gAppContext->newCameraBufferIdx =
                (gAppContext->newCameraBufferIdx + 1) % CAMERA_BUFFER_NUM;
        memcpy(gCameraFrameDataArray[gAppContext->newCameraBufferIdx].dataArray,
               data, 1280 * 480);
        gCameraFrameDataArray[gAppContext->newCameraBufferIdx].exposureNano = ts;
        gCameraFrameIndexArray[gAppContext->newCameraBufferIdx] = gCameraFrameIndex++;
        {
            std::unique_lock<std::mutex> autoLock(gAppContext->cameraMutex);
            gAppContext->availableCameraBufferIdx = gAppContext->newCameraBufferIdx;
        }
        LOGI("IvCameraCallback handle finished");
    }
    LOGI("IvCameraCallback done");
}

SvrResult svrInitSCCamera() {
    LOGI("svrInitSCCamera start gUseSCCamera=%d gUseLinePtr=%d", gUseSCCamera, gUseLinePtr);
    if (!gUseSCCamera) {
        return SVR_ERROR_NONE;
    }
    if (nullptr == gAppContext) {
        return SVR_ERROR_UNKNOWN;
    }
    LOGI("svrInitSCCamera gAppContext->a11GdeviceClientHelper=%p",
         gAppContext->a11GdeviceClientHelper);

    if (!gUseLinePtr) {
        gAppContext->a11GdeviceClientHelper = A11GDeviceClient_Create();
        A11GDeviceClient_Start(gAppContext->a11GdeviceClientHelper, TYPE_OTHERS);
    }
    A11GDeviceClient_RegisterCameraCB(gAppContext->a11GdeviceClientHelper, a11CameraCallback);
    gAppContext->cameraWidth = 1280;
    gAppContext->cameraHeight = 480;
    for (int bufferIdx = 0; bufferIdx != CAMERA_BUFFER_NUM; ++bufferIdx) {
        gCameraFrameDataArray[bufferIdx].dataArray = new uint8_t[gAppContext->cameraWidth *
                                                                 gAppContext->cameraHeight];
        gCameraFrameIndexArray[bufferIdx] = -1;
    }

    LOGI("svrInitSCCamera done");
    return SVR_ERROR_NONE;
}


SvrResult svrDestroySCCamera() {
    LOGI("svrDestroySCCamera start gUseSCCamera=%d", gUseSCCamera);
    if (!gUseSCCamera || nullptr == gAppContext->a11GdeviceClientHelper) {
        return SVR_ERROR_NONE;
    }
    if (nullptr == gAppContext) {
        return SVR_ERROR_UNKNOWN;
    }
    if (!gUseLinePtr) {
        A11GDeviceClient_Stop(gAppContext->a11GdeviceClientHelper, TYPE_OTHERS);
    }
    LOGI("svrDestroyQVRCamera done");
    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
extern "C" SVRP_EXPORT SvrResult

svrInitializeOptArgs(const svrInitParams *pInitParams, void *pTmAPI)
//-----------------------------------------------------------------------------
{
#if defined(SVR_PROFILING_ENABLED) && defined(SVR_PROFILE_TELEMETRY)
    gTelemetryAPI = (tm_api*)pTmAPI;
#endif

    return svrInitialize(pInitParams);
}

static bool userIvSlam() {
    bool ret = false;
    char value[4] = {0x00};
    __system_property_get("persist.debug_useivslam", value);

    //LOGI("%s, persist.debug_useivslam = %c",__FUNCTION__,value[0]);
    if ('1' == value[0]) {
        ret = true;
    }
    return ret;
}

int loadTransformMatrix(std::string path) {
    LOGI("loadTransformMatrix start path=%s", path.c_str());

    std::ifstream file(path);
    if (file.fail()) {
        LOGE("LayerScene::loadTransformMatrix Failed, use default");
        gAppContext->transformMatrixArray[0] = glm::mat4(1);
        gAppContext->transformMatrixArray[1] = glm::mat4(1);
        gAppContext->useTransformMatrix = false;
        return 1;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    std::vector<float> valueVector{std::istream_iterator<float>(ss),
                                   std::istream_iterator<float>()};
    gAppContext->transformMatrixArray[0] = glm::transpose(glm::make_mat4(&valueVector[0]));
    gAppContext->transformMatrixArray[1] = glm::transpose(glm::make_mat4(&valueVector[16]));
    gAppContext->useTransformMatrix = true;
    // willie note here 2020-12-5
    // currently disable transform matrix.
//    gAppContext->useTransformMatrix = false;

    glm::mat4 correctionYMatrix = glm::rotate(glm::mat4(1),
//                                              glm::radians(gSensorOrientationCorrectY),
                                              glm::radians(11.0f),
                                              glm::vec3(1.0f, 0.0f, 0.0f));
    gAppContext->correctYQuat = glm::quat_cast(correctionYMatrix);
    LOGI("left transformMatrix = [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
         gAppContext->transformMatrixArray[0][0][0], gAppContext->transformMatrixArray[0][1][0],
         gAppContext->transformMatrixArray[0][2][0], gAppContext->transformMatrixArray[0][3][0],
         gAppContext->transformMatrixArray[0][0][1], gAppContext->transformMatrixArray[0][1][1],
         gAppContext->transformMatrixArray[0][2][1], gAppContext->transformMatrixArray[0][3][1],
         gAppContext->transformMatrixArray[0][0][2], gAppContext->transformMatrixArray[0][1][2],
         gAppContext->transformMatrixArray[0][2][2], gAppContext->transformMatrixArray[0][3][2],
         gAppContext->transformMatrixArray[0][0][3], gAppContext->transformMatrixArray[0][1][3],
         gAppContext->transformMatrixArray[0][2][3], gAppContext->transformMatrixArray[0][3][3]);
    LOGI("right transformMatrix = [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
         gAppContext->transformMatrixArray[1][0][0], gAppContext->transformMatrixArray[1][1][0],
         gAppContext->transformMatrixArray[1][2][0], gAppContext->transformMatrixArray[1][3][0],
         gAppContext->transformMatrixArray[1][0][1], gAppContext->transformMatrixArray[1][1][1],
         gAppContext->transformMatrixArray[1][2][1], gAppContext->transformMatrixArray[1][3][1],
         gAppContext->transformMatrixArray[1][0][2], gAppContext->transformMatrixArray[1][1][2],
         gAppContext->transformMatrixArray[1][2][2], gAppContext->transformMatrixArray[1][3][2],
         gAppContext->transformMatrixArray[1][0][3], gAppContext->transformMatrixArray[1][1][3],
         gAppContext->transformMatrixArray[1][2][3], gAppContext->transformMatrixArray[1][3][3]);
    LOGI("correctYQuat=[%f, %f, %f, %f]", gAppContext->correctYQuat.x, gAppContext->correctYQuat.y,
         gAppContext->correctYQuat.z, gAppContext->correctYQuat.w);
    LOGI("loadTransformMatrix done");
    return 0;
}

//-----------------------------------------------------------------------------
SvrResult svrInitialize(const svrInitParams *pInitParams)
//-----------------------------------------------------------------------------
{
    LOGI("svrInitialize");

    LOGI("svrApi Version : %s (%s)", svrGetVersion(), ABI_STRING);

    LOGI("Invision Version : %s", getInvisionVersion());

    gAppContext = new SvrAppContext();

    gAppContext->qvrHelper = NULL;

    gAppContext->inVrMode = false;
    gAppContext->mUseIvSlam = userIvSlam();

    gAppContext->looper = ALooper_forThread();
    gAppContext->javaVm = pInitParams->javaVm;
    gAppContext->javaEnv = pInitParams->javaEnv;
    gAppContext->javaActivityObject = pInitParams->javaActivityObject;

    PROFILE_INITIALIZE();

    //Initialize render extensions
    if (!InitializeRenderExtensions()) {
        LOGE("Failed to initialize required EGL/GL extensions");
        return SVR_ERROR_UNSUPPORTED;
    } else {
        LOGI("EGL/GL Extensions Initialized");
    }

    //Load the SvrApi Java class and cache necessary method references
    //Since we are utilizing a native activity we need to go through the activities class loader to find the SvrApi class

    JNIEnv *pThreadJEnv;
    if (gAppContext->javaVm->AttachCurrentThread(&pThreadJEnv, NULL) != JNI_OK) {
        LOGE("svrInitialize AttachCurrentThread failed.");
        return SVR_ERROR_JAVA_ERROR;
    }

    jclass tmpActivityClass = pThreadJEnv->FindClass("android/app/NativeActivity");
    jclass activityClass = (jclass) pThreadJEnv->NewGlobalRef(tmpActivityClass);

    jmethodID getClassLoaderMethodId = pThreadJEnv->GetMethodID(activityClass, "getClassLoader",
                                                                "()Ljava/lang/ClassLoader;");
    jobject classLoaderObj = pThreadJEnv->CallObjectMethod(gAppContext->javaActivityObject,
                                                           getClassLoaderMethodId);

    jclass tmpClassLoaderClass = pThreadJEnv->FindClass("java/lang/ClassLoader");
    jclass classLoader = (jclass) pThreadJEnv->NewGlobalRef(tmpClassLoaderClass);

    jmethodID findClassMethodId = pThreadJEnv->GetMethodID(classLoader, "loadClass",
                                                           "(Ljava/lang/String;)Ljava/lang/Class;");

    //Get a reference to the SvrApi class
    jstring strClassName = pThreadJEnv->NewStringUTF("com/qualcomm/svrapi/SvrApi");
    jclass tmpjavaSvrApiClass = (jclass) pThreadJEnv->CallObjectMethod(classLoaderObj,
                                                                       findClassMethodId,
                                                                       strClassName);
    gAppContext->javaSvrApiClass = (jclass) pThreadJEnv->NewGlobalRef(tmpjavaSvrApiClass);

    if (gAppContext->javaSvrApiClass == NULL) {
        LOGE("Failed to initialzie SvrApi Java class");
        return SVR_ERROR_JAVA_ERROR;
    }

    //Register our native methods
    struct {
        jclass clazz;
        JNINativeMethod nm;
    } nativeMethods[] =
            {
                    {gAppContext->javaSvrApiClass, {"nativeVsync", "(J)V", (void *) Java_com_qualcomm_svrapi_SvrApi_nativeVsync}}
            };

    const int count = sizeof(nativeMethods) / sizeof(nativeMethods[0]);

    for (int i = 0; i < count; i++) {
        if (pThreadJEnv->RegisterNatives(nativeMethods[i].clazz, &nativeMethods[i].nm, 1) !=
            JNI_OK) {
            LOGE("Failed to register %s", nativeMethods[i].nm.name);
            return SVR_ERROR_JAVA_ERROR;
        }
    }

    //Cache method ids for the start/stop vsync callback methods
    gAppContext->javaSvrApiStartVsyncMethodId = pThreadJEnv->GetStaticMethodID(
            gAppContext->javaSvrApiClass,
            "startVsync", "(Landroid/app/Activity;)V");
    if (gAppContext->javaSvrApiStartVsyncMethodId == NULL) {
        LOGE("Failed to locate startVsync method");
        return SVR_ERROR_JAVA_ERROR;
    }

    gAppContext->javaSvrApiStopVsyncMethodId = pThreadJEnv->GetStaticMethodID(
            gAppContext->javaSvrApiClass,
            "stopVsync", "(Landroid/app/Activity;)V");
    if (gAppContext->javaSvrApiStopVsyncMethodId == NULL) {
        LOGE("Failed to locate stopVsync method");
        return SVR_ERROR_JAVA_ERROR;
    }

    //Gather information about the device
    memset(&gAppContext->deviceInfo, 0, sizeof(svrDeviceInfo));

    jmethodID refreshRateId = pThreadJEnv->GetStaticMethodID(gAppContext->javaSvrApiClass,
                                                             "getRefreshRate",
                                                             "(Landroid/app/Activity;)F");
    if (refreshRateId == NULL) {
        LOGE("svrGetDeviceInfo: Failed to locate getRefreshRate method");
        return SVR_ERROR_JAVA_ERROR;
    } else {
        jfloat refreshRate = pThreadJEnv->CallStaticFloatMethod(gAppContext->javaSvrApiClass,
                                                                refreshRateId,
                                                                gAppContext->javaActivityObject);
        gAppContext->deviceInfo.displayRefreshRateHz = refreshRate;
        LOGI("Display Refresh : %f", refreshRate);
    }

    jmethodID getDisplayWidthId = pThreadJEnv->GetStaticMethodID(gAppContext->javaSvrApiClass,
                                                                 "getDisplayWidth",
                                                                 "(Landroid/app/Activity;)I");
    if (getDisplayWidthId == NULL) {
        LOGE("Failed to locate getDisplayWidth method");
        return SVR_ERROR_JAVA_ERROR;
    } else {
        jint displayWidth = pThreadJEnv->CallStaticIntMethod(gAppContext->javaSvrApiClass,
                                                             getDisplayWidthId,
                                                             gAppContext->javaActivityObject);
        gAppContext->deviceInfo.displayWidthPixels = displayWidth;
        LOGI("Display Width : %d", displayWidth);
    }

    jmethodID getDisplayHeightId = pThreadJEnv->GetStaticMethodID(gAppContext->javaSvrApiClass,
                                                                  "getDisplayHeight",
                                                                  "(Landroid/app/Activity;)I");
    if (getDisplayHeightId == NULL) {
        LOGE("Failed to locate getDisplayHeight method");
        return SVR_ERROR_JAVA_ERROR;
    } else {
        jint displayHeight = pThreadJEnv->CallStaticIntMethod(gAppContext->javaSvrApiClass,
                                                              getDisplayHeightId,
                                                              gAppContext->javaActivityObject);
        gAppContext->deviceInfo.displayHeightPixels = displayHeight;
        LOGI("Display Height : %d", displayHeight);
    }


    jmethodID getDisplayOrientationId = pThreadJEnv->GetStaticMethodID(gAppContext->javaSvrApiClass,
                                                                       "getDisplayOrientation",
                                                                       "(Landroid/app/Activity;)I");
    if (getDisplayOrientationId == NULL) {
        LOGE("Failed to locate getDisplayOrientation method");
        return SVR_ERROR_JAVA_ERROR;
    } else {
        jint displayOrientation = 270;
        displayOrientation = pThreadJEnv->CallStaticIntMethod(gAppContext->javaSvrApiClass,
                                                              getDisplayOrientationId,
                                                              gAppContext->javaActivityObject);
        gAppContext->deviceInfo.displayOrientation = displayOrientation;
        LOGI("Display Orientation : %d", displayOrientation);
    }

    //Get the current OS version
    int osVersion = svrGetAndroidOSVersion();
    jmethodID getAndroidOsVersion = pThreadJEnv->GetStaticMethodID(gAppContext->javaSvrApiClass,
                                                                   "getAndroidOsVersion", "()I");
    {
        if (getAndroidOsVersion == NULL) {
            LOGE("Failed to locate getAndroidOsVersion method");
        } else {
            LOGI("Getting Android OS Version...");
            osVersion = pThreadJEnv->CallStaticIntMethod(gAppContext->javaSvrApiClass,
                                                         getAndroidOsVersion);
        }
    }
    LOGI("    Android OS Version = %d", osVersion);

    gAppContext->currentTrackingMode = 0;


    if (gAppContext->mUseIvSlam) {
        LOGE("IvSlam create ivslamHelper");
        gAppContext->ivslamHelper = IvSlamClient_Create();
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("IvSlam Serivce reported VR was not supported, ivslamHelper=NULL");
            return SVR_ERROR_UNSUPPORTED;
        }
    } else {
        LOGI("Calling QVRServiceClient_Create()...");
        gAppContext->qvrHelper = QVRServiceClient_Create();
        if (gAppContext->qvrHelper == NULL ||
            QVRServiceClient_GetVRMode(gAppContext->qvrHelper) == VRMODE_UNSUPPORTED) {
            // Need to destroy this so later functions will not return
            // an incomplete device info.
            if (gAppContext->qvrHelper != NULL)
                QVRServiceClient_Destroy(gAppContext->qvrHelper);
            gAppContext->qvrHelper = NULL;

            LOGE("    QVR Serivce reported VR not supported");
            svrNotifyFailedQvrService();
            return SVR_ERROR_UNSUPPORTED;
        }
    }

    //add zengchuiguo 20200724 (start)
    gAppContext->ivgestureClientHelper = IvGestureClient_Create();
    //add zengchuiguo 20200724 (end)

    // add by chenweihua 20201026 (start)
    if (gAppContext->ivhandshankHelper == nullptr) {
        gAppContext->ivhandshankHelper = HandShankClient_Create();
        HandShankClient_Start(gAppContext->ivhandshankHelper);
    }
    // add by chenweihua 20201026 (end)

    // Svr Service
    {
        //TODO: need to pass gAppContext or the eventlooper to add events
        gAppContext->svrServiceClient = new SvrServiceClient(gAppContext->javaVm,
                                                             gAppContext->javaActivityObject);
        gAppContext->svrServiceClient->Connect();
    }

    int qRes;
    if (gAppContext->mUseIvSlam) {
        //TODO:Check whether the qvrClientStatusCallback is necessary for IvSlam
    } else {
        qRes = QVRServiceClient_SetClientStatusCallback(gAppContext->qvrHelper,
                                                        qvrClientStatusCallback, 0);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_SetClientStatusCallback()", qRes);
        }
    }

    //Get the supported tracking modes
    uint32_t supportedTrackingModes = svrGetSupportedTrackingModes();
    if (supportedTrackingModes & kTrackingRotation) {
        LOGI("  QVR Service supports rotational tracking");
    }
    if (supportedTrackingModes & kTrackingPosition) {
        LOGI("  QVR Service supports positional tracking");
    }

    //Load SVR configuration options
    unsigned int len = 0;
    if (gAppContext->mUseIvSlam) {
        qRes = invisionGetParam(NULL, &len);
        if (qRes == QVR_SUCCESS) {
            LOGI("%s, Loading variables [len=%d]", __FUNCTION__, len);
            char *p0 = (char *) malloc(sizeof(char) * len);
            if (!p0) {
                LOGI("%s, Insufficient memory", __FUNCTION__);
                return SVR_ERROR_UNSUPPORTED;
            }
            memset(p0, 0x00, sizeof(char) * len);
            qRes = invisionGetParam(p0, &len);
            if (qRes == QVR_SUCCESS) {
//                dump_buffer_to_txt("svrapi_config_dump",p0,len);
                LoadVariableBuffer(p0);
            } else {
                LOGE("%s, Loading variables from svrapi_config.txt fail", __FUNCTION__);
            }
            free(p0);
        } else {
            LOGE("%s, Loading variables from svrapi_config.txt fail", __FUNCTION__);
        }
        loadTransformMatrix("/data/misc/ivslam/a11_transform.txt");
    } else {
        // willie change start 2020-8-20
        std::string customPath{"/persist/qvr/svrapi_config.txt"};
        std::string oriPath{"/system/vendor/etc/qvr/svrapi_config.txt"};
        struct stat testBuffer;
        if (stat(customPath.c_str(), &testBuffer) == 0) {
            LoadVariableFile(customPath.c_str());
        } else {
            LoadVariableFile(oriPath.c_str());
        }

        loadTransformMatrix("/persist/qvr/a95_transform.txt");
//        qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SDK_CONFIG_FILE, &len,
//                                         NULL);
//        if (qRes == QVR_SUCCESS) {
//            LOGI("Loading variables from QVR Service [len=%d]", len);
//            if (len > 0) {
//                char *p = new char[len];
//                qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SDK_CONFIG_FILE,
//                                                 &len, p);
//                if (qRes == QVR_SUCCESS) {
//                    LoadVariableBuffer(p);
//                } else {
//                    L_LogQvrError(
//                            "QVRServiceClient_SetClientStatusCallback(QVRSERVICE_SDK_CONFIG_FILE)",
//                            qRes);
//                }
//                delete[] p;
//            }
//        } else {
//            L_LogQvrError("QVRServiceClient_GetParam(QVRSERVICE_SDK_CONFIG_FILE - Length)", qRes);
//        }
    }


    if (gUseLinePtr) {
        if (!gAppContext->mUseIvSlam) {
            //Make sure the linePtr support is actually available
            qvrservice_ts_t *qvrStamp;
            qRes = QVRServiceClient_GetDisplayInterruptTimestamp(gAppContext->qvrHelper,
                                                                 DISP_INTERRUPT_LINEPTR, &qvrStamp);
            if (qRes != QVR_SUCCESS) {
                L_LogQvrError("QVRServiceClient_GetDisplayInterruptTimestamp", qRes);
                gUseLinePtr = false;
            }
        } else {
            gAppContext->a11GdeviceClientHelper = A11GDeviceClient_Create();
        }
    }

    //Set the default tracking mode to rotational only
    // Need to be AFTER reading the config file so overrides will work
    svrSetTrackingMode(kTrackingRotation);

    //Set other device info
    gAppContext->deviceInfo.deviceOSVersion = osVersion;
    gAppContext->deviceInfo.targetEyeWidthPixels = gEyeBufferWidth;
    gAppContext->deviceInfo.targetEyeHeightPixels = gEyeBufferHeight;
    gAppContext->deviceInfo.targetFovXRad = gEyeBufferFovX * DEG_TO_RAD;
    gAppContext->deviceInfo.targetFovYRad = gEyeBufferFovY * DEG_TO_RAD;

    // Frustum
    gAppContext->deviceInfo.targetEyeConvergence = gFrustum_Convergence;
    gAppContext->deviceInfo.targetEyePitch = gFrustum_Pitch;

    // Left Eye Frustum
    gAppContext->deviceInfo.leftEyeFrustum.near = gLeftFrustum_Near;
    gAppContext->deviceInfo.leftEyeFrustum.far = gLeftFrustum_Far;
    gAppContext->deviceInfo.leftEyeFrustum.left = gLeftFrustum_Left;
    gAppContext->deviceInfo.leftEyeFrustum.right = gLeftFrustum_Right;
    gAppContext->deviceInfo.leftEyeFrustum.top = gLeftFrustum_Top;
    gAppContext->deviceInfo.leftEyeFrustum.bottom = gLeftFrustum_Bottom;

    // Right Eye Frustum
    gAppContext->deviceInfo.rightEyeFrustum.near = gRightFrustum_Near;
    gAppContext->deviceInfo.rightEyeFrustum.far = gRightFrustum_Far;
    gAppContext->deviceInfo.rightEyeFrustum.left = gRightFrustum_Left;
    gAppContext->deviceInfo.rightEyeFrustum.right = gRightFrustum_Right;
    gAppContext->deviceInfo.rightEyeFrustum.top = gRightFrustum_Top;
    gAppContext->deviceInfo.rightEyeFrustum.bottom = gRightFrustum_Bottom;

    // Warp mesh type
    switch (gWarpMeshType) {
        case 0:     // 0 = Columns (Left To Right)
            gAppContext->deviceInfo.warpMeshType = kMeshTypeColumsLtoR;
            break;

        case 1:     // 1 = Columns (Right To Left)
            gAppContext->deviceInfo.warpMeshType = kMeshTypeColumsRtoL;
            break;

        case 2:     // 2 = Rows (Top To Bottom)
            gAppContext->deviceInfo.warpMeshType = kMeshTypeRowsTtoB;
            break;

        case 3:     // 3 = Rows (Bottom To Top)
            gAppContext->deviceInfo.warpMeshType = kMeshTypeRowsBtoT;
            break;

        default:
            LOGE("Unknown warp mesh type (gWarpMeshType) specified in configuration file: %d",
                 gWarpMeshType);
            gAppContext->deviceInfo.warpMeshType = kMeshTypeColumsLtoR;
            break;
    }

    //Log out some useful information
    jmethodID vsyncOffetId = pThreadJEnv->GetStaticMethodID(gAppContext->javaSvrApiClass,
                                                            "getVsyncOffsetNanos",
                                                            "(Landroid/app/Activity;)J");
    if (vsyncOffetId == NULL) {
        LOGE("Failed to locate getVsyncOffsetNanos method");
    } else {
        jlong result = pThreadJEnv->CallStaticLongMethod(gAppContext->javaSvrApiClass, vsyncOffetId,
                                                         gAppContext->javaActivityObject);
        LOGI("Vsync Offset : %d", (int) result);
    }

    if (gDisableReprojection) {
        LOGI("Timewarp disabled from configuration file");
    }

    if (gForceMinVsync > 0) {
        LOGI("Forcing minVsync = %d", gForceMinVsync);
    }

    LOGI("Using QVR Performance Module");
    if (gAppContext->mUseIvSlam) {
        svrInitSCCamera();
    } else {
        svrInitQVRCamera();
    }


    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrShutdown()
//-----------------------------------------------------------------------------
{
    LOGI("svrShutdown");
    if (gAppContext->mUseIvSlam) {
        svrDestroySCCamera();
    } else {
        svrDestroyQVRCamera();
    }

    PROFILE_SHUTDOWN();

    if (gAppContext == NULL) {
        LOGE("svrShutdown Failed: SnapdragonVR not initialized!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    //add zengchuiguo 20200724 (start)
    destroyIvGesture();
    //add zengchuiguo 20200724 (end)

    // add by chenweihua 20201026 (start)
    if (gAppContext->ivhandshankHelper) {
        HandShankClient_Stop(gAppContext->ivhandshankHelper);
        HandShankClient_Destroy(gAppContext->ivhandshankHelper);
        gAppContext->ivhandshankHelper = nullptr;
    }
    // add by chenweihua 20201026 (end)

    LOGI("svrShutdown call A11GDeviceClient_Destroy a11GdeviceClientHelper=%p",
         gAppContext->a11GdeviceClientHelper);
    if (gAppContext->a11GdeviceClientHelper) {
        A11GDeviceClient_Destroy(gAppContext->a11GdeviceClientHelper);
    }

    if (gAppContext != NULL) {
        // Disconnect from SvrService
        if (gAppContext->svrServiceClient != 0) {
            gAppContext->svrServiceClient->Disconnect();
            delete gAppContext->svrServiceClient;
            gAppContext->svrServiceClient = 0;
        }

        if (gAppContext->qvrHelper != NULL)
            QVRServiceClient_Destroy(gAppContext->qvrHelper);
        gAppContext->qvrHelper = NULL;

        if (gAppContext->ivslamHelper != nullptr) {
            IvSlamClient_Destroy(gAppContext->ivslamHelper);
            gAppContext->ivslamHelper = nullptr;
        }

        delete gAppContext;
        gAppContext = NULL;
    }

    LOGI("%s end",__FUNCTION__);
    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
svrDeviceInfo svrGetDeviceInfo()
//-----------------------------------------------------------------------------
{
    if (gAppContext != NULL) {
        return gAppContext->deviceInfo;
    } else {
        LOGE("Called svrGetDeviceInfo without initialzing svr");
        svrDeviceInfo tmp;
        memset(&tmp, 0, sizeof(svrDeviceInfo));
        return tmp;
    }
}

//-----------------------------------------------------------------------------
void L_AddHeuristicPredictData(uint64_t newData)
//-----------------------------------------------------------------------------
{
    if (!gHeuristicPredictedTime || gNumHeuristicEntries <= 0 || gpHeuristicPredictData == NULL) {
        LOGE("Unable to add heuristic predicted time data. Heuristic predicted time is not enabled!");
        return;
    }

    // Put this new time in the latest slot...
    float predictTimeMs = (float) newData * NANOSECONDS_TO_MILLISECONDS;

    // During initialization the values put in heuristic list can be very large
    if (predictTimeMs < 0.0f || predictTimeMs > gMaxPredictedTime) {
        if (gLogMaxPredictedTime) {
            LOGI("Heuristic data of %0.3f clampled by gMaxPredictedTime of %0.3f", predictTimeMs,
                 gMaxPredictedTime);
        }
        predictTimeMs = gMaxPredictedTime;
    }

    gpHeuristicPredictData[gHeuristicWriteIndx] = predictTimeMs;

    // ... and bump the write pointer
    gHeuristicWriteIndx++;
    if (gHeuristicWriteIndx >= gNumHeuristicEntries)
        gHeuristicWriteIndx = 0;

}

//-----------------------------------------------------------------------------
float L_GetHeuristicPredictTime()
//-----------------------------------------------------------------------------
{
    if (!gHeuristicPredictedTime || gNumHeuristicEntries <= 0 || gpHeuristicPredictData == NULL) {
        LOGE("Unable to get heuristic predicted time. Heuristic predicted time is not enabled!");
        return 0.0f;
    }

    // What is the average of the last gNumHeuristicEntries
    float averageEntry = 0.0f;
    for (int whichEntry = 0; whichEntry < gNumHeuristicEntries; whichEntry++) {
        averageEntry += gpHeuristicPredictData[whichEntry];
    }
    averageEntry /= (float) gNumHeuristicEntries;

    // Add the offset before logging
    averageEntry += gHeuristicOffset;

    if (gLogPrediction) {
        LOGI("Heuristic predicted display time: %0.3f", averageEntry);
    }

    return averageEntry;
}


//-----------------------------------------------------------------------------
SvrResult svrBeginVr(const svrBeginParams *pBeginParams)
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("%s Failed: App context was not initialized!", __FUNCTION__);
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    LOGI("svrBeginVr");

    if (!gGpuBusyFStream.is_open()) {
        gGpuBusyFStream.open(QCOM_GPU_BUSY_FILE_PATH);
        LOGI("svrBeginVr open gGpuBusyFStream gGpuBusyCheckPeriod=%f", gGpuBusyCheckPeriod);
    }

    gVrStartTimeNano = Svr::GetTimeNano();

    // Motion vectors start out disabled
    gEnableMotionVectors = false;

    // Check if forced on globally...
    if (gForceAppEnableMotionVectors) {
        LOGI("Forcing motion vectors enabled by config file setting");
        gEnableMotionVectors = true;
    }

    // ... or enabled by the application
    if (pBeginParams->optionFlags & kMotionAwareFrames) {
        LOGI("Motion vectors enabled by application");
        gEnableMotionVectors = true;
    }

    if (gHeuristicPredictedTime) {
        if (gNumHeuristicEntries <= 0)
            gNumHeuristicEntries = 25;

        gHeuristicWriteIndx = 0;

        LOGI("Allocating %d entries for heuristic prediction time calculation...",
             gNumHeuristicEntries);
        gpHeuristicPredictData = new float[gNumHeuristicEntries];
        if (gpHeuristicPredictData == NULL) {
            LOGE("Unable to allocate %d entries for heuristic prediction time calculation!",
                 gNumHeuristicEntries);
        } else {
            memset(gpHeuristicPredictData, 0, gNumHeuristicEntries * sizeof(float));
            for (int whichEntry = 0; whichEntry < gNumHeuristicEntries; whichEntry++) {
                gpHeuristicPredictData[whichEntry] = 0.0f;
            }
        }
    }

    //Ensure the VR Service is currently in the stopped state (e.g. another application isn't using it)
    QVRSERVICE_VRMODE_STATE serviceState;

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s Failed: Invision VR not initialized!", __FUNCTION__);
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
        //TODO:implement services state for IvSlam
    } else {
        if (gAppContext->qvrHelper == nullptr) {
            LOGE("%s Failed: SnapdragonVR not initialized!", __FUNCTION__);
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }

        serviceState = QVRServiceClient_GetVRMode(gAppContext->qvrHelper);

        const int maxTries = 8;
        const int waitTime = 500000;
        int attempt = 0;
        while (serviceState != VRMODE_STOPPED && (attempt < maxTries)) {
            switch (serviceState) {
                case VRMODE_UNSUPPORTED:
                    LOGE("svrBeginVr called but VR service is in state VRMODE_UNSUPPORTED, waiting... (attempt %d)",
                         attempt);
                    break;
                case VRMODE_STARTING:
                    LOGE("svrBeginVr called but VR service is in state VRMODE_STARTING, waiting... (attempt %d)",
                         attempt);
                    break;
                case VRMODE_STARTED:
                    LOGE("svrBeginVr called but VR service is in state VRMODE_STARTED, waiting... (attempt %d)",
                         attempt);
                    break;
                case VRMODE_STOPPING:
                    LOGE("svrBeginVr called but VR service is in state VRMODE_STOPPING, waiting... (attempt %d)",
                         attempt);
                    break;
                case VRMODE_STOPPED:
                    LOGE("svrBeginVr called but VR service is in state VRMODE_STOPPED, waiting... (attempt %d)",
                         attempt);
                    break;

                default:
                    LOGE("svrBeginVr called but VR service is in state Unknown (%d), waiting... (attempt %d)",
                         serviceState, attempt);
                    break;
            }
            usleep(waitTime);
            serviceState = QVRServiceClient_GetVRMode(gAppContext->qvrHelper);
            attempt++;
        }

        if (serviceState != VRMODE_STOPPED) {
            LOGE("svrBeginVr, VR service is currently unavailable for this application.");
            return SVR_ERROR_QVR_SERVICE_UNAVAILABLE;
        }
    }

    PROFILE_THREAD_NAME(0, 0, "Eye Render Thread");

    //Set currently selected tracking mode. This is needed when the application resumes from suspension
    LOGI("Set tracking mode context...");
    svrSetTrackingMode(gAppContext->currentTrackingMode);

    LOGI("Creating mode context...");
    gAppContext->modeContext = new SvrModeContext();

    gAppContext->modeContext->renderThreadId = gettid();
    gAppContext->modeContext->warpThreadId = -1;

    gAppContext->modeContext->nativeWindow = pBeginParams->nativeWindow;
    gAppContext->modeContext->isProtectedContent = (pBeginParams->optionFlags & kProtectedContent);

    switch (pBeginParams->colorSpace) {
        case kColorSpaceLinear:
            gAppContext->modeContext->colorspace = EGL_COLORSPACE_LINEAR;
            break;
        case kColorSpaceSRGB:
            gAppContext->modeContext->colorspace = EGL_COLORSPACE_sRGB;
            break;
        default:
            gAppContext->modeContext->colorspace = EGL_COLORSPACE_LINEAR;
            break;
    }

    gAppContext->modeContext->vsyncCount = 0;
    gAppContext->modeContext->vsyncTimeNano = 0;
    pthread_mutex_init(&gAppContext->modeContext->vsyncMutex, NULL);

    pthread_cond_init(&gAppContext->modeContext->warpThreadContextCv, NULL);
    pthread_mutex_init(&gAppContext->modeContext->warpThreadContextMutex, NULL);
    gAppContext->modeContext->warpContextCreated = false;

    gAppContext->modeContext->warpThreadExit = false;
    gAppContext->modeContext->vsyncThreadExit = false;

    pthread_cond_init(&gAppContext->modeContext->warpBufferConsumedCv, NULL);
    pthread_mutex_init(&gAppContext->modeContext->warpBufferConsumedMutex, NULL);

    gAppContext->modeContext->eyeRenderWarpSurface = EGL_NO_SURFACE;
    gAppContext->modeContext->eyeRenderOrigSurface = EGL_NO_SURFACE;
    gAppContext->modeContext->eyeRenderOrigConfigId = -1;
    gAppContext->modeContext->eyeRenderContext = EGL_NO_CONTEXT;
    gAppContext->modeContext->warpRenderContext = EGL_NO_CONTEXT;
    gAppContext->modeContext->warpRenderSurface = EGL_NO_SURFACE;
    gAppContext->modeContext->warpRenderSurfaceWidth = 0;
    gAppContext->modeContext->warpRenderSurfaceHeight = 0;

    //Initialize the warp frame param structures
    memset(&gAppContext->modeContext->frameParams[0], 0,
           sizeof(svrFrameParamsInternal) * NUM_SWAP_FRAMES);
    gAppContext->modeContext->submitFrameCount = 0;
    gAppContext->modeContext->warpFrameCount = 0;
    gAppContext->modeContext->prevSubmitVsyncCount = 0;

    // Recenter rotation
    gAppContext->modeContext->recenterRot = glm::fquat();
    gAppContext->modeContext->recenterPos = glm::vec3(0.0f, 0.0f, 0.0f);

    // Create Event Manager
    gAppContext->modeContext->eventManager = new svrEventManager();

    // Initialize frame counters
    gAppContext->modeContext->fpsPrevTimeMs = 0;
    gAppContext->modeContext->fpsFrameCounter = 0;

    memset(&gAppContext->modeContext->prevPoseState, 0, sizeof(svrHeadPoseState));

    //Start Vsync monitoring
    LOGI("Starting VSync Monitoring...");

    if (gUseLinePtr) {
        if (!gAppContext->mUseIvSlam) {
            LOGI("Configuring QVR Vsync Interrupt...");
            qvrservice_lineptr_interrupt_config_t interruptConfig;
            memset(&interruptConfig, 0, sizeof(qvrservice_lineptr_interrupt_config_t));
            interruptConfig.cb = 0;
            interruptConfig.line = 1;
            //TODO:Check whether the SetDisplayInterruptConfig is necessary for IvSlam
            if (!gAppContext->mUseIvSlam) {
                int qRes = QVRServiceClient_SetDisplayInterruptConfig(gAppContext->qvrHelper,
                                                                      DISP_INTERRUPT_LINEPTR,
                                                                      (void *) &interruptConfig,
                                                                      sizeof(qvrservice_lineptr_interrupt_config_t));
                if (qRes != QVR_SUCCESS) {
                    L_LogQvrError("QVRServiceClient_SetDisplayInterruptConfig", qRes);
                }
            }
        } else {
            //A11GDeviceClient_Start(gAppContext->a11GdeviceClientHelper,TYPE_OTHERS);
        }
    } else {
        //Utilize Choreographer for tracking HW Vsync
        JNIEnv *pThreadJEnv;
        if (gAppContext->javaVm->AttachCurrentThread(&pThreadJEnv, NULL) != JNI_OK) {
            LOGE("    svrBeginVr AttachCurrentThread failed.");
        }

        LOGI("   Using Choreographer VSync Monitoring");
        pThreadJEnv->CallStaticVoidMethod(gAppContext->javaSvrApiClass,
                                          gAppContext->javaSvrApiStartVsyncMethodId,
                                          gAppContext->javaActivityObject);
    }

    // Ring Buffer Descriptor
    qvrservice_ring_buffer_desc_t ringDesc;

    if (gAppContext->mUseIvSlam) {
        IvSlamClient_Start(gAppContext->ivslamHelper);
    } else {
        int qRes;

        //Tell the VR Service to put the system into VR Mode
        //Note : This must be called after the line pointer interrupt has been configured
        qRes = QVRServiceClient_StartVRMode(gAppContext->qvrHelper);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_StartVRMode", qRes);
            delete gAppContext->modeContext;
            gAppContext->modeContext = 0;
            return SVR_ERROR_UNKNOWN;
        }

        serviceState = QVRServiceClient_GetVRMode(gAppContext->qvrHelper);
        if (serviceState != VRMODE_STARTED) {
            switch (serviceState) {
                case VRMODE_UNSUPPORTED:
                    LOGE("svrBeginVr : QVRService VR not started, current State = VRMODE_UNSUPPORTED!");
                    break;

                case VRMODE_STARTING:
                    LOGI("svrBeginVr : QVRService VR not started, current State = VRMODE_STARTING");
                    break;
                case VRMODE_STARTED:
                    LOGI("svrBeginVr : QVRService VR not started, current State = VRMODE_STARTED");
                    break;
                case VRMODE_STOPPING:
                    LOGI("svrBeginVr : QVRService VR not started, current State = VRMODE_STOPPING");
                    break;
                case VRMODE_STOPPED:
                    LOGI("svrBeginVr : QVRService VR not started, current State = VRMODE_STOPPED");
                    break;
                default:
                    LOGE("svrBeginVr : QVRService VR not started, current State = %d",
                         (int) serviceState);
                    break;
            }
            return SVR_ERROR_UNKNOWN;
        }

        // Check the current tracking mode
        QVRSERVICE_TRACKING_MODE currentMode;
        uint32_t supportedModes;
        qRes = QVRServiceClient_GetTrackingMode(gAppContext->qvrHelper, &currentMode,
                                                &supportedModes);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_GetTrackingMode()", qRes);
        } else {
            switch (currentMode) {
                case TRACKING_MODE_NONE:
                    LOGE("Current tracking mode: TRACKING_MODE_NONE");
                    break;

                case TRACKING_MODE_ROTATIONAL:
                    LOGI("Current tracking mode: TRACKING_MODE_ROTATIONAL");
                    break;

                case TRACKING_MODE_POSITIONAL:
                    LOGI("Current tracking mode: TRACKING_MODE_POSITIONAL");
                    break;

                case TRACKING_MODE_ROTATIONAL_MAG:
                    LOGI("Current tracking mode: TRACKING_MODE_ROTATIONAL_MAG");
                    break;

                default:
                    LOGE("Current tracking mode: Unknown = %d", currentMode);
                    break;
            }

            // Says it is a mask, but the results are an enumeration!  This could be anything so just print it out.
            LOGI("Supported tracking mode mask: 0x%x", supportedModes);
        }

        // Ring Buffer Descriptor
        qvrservice_ring_buffer_desc_t ringDesc;
        qRes = QVRServiceClient_GetRingBufferDescriptor(gAppContext->qvrHelper, RING_BUFFER_POSE,
                                                        &ringDesc);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("GetRingBufferDescriptor()", qRes);
        } else {
            LOGI("Pose Ring Buffer:");
            LOGI("    fd: %d", ringDesc.fd);
            LOGI("    size: %d", ringDesc.size);
            LOGI("    index_offset: %d", ringDesc.index_offset);
            LOGI("    ring_offset: %d", ringDesc.ring_offset);
            LOGI("    element_size: %d", ringDesc.element_size);
            LOGI("    num_elements: %d", ringDesc.num_elements);
        }

        unsigned int retLen = 0;

        // **************************************
        LOGI("Reading Sample to Android time...");
        // **************************************
        qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper,
                                         QVRSERVICE_TRACKER_ANDROID_OFFSET_NS,
                                         &retLen, NULL);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError(
                    "QVRServiceClient_GetParam(QVRSERVICE_TRACKER_ANDROID_OFFSET_NS - Length)",
                    qRes);
        } else {
            char *pRetValue = new char[retLen + 1];
            if (pRetValue == NULL) {
                LOGE("Unable to allocate %d bytes for return string!", retLen + 1);
            } else {
                memset(pRetValue, 0, retLen + 1);
                qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper,
                                                 QVRSERVICE_TRACKER_ANDROID_OFFSET_NS, &retLen,
                                                 pRetValue);
                if (qRes != QVR_SUCCESS) {
                    L_LogQvrError("QVRServiceClient_GetParam(QVRSERVICE_TRACKER_ANDROID_OFFSET_NS)",
                                  qRes);
                } else {
                    gQTimeToAndroidBoot = strtoll(pRetValue, NULL, 0);

                    LOGI("QTimer Android offset string: %s", pRetValue);
                    LOGI("QTimer Android offset value: %lld", (long long int) gQTimeToAndroidBoot);
                }

                // No longer need the temporary buffer
                delete[] pRetValue;
            }
        }

        // **************************************
        LOGI("Reading QVR Service Version...");
        // **************************************
        qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SERVICE_VERSION,
                                         &retLen,
                                         NULL);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_GetParam(QVRSERVICE_SERVICE_VERSION - Length)", qRes);
        } else {
            char *pRetValue = new char[retLen + 1];
            if (pRetValue == NULL) {
                LOGE("Unable to allocate %d bytes for return string!", retLen + 1);
            } else {
                memset(pRetValue, 0, retLen + 1);
                qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SERVICE_VERSION,
                                                 &retLen, pRetValue);
                if (qRes != QVR_SUCCESS) {
                    L_LogQvrError("QVRServiceClient_GetParam(QVRSERVICE_SERVICE_VERSION)", qRes);
                } else {
                    LOGI("Qvr Service Version: %s", pRetValue);
                }

                // No longer need the temporary buffer
                delete[] pRetValue;
            }
        }

        // **************************************
        LOGI("Reading QVR Client Version...");
        // **************************************
        qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_CLIENT_VERSION, &retLen,
                                         NULL);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_GetParam(QVRSERVICE_CLIENT_VERSION - Length)", qRes);
        } else {
            char *pRetValue = new char[retLen + 1];
            if (pRetValue == NULL) {
                LOGE("Unable to allocate %d bytes for return string!", retLen + 1);
            } else {
                memset(pRetValue, 0, retLen + 1);
                qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_CLIENT_VERSION,
                                                 &retLen, pRetValue);
                if (qRes != QVR_SUCCESS) {
                    L_LogQvrError("QVRServiceClient_GetParam(QVRSERVICE_CLIENT_VERSION)", qRes);
                } else {
                    LOGI("Qvr Client Version: %s", pRetValue);
                }

                // No longer need the temporary buffer
                delete[] pRetValue;
            }
        }

        //Enable thermal notifications
        qRes = QVRServiceClient_RegisterForNotification(gAppContext->qvrHelper,
                                                        NOTIFICATION_THERMAL_INFO,
                                                        qvrClientThermalNotificationCallback, 0);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_RegisterForNotification", qRes);
        }

        LOGI("QVRService: Service Initialized");


        if (gAppContext->qvrHelper) {
            LOGI("Calling SetThreadAttributesByType for RENDER thread...");
            QVRSERVICE_THREAD_TYPE threadType = gEnableRenderThreadFifo ? RENDER : NORMAL;
            qRes = QVRServiceClient_SetThreadAttributesByType(gAppContext->qvrHelper,
                                                              gAppContext->modeContext->renderThreadId,
                                                              threadType);
            if (qRes != QVR_SUCCESS) {
                L_LogQvrError("QVRServiceClient_SetThreadAttributesByType", qRes);
            }
        } else {
            LOGE("QVRServiceClient unavailable, SetThreadAttributesByType for RENDER thread failed.");
        }
    }

    LOGI("Setting CPU/GPU Performance Levels...");
    svrSetPerformanceLevels(pBeginParams->cpuPerfLevel, pBeginParams->gpuPerfLevel);

    if (gUseLinePtr) {
        if (!gAppContext->mUseIvSlam) {
            int qRes = QVRServiceClient_SetDisplayInterruptCapture(gAppContext->qvrHelper,
                                                                   DISP_INTERRUPT_LINEPTR, 1);
            if (qRes != QVR_SUCCESS) {
                L_LogQvrError("QVRServiceClient_SetDisplayInterruptCapture", qRes);
            } else {
                LOGE("Started VSync Capture");
            }
        } else {
            A11GDeviceClient_Start(gAppContext->a11GdeviceClientHelper, TYPE_OTHERS);
        }

    }

    if (gEnableTimeWarp) {
        LOGI("Starting Timewarp...");
        svrBeginTimeWarp();
    } else {
        LOGI("Timewarp not started.  Disabled by config file.");
    }

    if (gEnableDebugServer) {
        svrStartDebugServer();
    }

    // Controller Manager
    {
        gAppContext->controllerManager = new ControllerManager(gAppContext->javaVm,
                                                               gAppContext->javaActivityObject,
                                                               gAppContext->svrServiceClient,
                                                               gAppContext->modeContext->eventManager,
                                                               ringDesc.fd,
                                                               ringDesc.size);
        gAppContext->controllerManager->Initialize(gControllerService, gControllerRingBufferSize);
    }

    // Only now are we truly in VR mode
    gAppContext->inVrMode = true;

    if (gUseQVRCamera) {
        LOGI("svrApiCore.cpp svrBeginVr raise qvrCameraStartSignal");
        gAppContext->qvrCameraStartSignal.raiseSignal();
    }


    //Signal VR Mode Active
    svrEventQueue *pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
    svrEventData data;
    pQueue->SubmitEvent(kEventVrModeStarted, data);

    data.thermal.zone = kCpu;
    data.thermal.level = kSafe;
    pQueue->SubmitEvent(kEventThermal, data);

    data.thermal.zone = kGpu;
    data.thermal.level = kSafe;
    pQueue->SubmitEvent(kEventThermal, data);

    data.thermal.zone = kSkin;
    data.thermal.level = kSafe;
    pQueue->SubmitEvent(kEventThermal, data);

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrEndVr()
//-----------------------------------------------------------------------------
{
    LOGI("svrEndVr");

    if (gAppContext == NULL) {
        LOGE("Unable to end VR! Application context has been released!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext == NULL) {
        LOGE("Unable to end VR! gAppContext has been released!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s, Unable to end VR! ivslamHelper has been released!", __FUNCTION__);
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("Unable to end VR! qvrHelper has been released!");
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    }

    if (gUseQVRCamera) {
        LOGI("svrApiCore.cpp svrEndVr clear qvrCameraStartSignal");
        gAppContext->qvrCameraStartSignal.clearSignal();
    }

    svrEventQueue *pQueue = NULL;
    svrEventData data;
    if (gAppContext->modeContext != NULL && gAppContext->modeContext->eventManager != NULL) {
        pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
    }

    if (pQueue != NULL)
        pQueue->SubmitEvent(kEventVrModeStopping, data);

    // No longer in VR mode
    gAppContext->inVrMode = false;

    if (gpHeuristicPredictData != NULL) {
        gHeuristicWriteIndx = 0;

        LOGI("Deleting %d entries for heuristic prediction time calculation...",
             gNumHeuristicEntries);
        delete[] gpHeuristicPredictData;
        gpHeuristicPredictData = NULL;
    }


    if (gEnableTimeWarp) {
        LOGI("Stopping Timewarp...");
        svrEndTimeWarp();
    } else {
        LOGI("Not stopping Timewarp.  Was not started since disabled by config file.");
    }

    if (!gUseLinePtr) {
        LOGI("Stopping Choreographer VSync Monitoring...");
        JNIEnv *pThreadJEnv;
        if (gAppContext->javaVm->AttachCurrentThread(&pThreadJEnv, NULL) != JNI_OK) {
            LOGE("    svrBeginVr AttachCurrentThread failed.");
        }
        pThreadJEnv->CallStaticVoidMethod(gAppContext->javaSvrApiClass,
                                          gAppContext->javaSvrApiStopVsyncMethodId,
                                          gAppContext->javaActivityObject);
    }

    int qRes;
    if (!gAppContext->mUseIvSlam) {
        //Disable thermal notifications
        qRes = QVRServiceClient_RegisterForNotification(gAppContext->qvrHelper,
                                                        NOTIFICATION_THERMAL_INFO, 0, 0);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_RegisterForNotification", qRes);
        }
    }

    if (!gUseQvrPerfModule) {
        if (gRenderThreadCore >= 0) {
            LOGI("Clearing Eye Render Affinity");
            svrClearThreadAffinity();
        }

        if (gEnableRenderThreadFifo) {
            L_SetThreadPriority("Render Thread", SCHED_NORMAL, gNormalPriorityRender);
        }
    }

    LOGI("Resetting CPU/GPU Performance Levels...");
    svrSetPerformanceLevelsInternal(kPerfSystem, kPerfSystem);

    if (!gAppContext->mUseIvSlam) {
        // Stop Eye Tracking
        LOGI("Stopping eye tracking...");
        qRes = QVRServiceClient_SetEyeTrackingMode(gAppContext->qvrHelper,
                                                   QVRSERVICE_EYE_TRACKING_MODE_NONE);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_SetEyeTrackingMode (Off)", qRes);
        }

        LOGI("Disconnecting from QVR Service...");
        qRes = QVRServiceClient_StopVRMode(gAppContext->qvrHelper);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_StopVRMode", qRes);
        }
    }

    if (pQueue != NULL)
        pQueue->SubmitEvent(kEventVrModeStopped, data);

    if (gAppContext->ivslamHelper != nullptr) {
        IvSlamClient_Stop(gAppContext->ivslamHelper);
    } else {
        QVRSERVICE_VRMODE_STATE state;
        state = QVRServiceClient_GetVRMode(gAppContext->qvrHelper);
        if (state != VRMODE_STOPPED) {
            switch (state) {
                case VRMODE_UNSUPPORTED:
                    LOGE("VR not stopped: Current State = VRMODE_UNSUPPORTED!");
                    break;
                case VRMODE_STARTING:
                    LOGI("VR not stopped: Current State = VRMODE_STARTING");
                    break;
                case VRMODE_STARTED:
                    LOGI("VR not stopped: Current State = VRMODE_STARTED");
                    break;
                case VRMODE_STOPPING:
                    LOGI("VR not stopped: Current State = VRMODE_STOPPING");
                    break;
                case VRMODE_STOPPED:
                    LOGI("VR not stopped: Current State = VRMODE_STOPPED");
                    break;
                default:
                    LOGE("VR not stopped: Current State = %d", (int) state);
                    break;
            }
        }
    }

    if (gUseLinePtr && gAppContext->mUseIvSlam) {
        A11GDeviceClient_Stop(gAppContext->a11GdeviceClientHelper, TYPE_OTHERS);
    }

    // Controller Manager Shutdown
    if (gAppContext->controllerManager != 0) {
        delete gAppContext->controllerManager;
        gAppContext->controllerManager = 0;
    }

    //Delete the mode context
    //We can end up here with gAppContext->modeContext set to NULL
    if (gAppContext->modeContext != NULL) {
        //Clean up our synchronization primitives
        LOGI("Cleaning up thread synchronization primitives...");
        pthread_mutex_destroy(&gAppContext->modeContext->warpThreadContextMutex);
        pthread_cond_destroy(&gAppContext->modeContext->warpThreadContextCv);
        pthread_mutex_destroy(&gAppContext->modeContext->vsyncMutex);

        //Clean up any GPU fences still hanging around
        LOGI("Cleaning up frame fences...");
        for (int i = 0; i < NUM_SWAP_FRAMES; i++) {
            svrFrameParamsInternal &fp = gAppContext->modeContext->frameParams[i];
            if (fp.frameSync != 0) {
                glDeleteSync(fp.frameSync);
            }
        }

        //Clean up the event manager
        delete gAppContext->modeContext->eventManager;

        LOGI("Deleting mode context...");
        delete gAppContext->modeContext;
        gAppContext->modeContext = NULL;
    }

    if (gEnableDebugServer) {
        svrStopDebugServer();
    }

    LOGI("VR mode ended");

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
void L_LogSubmitFrame(const svrFrameParams *pFrameParams)
//-----------------------------------------------------------------------------
{
    LOGI("****************************************");
    LOGI("svrSubmitFrame Parameters:");
    LOGI("    frameIndex = %d", pFrameParams->frameIndex);
    LOGI("    minVsyncs = %d", pFrameParams->minVsyncs);
    LOGI("    frameOptions = 0x%x", pFrameParams->frameOptions);
    LOGI("    warpType = %d", (int) pFrameParams->warpType);
    LOGI("    fieldOfView = %0.2f", pFrameParams->fieldOfView);
    LOGI("    headPoseState = Not Displayed");

    for (uint32_t whichLayer = 0; whichLayer < SVR_MAX_RENDER_LAYERS; whichLayer++) {
        if (pFrameParams->renderLayers[whichLayer].imageHandle == 0)
            continue;

        const svrRenderLayer *pOneLayer = &pFrameParams->renderLayers[whichLayer];
        LOGI("    Render Layer %d:", whichLayer);
        LOGI("        imageHandle = %d", pOneLayer->imageHandle);

        switch (pOneLayer->imageType) {
            case kTypeTexture:
                LOGI("        imageType = kTypeTexture");
                break;
            case kTypeTextureArray:
                LOGI("        imageType = kTypeTextureArray");
                break;
            case kTypeImage:
                LOGI("        imageType = kTypeImage");
                break;
            case kTypeEquiRectTexture:
                LOGI("        imageType = kTypeEquiRectTexture");
                break;
            case kTypeEquiRectImage:
                LOGI("        imageType = kTypeEquiRectImage");
                break;
            case kTypeCubemapTexture:
                LOGI("        imageType = kTypeCubemapTexture");
                break;
            case kTypeVulkan:
                LOGI("        imageType = kTypeVulkan");
                break;
            default:
                LOGI("        imageType = Unknown (%d)", pOneLayer->imageType);
                break;
        }

        LOGI("        imageCoords:");
        LOGI("             LL: [%0.2f, %0.2f, %0.2f, %0.2f]",
             pOneLayer->imageCoords.LowerLeftPos[0], pOneLayer->imageCoords.LowerLeftPos[1],
             pOneLayer->imageCoords.LowerLeftPos[2], pOneLayer->imageCoords.LowerLeftPos[3]);
        LOGI("             LR: [%0.2f, %0.2f, %0.2f, %0.2f]",
             pOneLayer->imageCoords.LowerRightPos[0], pOneLayer->imageCoords.LowerRightPos[1],
             pOneLayer->imageCoords.LowerRightPos[2], pOneLayer->imageCoords.LowerRightPos[3]);
        LOGI("             UL: [%0.2f, %0.2f, %0.2f, %0.2f]",
             pOneLayer->imageCoords.UpperLeftPos[0], pOneLayer->imageCoords.UpperLeftPos[1],
             pOneLayer->imageCoords.UpperLeftPos[2], pOneLayer->imageCoords.UpperLeftPos[3]);
        LOGI("             UR: [%0.2f, %0.2f, %0.2f, %0.2f]",
             pOneLayer->imageCoords.UpperRightPos[0], pOneLayer->imageCoords.UpperRightPos[1],
             pOneLayer->imageCoords.UpperRightPos[2], pOneLayer->imageCoords.UpperRightPos[3]);
        LOGI("            LUV: [%0.2f, %0.2f, %0.2f, %0.2f]", pOneLayer->imageCoords.LowerUVs[0],
             pOneLayer->imageCoords.LowerUVs[1], pOneLayer->imageCoords.LowerUVs[2],
             pOneLayer->imageCoords.LowerUVs[3]);
        LOGI("            UUV: [%0.2f, %0.2f, %0.2f, %0.2f]", pOneLayer->imageCoords.UpperUVs[0],
             pOneLayer->imageCoords.UpperUVs[1], pOneLayer->imageCoords.UpperUVs[2],
             pOneLayer->imageCoords.UpperUVs[3]);

        switch (pOneLayer->eyeMask) {
            case kEyeMaskLeft:
                LOGI("        eyeMask = kEyeMaskLeft");
                break;
            case kEyeMaskRight:
                LOGI("        eyeMask = kEyeMaskRight");
                break;
            case kEyeMaskBoth:
                LOGI("        eyeMask = kEyeMaskBoth");
                break;
            default:
                LOGI("        eyeMask = Unknown (0x%x)", pOneLayer->eyeMask);
                break;
        }

        // kLayerFlagNone = 0x00000000,
        // kLayerFlagHeadLocked = 0x00000001,
        // kLayerFlagOpaque = 0x00000002
        // TODO: Better way to handle this as flags are added
        if (pOneLayer->layerFlags == 0)
            LOGI("        layerFlags = kLayerFlagNone");
        else if (pOneLayer->layerFlags == kLayerFlagHeadLocked)
            LOGI("        layerFlags = kLayerFlagHeadLocked");
        else if (pOneLayer->layerFlags == kLayerFlagOpaque)
            LOGI("        layerFlags = kLayerFlagOpaque");
        else if (pOneLayer->layerFlags == kLayerFlagHeadLocked + kLayerFlagOpaque)
            LOGI("        layerFlags = kLayerFlagHeadLocked + kLayerFlagOpaque");
        else
            LOGI("        layerFlags = 0x%x", pOneLayer->layerFlags);

        if (pOneLayer->imageType == kTypeVulkan) {
            LOGI("        vulkanInfo:");
            LOGI("             memsize: %d", pOneLayer->vulkanInfo.memSize);
            LOGI("             width: %d", pOneLayer->vulkanInfo.width);
            LOGI("             height: %d", pOneLayer->vulkanInfo.height);
            LOGI("             numMips: %d", pOneLayer->vulkanInfo.numMips);
            LOGI("             bytesPerPixel: %d", pOneLayer->vulkanInfo.bytesPerPixel);
            LOGI("             renderSemaphore: %d", pOneLayer->vulkanInfo.renderSemaphore);
        }

    }   // Each Render Layer

    LOGI("****************************************");
}

//-----------------------------------------------------------------------------
SvrResult svrSubmitFrame(const svrFrameParams *pFrameParams)
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("svrSubmitFrame Failed: Called without VR initialized");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->inVrMode == false) {
        LOGE("svrSubmitFrame Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("svrSubmitFrame Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    if (gLogSubmitFrame) {
        L_LogSubmitFrame(pFrameParams);
    }

    while (gDisableFrameSubmit) {
        sleep(1);
        continue;
    }

    if (gLogSubmitFps) {
        unsigned int currentTimeMs = GetTimeNano() * 1e-6;
        gAppContext->modeContext->fpsFrameCounter++;
        if (gAppContext->modeContext->fpsPrevTimeMs == 0) {
            gAppContext->modeContext->fpsPrevTimeMs = currentTimeMs;
        }
        if (currentTimeMs - gAppContext->modeContext->fpsPrevTimeMs > 1000) {
            float elapsedSec =
                    (float) (currentTimeMs - gAppContext->modeContext->fpsPrevTimeMs) / 1000.0f;
            float currentFPS = (float) gAppContext->modeContext->fpsFrameCounter / elapsedSec;
            LOGI("FPS: %0.2f", currentFPS);

            gAppContext->modeContext->fpsFrameCounter = 0;
            gAppContext->modeContext->fpsPrevTimeMs = currentTimeMs;
        }
    }

    if (!gEnableTimeWarp) {
        // Since Timewarp has not been started, can't really wait for it to consume buffer :)
        // But put here so we can log FPS
        return SVR_ERROR_NONE;
    }

    unsigned int lastFrameCount = gAppContext->modeContext->submitFrameCount;
    unsigned int nextFrameCount = gAppContext->modeContext->submitFrameCount + 1;

    if (lastFrameCount % 100 == 0) {
        //every 100 frame,to SetPerformanceLevels to solver low hz
        svrSetPerformanceLevels(kPerfMaximum, kPerfMaximum);
    }

    if (gLogPrediction) {
        float timeSinceVrStart = L_MilliSecondsSinceVrStart();

        uint64_t timeNowNano = Svr::GetTimeNano();
        uint64_t expectedTime = pFrameParams->headPoseState.expectedDisplayTimeNs;
        uint64_t diffTimeNano = expectedTime - timeNowNano;
        float diffTimeMs = (float) diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
        // LOGI("Time Now = %llu; Expected Time = %llu; Diff Time = %llu", (long long unsigned int)timeNowNano, (long long unsigned int)expectedTime, (long long unsigned int)diffTimeNano);
        LOGI("(%0.3f ms) Fame %d submitted. Expected to be displayed in %0.2f ms ",
             timeSinceVrStart, pFrameParams->frameIndex, diffTimeMs);
    }

    svrFrameParamsInternal &fp = gAppContext->modeContext->frameParams[nextFrameCount %
                                                                       NUM_SWAP_FRAMES];
    fp.frameParams = *pFrameParams;

    if (gForceMinVsync > 0) {
        gCurrMinVSync = gForceMinVsync;
    } else {
        if (0 == gGpuBusyLastCheckTimestamp) {
            // gGpuBusyLastCheckTimestamp = fp.frameSubmitTimeStamp;
        } else if (fp.frameSubmitTimeStamp - gGpuBusyLastCheckTimestamp >=
                   gGpuBusyCheckPeriod * 1e9) {
            LOGI("svrSubmitFrame start check gpu busy rate");
            if (!gGpuBusyFStream.is_open()) {
                gGpuBusyFStream.open(QCOM_GPU_BUSY_FILE_PATH);
            }
            gGpuBusyFStream.seekg(0);
            std::string content((std::istreambuf_iterator<char>(gGpuBusyFStream)),
                                (std::istreambuf_iterator<char>()));
            char *pStart;
            char *pEnd;
            bool bError = false;
            int firstNum = strtol(content.c_str(), &pEnd, 10);
            int secondNum = 0;
            if (pEnd == content.c_str() || *pEnd == 0) {
                bError = true;
            } else {
                pStart = pEnd;
                secondNum = strtol(pStart, &pEnd, 10);
                if (pStart == pEnd) {
                    bError = true;
                }
            }
            if (!bError) {
                float rate = firstNum * 100.0f / secondNum;
                if (gMaxGpuBusyRate < rate) {
                    LOGI("svrSubmitFrame update gMaxGpuBusyRate=%f", gMaxGpuBusyRate);
                    gMaxGpuBusyRate = rate;
                }
                if (gMaxGpuBusyRate >= gGpuBusyThreshold2) {
                    gCurrMinVSync = 3;
                } else if (gMaxGpuBusyRate >= gGpuBusyThreshold1) {
                    gCurrMinVSync = 2;
                } else {
                    gCurrMinVSync = 1;
                }
                LOGI("svrSubmitFrame firstNum=%d, secondNum=%d, rate=%f, gMaxGpuBusyRate=%f gCurrMinVSync=%d",
                     firstNum, secondNum, rate, gMaxGpuBusyRate, gCurrMinVSync);
                gGpuBusyLastCheckTimestamp = fp.frameSubmitTimeStamp;
            } else {
                LOGE("svrSubmitFrame check gpu busy meet Error content=%s", content.c_str());
            }
        }
    }

    fp.frameParams.minVsyncs = gCurrMinVSync;

    // Make sure we have at least "1" here
    if (fp.frameParams.minVsyncs == 0)
        fp.frameParams.minVsyncs = 1;

    fp.externalSync = 0;
    fp.frameSubmitTimeStamp = 0;
    fp.warpFrameLeftTimeStamp = 0;
    fp.warpFrameRightTimeStamp = 0;
    fp.minVSyncCount = gAppContext->modeContext->prevSubmitVsyncCount + fp.frameParams.minVsyncs;

    if (gDisableReprojection) {
        fp.frameParams.frameOptions |= kDisableReprojection;
    }

    fp.frameSubmitTimeStamp = Svr::GetTimeNano();

    // Vulkan or GL we need to release the last one
    // If this is a Vulkan command buffer then we have a handle.
    // Determine it is Vulkan by looking at the first eyebuffer.
    if (pFrameParams->renderLayers[0].vulkanInfo.renderSemaphore != 0) {
        // This is a Vulkan frame
        fp.frameSync = 0;   // TimeWarp will handle the sync object
    } else {
        if (fp.frameSync != 0) {
            // LOGI("Deleting sync: %d...", (int)fp.frameSync);
            glDeleteSync(fp.frameSync);
        }

        // This is a GL Frame
        fp.frameSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        // LOGI("Created sync: %d", (int)fp.frameSync);

        PROFILE_ENTER(GROUP_WORLDRENDER, 0, "glFlush");
        glFlush();
        PROFILE_EXIT(GROUP_WORLDRENDER);
    }

    // This log lines up with the log in TimeWarp for which frame is actually warped
    // LOGI("DEBUG! Submitting Frame %d", nextFrameCount % NUM_SWAP_FRAMES);

    gAppContext->modeContext->submitFrameCount = nextFrameCount;
    LOGV("Submitting Frame : %d (Ring = %d) [minV=%llu curV=%llu]",
         gAppContext->modeContext->submitFrameCount,
         gAppContext->modeContext->submitFrameCount % NUM_SWAP_FRAMES, fp.minVSyncCount,
         gAppContext->modeContext->vsyncCount);

#ifdef ENABLE_MOTION_VECTORS
    // Since submitted, generate the motion vectors for this frame
    if (gEnableMotionVectors) {
        GenerateMotionData(&fp);
    }
#endif // ENABLE_MOTION_VECTORS

    PROFILE_ENTER(GROUP_WORLDRENDER, 0, "Submit Frame : %d",
                  gAppContext->modeContext->submitFrameCount);

    //Process the events for the frame
    PROFILE_ENTER(GROUP_WORLDRENDER, 0, "ProcessEvents");
    gAppContext->modeContext->eventManager->ProcessEvents();
    PROFILE_EXIT(GROUP_WORLDRENDER);

    //Wait until the eyebuffer has been consumed
    while (true) {
//        pthread_mutex_lock(&gAppContext->modeContext->warpBufferConsumedMutex);
//        pthread_cond_wait(&gAppContext->modeContext->warpBufferConsumedCv,
//                          &gAppContext->modeContext->warpBufferConsumedMutex);
//        pthread_mutex_unlock(&gAppContext->modeContext->warpBufferConsumedMutex);

        if (gAppContext->modeContext->warpFrameCount >= lastFrameCount) {
            //Make sure we maintain the minSync interval
            gAppContext->modeContext->prevSubmitVsyncCount = glm::max(
                    gAppContext->modeContext->vsyncCount,
                    gAppContext->modeContext->prevSubmitVsyncCount + fp.frameParams.minVsyncs);

            LOGV("Finished : %d [%llu]", gAppContext->modeContext->submitFrameCount,
                 gAppContext->modeContext->vsyncCount);
            break;
        }
        usleep(1000);
    }

    PROFILE_EXIT(GROUP_WORLDRENDER);

    PROFILE_TICK();

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrSetPerformanceLevels(svrPerfLevel cpuPerfLevel, svrPerfLevel gpuPerfLevel)
//-----------------------------------------------------------------------------
{
    //Currently as of 8998 LA 2.0 kPerfSystem doesn't deliver enough performance for even simple VR use cases
    //so if kPerfSystem has been specified by the app default to kPerfMedium instead
    if (cpuPerfLevel == kPerfSystem) {
        cpuPerfLevel = kPerfMedium;
    }

    if (gpuPerfLevel == kPerfSystem) {
        gpuPerfLevel = kPerfMedium;
    }

    // Apply the override here, rather than in svrSetPerformanceLevelsInternal
    cpuPerfLevel = (gForceCpuLevel < 0) ? cpuPerfLevel : (svrPerfLevel) gForceCpuLevel;
    gpuPerfLevel = (gForceGpuLevel < 0) ? gpuPerfLevel : (svrPerfLevel) gForceGpuLevel;


    return svrSetPerformanceLevelsInternal(cpuPerfLevel, gpuPerfLevel);
}

//-----------------------------------------------------------------------------
SVRP_EXPORT float svrGetPredictedDisplayTimePipelined(unsigned int depth)
//-----------------------------------------------------------------------------
{
    if (!gEnableTimeWarp) {
        // Since Timewarp has not been started, can't really get a predicted time :)
        return 0.0f;
    }

    if (gHeuristicPredictedTime) {
        return L_GetHeuristicPredictTime();
    }

    if (gAppContext == NULL || gAppContext->inVrMode == false) {
        LOGE("svrGetPredictedDisplayTime Failed: Called when not in VR mode!");
        return 0.0f;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("svrGetPredictedDisplayTime Failed: Called when not in VR mode!");
        return 0.0f;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s Failed: ivslamHelper not initialized!", __FUNCTION__);
            return 0.0f;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrGetPredictedDisplayTime Failed: SnapdragonVR not initialized!");
            return 0.0f;
        }
    }

    if (gDisablePredictedTime) {
        return 0.0f;
    }

    if (2 == gPredictVersion) {
        double framePeriodNano = 1e9 / gAppContext->deviceInfo.displayRefreshRateHz;

        pthread_mutex_lock(&gAppContext->modeContext->vsyncMutex);
        uint64_t timestamp = Svr::GetTimeNano();
        double framePct = (double) gAppContext->modeContext->vsyncCount +
                          ((double) (timestamp - gAppContext->modeContext->vsyncTimeNano) /
                           (framePeriodNano));
        pthread_mutex_unlock(&gAppContext->modeContext->vsyncMutex);

        double fractFrame = framePct - ((long) framePct);

        float predictedTime =
                (framePeriodNano - (fractFrame * framePeriodNano)) + (0.5 * framePeriodNano);
        return predictedTime;
    }

    // Need number of nanoseconds per VSync interval for later calculations
    uint64_t nanosPerVSync = 1000000000LL / (uint64_t) gAppContext->deviceInfo.displayRefreshRateHz;
    double msPerVSync = (double) nanosPerVSync * NANOSECONDS_TO_MILLISECONDS;

    // Now start building up how long until the frame should be displayed.
    // First, we have to get to the next VSync time ...
    double timeToVsync = L_MilliSecondsToNextVSync();

    // ... add in the pipeline depth (need depth + 1 so correct pose on screen the longest) ...
    double pipelineDepth = depth * msPerVSync;

    if (gDebugWithProperty) {
        char value[20] = {0x00};
        __system_property_get("svr.gTimeToHalfExposure", value);
        if ('\0' != value[0]) {
            gTimeToHalfExposure = atof(value);
        }
        __system_property_get("svr.gTimeToMidEyeWarp", value);
        if ('\0' != value[0]) {
            gTimeToMidEyeWarp = atof(value);
        }
    }

    // ... then we add time to half exposure (already in milliseconds)...
    double timeToHalfExp = gTimeToHalfExposure;

    // ... so warp is split between each eye
    double timeToMidEye = gTimeToMidEyeWarp;

    // Sum up all the times for the total
    double predictedTime = timeToVsync + pipelineDepth + timeToHalfExp + timeToMidEye;

    if (gLogPrediction) {
        float timeSinceVrStart = L_MilliSecondsSinceVrStart();
        LOGI("(%0.3f ms) Predicted Display Time: %0.2f ms (To VSync = %0.2f; Pipeline = %0.2f; Half Exposure = %0.2f; Mid Eye = %0.2f)",
             timeSinceVrStart, predictedTime, timeToVsync, pipelineDepth, timeToHalfExp,
             timeToMidEye);
    }

    // OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! 
    {
        // double framePeriodNano = 1e9 / gAppContext->deviceInfo.displayRefreshRateHz;
        // 
        // double framePct = (double)gAppContext->modeContext->vsyncCount + ((double)(timestamp - latestVsyncTimeNano) / (framePeriodNano));
        // double fractFrame = framePct - ((long)framePct);
        // 
        // // (Time left until the next vsync) + 
        // // ( pipeline depth * framePerid) + 
        // // ( fixed display latency ) + 
        // // ( 1/4 through next vysnc interval )
        // // double oldPredictedTime = MIN((framePeriodNano / 2.0), (framePeriodNano - (fractFrame * framePeriodNano))) + (depth * framePeriodNano) + (gFixedDisplayDelay * framePeriodNano) + (0.25 * framePeriodNano);
        // double oldPredictedTime = MIN((framePeriodNano / 2.0), (framePeriodNano - (fractFrame * framePeriodNano)));
        // oldPredictedTime += (depth * framePeriodNano);
        // // oldPredictedTime += (gFixedDisplayDelay * framePeriodNano);
        // oldPredictedTime += (0.25 * framePeriodNano);
        // 
        // float oldRetValue = oldPredictedTime * 1e-6;
        // 
        // LOGE("Old Predicted Time: %0.4f; New Predicted Time: %0.4f", oldRetValue, predictedTime);
    }
    // OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! OLD METHOD! 

    if (predictedTime < 0.0f) {
        LOGE("svrGetPredictedDisplayTime: Predicted display time is negative! (%0.2f ms)",
             predictedTime);
        // LOGE("       vSyncCount = %llu", gAppContext->modeContext->vsyncCount);
        // LOGE("          TimeNow = %llu", timestamp);
        // LOGE("    vSyncTimeNano = %llu", gAppContext->modeContext->vsyncTimeNano);
        // LOGE("      FramePeriod = %0.4f", framePeriodNano);
        // LOGE("         framePct = %0.4f", framePct);
        // LOGE("       fractFrame = %0.4f", fractFrame);
        predictedTime = 0.0f;
    }

    return predictedTime;
}


//-----------------------------------------------------------------------------
float svrGetPredictedDisplayTime()
//-----------------------------------------------------------------------------
{
    if (gHeuristicPredictedTime) {
        return L_GetHeuristicPredictTime();
    }

    return svrGetPredictedDisplayTimePipelined(1);
}

//-----------------------------------------------------------------------------
SvrResult svrGetVrServiceVersion(char *pRetBuffer, int bufferSize)
//-----------------------------------------------------------------------------
{
    int32_t qvrReturn;
    unsigned int retLen = 0;

    if (pRetBuffer == NULL) {
        return SVR_ERROR_UNKNOWN;
    }
    memset(pRetBuffer, 0, bufferSize);

    if (gAppContext == NULL) {
        return SVR_ERROR_UNKNOWN;
    }

    if (gAppContext->mUseIvSlam) {
        //TODO: implement api to read version
        memcpy(pRetBuffer, IVSLAM_VER, strlen(IVSLAM_VER));
        LOGE("%s, implement API to read slam version, buffer size = %d", __FUNCTION__, bufferSize);
        return SVR_ERROR_NONE;
    }

    if (gAppContext->qvrHelper == NULL) {
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    qvrReturn = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SERVICE_VERSION,
                                          &retLen, NULL);
    if (qvrReturn != QVR_SUCCESS) {
        L_LogQvrError("svrGetVrServiceVersion(Length)", qvrReturn);

        SvrResult retCode = SVR_ERROR_UNKNOWN;
        switch (qvrReturn) {
            case QVR_ERROR:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            case QVR_CALLBACK_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_API_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_INVALID_PARAM:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            default:
                retCode = SVR_ERROR_UNKNOWN;
                break;
        }

        return retCode;
    }

    // Will this fit in the return buffer?
    if ((int) retLen >= bufferSize - 1) {
        LOGE("svrGetVrServiceVersion passed return buffer that is too small! Need %d bytes, buffer is %d bytes.",
             retLen + 1, bufferSize);
        return SVR_ERROR_UNKNOWN;
    }


    qvrReturn = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SERVICE_VERSION,
                                          &retLen, pRetBuffer);
    if (qvrReturn != QVR_SUCCESS) {
        L_LogQvrError("svrGetVrServiceVersion()", qvrReturn);

        SvrResult retCode = SVR_ERROR_UNKNOWN;
        switch (qvrReturn) {
            case QVR_ERROR:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            case QVR_CALLBACK_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_API_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_INVALID_PARAM:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            default:
                retCode = SVR_ERROR_UNKNOWN;
                break;
        }

        return retCode;
    }

    // Read the string correctly
    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrGetVrClientVersion(char *pRetBuffer, int bufferSize)
//-----------------------------------------------------------------------------
{
    int32_t qvrReturn;
    unsigned int retLen = 0;

    if (pRetBuffer == NULL) {
        return SVR_ERROR_UNKNOWN;
    }
    memset(pRetBuffer, 0, bufferSize);

    if (gAppContext == NULL) {
        return SVR_ERROR_UNKNOWN;
    }

    if (gAppContext->mUseIvSlam) {
        //TODO: implement api to read version
        memcpy(pRetBuffer, IVSLAM_VER, strlen(IVSLAM_VER));
        LOGE("%s, implement API to read slam version, buffer size = %d", __FUNCTION__, bufferSize);
        return SVR_ERROR_NONE;
    }

    if (gAppContext->qvrHelper == NULL) {
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    qvrReturn = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_CLIENT_VERSION,
                                          &retLen, NULL);
    if (qvrReturn != QVR_SUCCESS) {
        L_LogQvrError("svrGetVrClientVersion(Length)", qvrReturn);

        SvrResult retCode = SVR_ERROR_UNKNOWN;
        switch (qvrReturn) {
            case QVR_ERROR:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            case QVR_CALLBACK_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_API_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_INVALID_PARAM:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            default:
                retCode = SVR_ERROR_UNKNOWN;
                break;
        }

        return retCode;
    }

    // Will this fit in the return buffer?
    if ((int) retLen >= bufferSize - 1) {
        LOGE("svrGetVrClientVersion passed return buffer that is too small! Need %d bytes, buffer is %d bytes.",
             retLen + 1, bufferSize);
        return SVR_ERROR_UNKNOWN;
    }


    qvrReturn = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_CLIENT_VERSION,
                                          &retLen, pRetBuffer);
    if (qvrReturn != QVR_SUCCESS) {
        L_LogQvrError("svrGetVrClientVersion()", qvrReturn);

        SvrResult retCode = SVR_ERROR_UNKNOWN;
        switch (qvrReturn) {
            case QVR_ERROR:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            case QVR_CALLBACK_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_API_NOT_SUPPORTED:
                retCode = SVR_ERROR_UNSUPPORTED;
                break;
            case QVR_INVALID_PARAM:
                retCode = SVR_ERROR_UNKNOWN;
                break;
            default:
                retCode = SVR_ERROR_UNKNOWN;
                break;
        }

        return retCode;
    }

    // Read the string correctly
    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
void L_SetPoseState(svrHeadPoseState &poseState, float predictedTimeMs)
//-----------------------------------------------------------------------------
{
    // Need glm versions for math
    glm::fquat poseRot;
    glm::vec3 posePos;

    poseRot.x = poseState.pose.rotation.x;
    poseRot.y = poseState.pose.rotation.y;
    poseRot.z = poseState.pose.rotation.z;
    poseRot.w = poseState.pose.rotation.w;

    posePos.x = poseState.pose.position.x;
    posePos.y = poseState.pose.position.y;
    posePos.z = poseState.pose.position.z;

    // Adjust by the recenter value
    poseRot = poseRot * gAppContext->modeContext->recenterRot;

    if (gAppContext->currentTrackingMode & kTrackingPosition) {
        // If no actual 6DOF camera the positions come back as NaN
        posePos = posePos - gAppContext->modeContext->recenterPos;

        // Need to adjust this new position by the rotation correction
        posePos = posePos * gAppContext->modeContext->recenterRot;
    }

    // Come back out of glm
    poseState.pose.rotation.x = poseRot.x;
    poseState.pose.rotation.y = poseRot.y;
    poseState.pose.rotation.z = poseRot.z;
    poseState.pose.rotation.w = poseRot.w;

    poseState.pose.position.x = posePos.x;
    poseState.pose.position.y = posePos.y;
    poseState.pose.position.z = posePos.z;

    // Don't set this since the time is already set in the structure...
    //poseState.poseTimeStampNs = Svr::GetTimeNano();

    // ... however, we need to adjust for DSP time
    poseState.poseTimeStampNs -= gQTimeToAndroidBoot;

    // When did we fetch this pose
    uint64_t timeNowNano = Svr::GetTimeNano();
    poseState.poseFetchTimeNs = timeNowNano;

    // When do we expect this pose to show up on the display
    poseState.expectedDisplayTimeNs = 0;
    if (!gDisablePredictedTime && predictedTimeMs > 0.0f) {
        poseState.expectedDisplayTimeNs =
                timeNowNano + (uint64_t) (predictedTimeMs * MILLISECONDS_TO_NANOSECONDS);
    }
}

svrHeadPoseState svrGetPredictedHeadPose(float predictedTimeMs){
    return svrGetPredictedHeadPoseExt(predictedTimeMs, false);
}

//-----------------------------------------------------------------------------
svrHeadPoseState svrGetPredictedHeadPoseExt(float predictedTimeMs, bool virHandGesture)
//-----------------------------------------------------------------------------
{
    svrHeadPoseState poseState;

    glm::fquat poseRot;     // Identity
    glm::vec3 posePos;     // Identity

    poseState.poseStatus = 0;

    poseState.pose.rotation.x = poseRot.x;
    poseState.pose.rotation.y = poseRot.y;
    poseState.pose.rotation.z = poseRot.z;
    poseState.pose.rotation.w = poseRot.w;

    poseState.pose.position.x = posePos.x;
    poseState.pose.position.y = posePos.y;
    poseState.pose.position.z = posePos.z;

    if (gAppContext == NULL) {
        LOGE("svrGetPredictedHeadPose Failed: SnapdragonVR not initialized!");
        return poseState;
    }

    if (gAppContext == NULL || gAppContext->inVrMode == false) {
        LOGE("svrGetPredictedHeadPose Failed: Called when not in VR mode!");
        return poseState;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("svrGetPredictedHeadPose Failed: Called when not in VR mode!");
        return poseState;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s Failed: ivslamHelper was not initialized!", __FUNCTION__);
            return poseState;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrGetPredictedHeadPose Failed: QVR service not initialized!");
            return poseState;
        }
    }

    uint64_t sampleTimeStamp;

    // Set the pose status and let it be changed by quality settings
    poseState.poseStatus = 0;
    if ((gAppContext->currentTrackingMode & kTrackingRotation) != 0)
        poseState.poseStatus |= kTrackingRotation;
    if ((gAppContext->currentTrackingMode & kTrackingPosition) != 0)
        poseState.poseStatus |= kTrackingPosition;

    if (SVR_ERROR_NONE !=
        GetTrackingFromPredictiveSensorExt(predictedTimeMs, &sampleTimeStamp, poseState,virHandGesture)) {
        // LOGE("svrGetPredictedHeadPose Failed: QVR service not initialized!");
        poseState.poseStatus = 0;
        return poseState;
    }

    if (gAppContext->useTransformMatrix) {
        glm::quat oriQuat{poseState.pose.rotation.w, poseState.pose.rotation.x,
                          poseState.pose.rotation.y, poseState.pose.rotation.z};
        glm::quat newQuat = gAppContext->correctYQuat * oriQuat;
        poseState.pose.rotation.w = newQuat.w;
        poseState.pose.rotation.x = newQuat.x;
        poseState.pose.rotation.y = newQuat.y;
        poseState.pose.rotation.z = newQuat.z;
    }
    // Assign to return value after adjusting for recenter
    L_SetPoseState(poseState, predictedTimeMs);


    if (gAppContext->modeContext->prevPoseState.poseTimeStampNs == 0) {
        //First time through
        gAppContext->modeContext->prevPoseState = poseState;
    } else {
        glm::fquat prevPoseQuat = glm::fquat(
                gAppContext->modeContext->prevPoseState.pose.rotation.w,
                gAppContext->modeContext->prevPoseState.pose.rotation.x,
                gAppContext->modeContext->prevPoseState.pose.rotation.y,
                gAppContext->modeContext->prevPoseState.pose.rotation.z);
        glm::fquat poseQuat = glm::fquat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                         poseState.pose.rotation.y, poseState.pose.rotation.z);
        poseQuat = glm::normalize(poseQuat);
        glm::fquat inversePrev = glm::conjugate(prevPoseQuat);
        glm::fquat diffValue = poseQuat * inversePrev;
        float diffRad = acosf(diffValue.w) * 2.0f;

        if (isnan(diffRad) || isinf(diffRad)) {
            diffRad = 0.0f;
        }

        float diffTimeSeconds = ((poseState.poseTimeStampNs -
                                  gAppContext->modeContext->prevPoseState.poseTimeStampNs) * 1e-9);
        float angVelocity = 0.0f;
        float linVelocity = 0.0f;

        if (diffTimeSeconds > 0.0f) {
            //Angular velocity (degrees per second)
            angVelocity = (diffRad * RAD_TO_DEG) / diffTimeSeconds;

            //Positional velocity
            glm::vec3 prevToCurVec = glm::vec3(poseState.pose.position.x -
                                               gAppContext->modeContext->prevPoseState.pose.position.x,
                                               poseState.pose.position.y -
                                               gAppContext->modeContext->prevPoseState.pose.position.y,
                                               poseState.pose.position.z -
                                               gAppContext->modeContext->prevPoseState.pose.position.z);
            float length = glm::length(prevToCurVec);
            linVelocity = length / diffTimeSeconds;
        }

        if (gLogPoseVelocity) {
            LOGI("Ang Vel %f (deg/s): Lin Vel : %f (m/s)", angVelocity, linVelocity);
        }

        if (gLogPoseVelocity && angVelocity > gMaxAngVel) {
            LOGE("svrGetPredictedHeadPose, angular velocity exceeded max of %f : %f", gMaxAngVel,
                 angVelocity);
        }

        if (gLogPoseVelocity && linVelocity > gMaxLinearVel) {
            LOGE("svrGetPredictedHeadPose, linear velocity exceeded max of %f : %f", gMaxLinearVel,
                 linVelocity);
        }


        gAppContext->modeContext->prevPoseState = poseState;
    }

    return poseState;
}

//-----------------------------------------------------------------------------
svrHeadPoseState svrGetHistoricHeadPose(int64_t timestampNs, bool bLoadCalibration)
//-----------------------------------------------------------------------------
{
    // Must adjust for gQTimeToAndroidBoot
    timestampNs += gQTimeToAndroidBoot;

    svrHeadPoseState poseState;

    glm::fquat poseRot;     // Identity
    glm::vec3 posePos;     // Identity

    // Start with the pose status of what they expect, then correct later based on quality
    poseState.poseStatus = 0;
    if ((gAppContext->currentTrackingMode & kTrackingRotation) != 0)
        poseState.poseStatus |= kTrackingRotation;
    if ((gAppContext->currentTrackingMode & kTrackingPosition) != 0)
        poseState.poseStatus |= kTrackingPosition;

    poseState.pose.rotation.x = poseRot.x;
    poseState.pose.rotation.y = poseRot.y;
    poseState.pose.rotation.z = poseRot.z;
    poseState.pose.rotation.w = poseRot.w;

    poseState.pose.position.x = posePos.x;
    poseState.pose.position.y = posePos.y;
    poseState.pose.position.z = posePos.z;

    if (gAppContext == NULL) {
        LOGE("svrGetHistoricHeadPose Failed: SnapdragonVR not initialized!");
        poseState.poseStatus = 0;
        return poseState;
    }

    if (gAppContext == NULL || gAppContext->inVrMode == false) {
        LOGE("svrGetHistoricHeadPose Failed: Called when not in VR mode!");
        poseState.poseStatus = 0;
        return poseState;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("svrGetHistoricHeadPose Failed: Called when not in VR mode!");
        poseState.poseStatus = 0;
        return poseState;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("svrGetHistoricHeadPose() not supported on this device! The ivslamHelper was invalid");
            poseState.poseStatus = 0;
            return poseState;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrGetHistoricHeadPose() not supported on this device!");
            poseState.poseStatus = 0;
            return poseState;
        }
    }

    if (SVR_ERROR_NONE != GetTrackingFromHistoricSensor(timestampNs, poseState, bLoadCalibration)) {
        LOGE("svrGetHistoricHeadPose Failed: Unknown!");
        poseState.poseStatus = 0;
        return poseState;
    }

    // Assign to return value after adjusting for recenter
    L_SetPoseState(poseState, 0.0f);

    return poseState;
}

int svrGetPointCloudData(int &outNum, uint64_t &outTimestamp, float *outDataArray) {

    if (gAppContext == NULL) {
        LOGE("svrGetPointCloudData Failed: SnapdragonVR not initialized!");
        return 0;
    }

    if (gAppContext == NULL || gAppContext->inVrMode == false) {
        LOGE("svrGetPointCloudData Failed: Called when not in VR mode!");
        return 0;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("svrGetPointCloudData Failed: Called when not in VR mode!");
        return 0;
    }

    if (gAppContext->mUseIvSlam) {
        //TODO: implement GetPointCloud?
        LOGE("%s: The GetPointCloud has not been implemented", __FUNCTION__);
        return 0;
    }

    if (gAppContext->qvrHelper == NULL) {
        LOGE("svrGetPointCloudData Failed: QVR service not initialized!");
        return 0;
    }

    XrPointCloudQTI *rawData = nullptr;
    int res = QVRServiceClient_GetPointCloud_Exp(gAppContext->qvrHelper,
                                                 &rawData);
    if (SVR_ERROR_NONE != res) {
        LOGE("svrGetPointCloudData Failed: res=%d ", res);
        return 0;
    }
    if (rawData->num_points == 0) {
        LOGE("svrGetPointCloudData Failed: rawData->num_point=0");
        return 0;
    }
    outNum = rawData->num_points;
    outTimestamp = rawData->timestamp;
    // LOGI("svrGetPointCloudData num_point=%u, timestamp=%"
    // PRIu64
    // "", outNum, outTimestamp);
    uint fixNum = outNum > 10000 ? 10000 : outNum;
    outNum = fixNum;
    for (uint idx = 0; idx != fixNum; ++idx) {
        outDataArray[idx * 3 + 0] = rawData->points[idx].position.x;
        outDataArray[idx * 3 + 1] = rawData->points[idx].position.y;
        outDataArray[idx * 3 + 2] = rawData->points[idx].position.z;
        LOGI("TEST svrGetPointCloudData No.%u point=[%f, %f, %f]", idx, outDataArray[idx * 3 + 0],
             outDataArray[idx * 3 + 1], outDataArray[idx * 3 + 2]);
    }
    return outNum;
}


//-----------------------------------------------------------------------------
SvrResult svrGetEyePose(svrEyePoseState *pReturnPose)
//-----------------------------------------------------------------------------
{
    // Clear out return in case we leave early
    memset(pReturnPose, 0, sizeof(svrEyePoseState));

    if (pReturnPose == NULL) {
        LOGE("svrGetEyePose Failed: Called with NULL parameter");
        return SVR_ERROR_UNKNOWN;
    }

    if (gAppContext == NULL) {
        LOGE("svrGetEyePose Failed: SnapdragonVR not initialized!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext == NULL || gAppContext->inVrMode == false) {
        LOGE("svrGetEyePose Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("svrGetEyePose Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->mUseIvSlam) {
        //LOGE("svrGetEyePose() not supported on this device!");
        return SVR_ERROR_UNSUPPORTED;
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrGetEyePose() not supported on this device!");
            return SVR_ERROR_UNSUPPORTED;
        }
    }

    if ((gAppContext->currentTrackingMode & kTrackingEye) == 0) {
        LOGE("svrGetEyePose Failed: Eye tracking has not been enabled!");
        return SVR_ERROR_UNSUPPORTED;
    }

    if (gUseFoveatWithoutEyeTracking) {

        pReturnPose->leftEyePoseStatus |=
                kGazePointValid | kGazeVectorValid | kEyeOpennessValid | kEyePupilDilationValid |
                kEyePositionGuideValid;
        pReturnPose->rightEyePoseStatus |=
                kGazePointValid | kGazeVectorValid | kEyeOpennessValid | kEyePupilDilationValid |
                kEyePositionGuideValid;
        pReturnPose->combinedEyePoseStatus |= kGazePointValid | kGazeVectorValid;

        // Set base point for left, right, and combined (meters)
        pReturnPose->leftEyeGazePoint[0] = 0;
        pReturnPose->leftEyeGazePoint[1] = 0;
        pReturnPose->leftEyeGazePoint[2] = -1.0f;

        pReturnPose->rightEyeGazePoint[0] = 0;
        pReturnPose->rightEyeGazePoint[1] = 0;
        pReturnPose->rightEyeGazePoint[2] = -1.0f;

        pReturnPose->combinedEyeGazePoint[0] = 0;
        pReturnPose->combinedEyeGazePoint[1] = 0;
        pReturnPose->combinedEyeGazePoint[2] = -1.0f;

        // Set direction vector for left, right, and combined
        pReturnPose->leftEyeGazeVector[0] = 0;
        pReturnPose->leftEyeGazeVector[1] = 0;
        pReturnPose->leftEyeGazeVector[2] = -1.0f;

        pReturnPose->rightEyeGazeVector[0] = 0;
        pReturnPose->rightEyeGazeVector[1] = 0;
        pReturnPose->rightEyeGazeVector[2] = -1.0f;

        pReturnPose->combinedEyeGazeVector[0] = 0;
        pReturnPose->combinedEyeGazeVector[1] = 0;
        pReturnPose->combinedEyeGazeVector[2] = -1.0f;

        // Set the left, right position guides
        pReturnPose->leftEyePositionGuide[0] = 0;
        pReturnPose->leftEyePositionGuide[1] = 0;
        pReturnPose->leftEyePositionGuide[2] = -1.0f;

        pReturnPose->rightEyePositionGuide[0] = 0;
        pReturnPose->rightEyePositionGuide[1] = 0;
        pReturnPose->rightEyePositionGuide[2] = -1.0f;

        // Set the openness
        pReturnPose->leftEyeOpenness = 1.0f;
        pReturnPose->rightEyeOpenness = 1.0f;

        // Calibration (temporary)
        pReturnPose->leftEyeGazeVector[0] =
                pReturnPose->leftEyeGazeVector[0] * gSensorEyeScaleX + gSensorEyeOffsetX;
        pReturnPose->leftEyeGazeVector[1] =
                pReturnPose->leftEyeGazeVector[1] * gSensorEyeScaleY + gSensorEyeOffsetY;

        pReturnPose->rightEyeGazeVector[0] =
                pReturnPose->rightEyeGazeVector[0] * gSensorEyeScaleX + gSensorEyeOffsetX;
        pReturnPose->rightEyeGazeVector[1] =
                pReturnPose->rightEyeGazeVector[1] * gSensorEyeScaleY + gSensorEyeOffsetY;

        pReturnPose->combinedEyeGazeVector[0] =
                pReturnPose->combinedEyeGazeVector[0] * gSensorEyeScaleX + gSensorEyeOffsetX;
        pReturnPose->combinedEyeGazeVector[1] =
                pReturnPose->combinedEyeGazeVector[1] * gSensorEyeScaleY + gSensorEyeOffsetY;
        return SVR_ERROR_NONE;
    }
    // Finally, we can get the actual eye pose
    qvrservice_eye_tracking_data_t *qvr_eye_pose;
    int32_t qvrReturn = QVRServiceClient_GetEyeTrackingData(gAppContext->qvrHelper, &qvr_eye_pose,
                                                            0);
    if (qvrReturn != QVR_SUCCESS) {
        switch (qvrReturn) {
            case QVR_CALLBACK_NOT_SUPPORTED:
                LOGE("Error from QVRServiceClient_GetEyeTrackingData: QVR_CALLBACK_NOT_SUPPORTED");
                return SVR_ERROR_UNSUPPORTED;

            case QVR_API_NOT_SUPPORTED:
                LOGE("Error from QVRServiceClient_GetEyeTrackingData: QVR_API_NOT_SUPPORTED");
                return SVR_ERROR_UNSUPPORTED;

            case QVR_INVALID_PARAM:
                LOGE("Error from QVRServiceClient_GetEyeTrackingData: QVR_INVALID_PARAM");
                return SVR_ERROR_UNKNOWN;

            default:
                LOGE("Error from QVRServiceClient_GetEyeTrackingData: Unknown = %d", qvrReturn);
                return SVR_ERROR_UNKNOWN;
        }
    }

    memset(pReturnPose, 0, sizeof(svrEyePoseState));

    // Set the status values
    pReturnPose->leftEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_LEFT].flags &
                                       QVR_GAZE_PER_EYE_GAZE_ORIGIN_VALID) ? kGazePointValid : 0;
    pReturnPose->leftEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_LEFT].flags &
                                       QVR_GAZE_PER_EYE_GAZE_DIRECTION_VALID) ? kGazeVectorValid
                                                                              : 0;
    pReturnPose->leftEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_LEFT].flags &
                                       QVR_GAZE_PER_EYE_EYE_OPENNESS_VALID) ? kEyeOpennessValid : 0;
    pReturnPose->leftEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_LEFT].flags &
                                       QVR_GAZE_PER_EYE_PUPIL_DILATION_VALID)
                                      ? kEyePupilDilationValid : 0;
    pReturnPose->leftEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_LEFT].flags &
                                       QVR_GAZE_PER_EYE_POSITION_GUIDE_VALID)
                                      ? kEyePositionGuideValid : 0;

    pReturnPose->rightEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_RIGHT].flags &
                                        QVR_GAZE_PER_EYE_GAZE_ORIGIN_VALID) ? kGazePointValid : 0;
    pReturnPose->rightEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_RIGHT].flags &
                                        QVR_GAZE_PER_EYE_GAZE_DIRECTION_VALID) ? kGazeVectorValid
                                                                               : 0;
    pReturnPose->rightEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_RIGHT].flags &
                                        QVR_GAZE_PER_EYE_EYE_OPENNESS_VALID) ? kEyeOpennessValid
                                                                             : 0;
    pReturnPose->rightEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_RIGHT].flags &
                                        QVR_GAZE_PER_EYE_PUPIL_DILATION_VALID)
                                       ? kEyePupilDilationValid : 0;
    pReturnPose->rightEyePoseStatus |= (qvr_eye_pose->eye[QVR_EYE_RIGHT].flags &
                                        QVR_GAZE_PER_EYE_POSITION_GUIDE_VALID)
                                       ? kEyePositionGuideValid : 0;

    pReturnPose->combinedEyePoseStatus |= (qvr_eye_pose->flags & QVR_GAZE_ORIGIN_COMBINED_VALID)
                                          ? kGazePointValid : 0;
    pReturnPose->combinedEyePoseStatus |= (qvr_eye_pose->flags & QVR_GAZE_DIRECTION_COMBINED_VALID)
                                          ? kGazeVectorValid : 0;

    // Set base point for left, right, and combined (meters)
    pReturnPose->leftEyeGazePoint[0] = qvr_eye_pose->eye[QVR_EYE_LEFT].gazeOrigin[0] * 0.001f;
    pReturnPose->leftEyeGazePoint[1] = qvr_eye_pose->eye[QVR_EYE_LEFT].gazeOrigin[1] * 0.001f;
    pReturnPose->leftEyeGazePoint[2] = qvr_eye_pose->eye[QVR_EYE_LEFT].gazeOrigin[2] * 0.001f;

    pReturnPose->rightEyeGazePoint[0] = qvr_eye_pose->eye[QVR_EYE_RIGHT].gazeOrigin[0] * 0.001f;
    pReturnPose->rightEyeGazePoint[1] = qvr_eye_pose->eye[QVR_EYE_RIGHT].gazeOrigin[1] * 0.001f;
    pReturnPose->rightEyeGazePoint[2] = qvr_eye_pose->eye[QVR_EYE_RIGHT].gazeOrigin[2] * 0.001f;

    pReturnPose->combinedEyeGazePoint[0] = qvr_eye_pose->gazeOriginCombined[0] * 0.001f;
    pReturnPose->combinedEyeGazePoint[1] = qvr_eye_pose->gazeOriginCombined[1] * 0.001f;
    pReturnPose->combinedEyeGazePoint[2] = qvr_eye_pose->gazeOriginCombined[2] * 0.001f;

    // Set direction vector for left, right, and combined
    pReturnPose->leftEyeGazeVector[0] = qvr_eye_pose->eye[QVR_EYE_LEFT].gazeDirection[0];
    pReturnPose->leftEyeGazeVector[1] = qvr_eye_pose->eye[QVR_EYE_LEFT].gazeDirection[1];
    pReturnPose->leftEyeGazeVector[2] = qvr_eye_pose->eye[QVR_EYE_LEFT].gazeDirection[2];

    pReturnPose->rightEyeGazeVector[0] = qvr_eye_pose->eye[QVR_EYE_RIGHT].gazeDirection[0];
    pReturnPose->rightEyeGazeVector[1] = qvr_eye_pose->eye[QVR_EYE_RIGHT].gazeDirection[1];
    pReturnPose->rightEyeGazeVector[2] = qvr_eye_pose->eye[QVR_EYE_RIGHT].gazeDirection[2];

    pReturnPose->combinedEyeGazeVector[0] = qvr_eye_pose->gazeDirectionCombined[0];
    pReturnPose->combinedEyeGazeVector[1] = qvr_eye_pose->gazeDirectionCombined[1];
    pReturnPose->combinedEyeGazeVector[2] = qvr_eye_pose->gazeDirectionCombined[2];

    // Set the left, right position guides
    pReturnPose->leftEyePositionGuide[0] = qvr_eye_pose->eye[QVR_EYE_LEFT].positionGuide[0];
    pReturnPose->leftEyePositionGuide[1] = qvr_eye_pose->eye[QVR_EYE_LEFT].positionGuide[1];
    pReturnPose->leftEyePositionGuide[2] = qvr_eye_pose->eye[QVR_EYE_LEFT].positionGuide[2];

    pReturnPose->rightEyePositionGuide[0] = qvr_eye_pose->eye[QVR_EYE_RIGHT].positionGuide[0];
    pReturnPose->rightEyePositionGuide[1] = qvr_eye_pose->eye[QVR_EYE_RIGHT].positionGuide[1];
    pReturnPose->rightEyePositionGuide[2] = qvr_eye_pose->eye[QVR_EYE_RIGHT].positionGuide[2];

    // Set the openness
    pReturnPose->leftEyeOpenness = qvr_eye_pose->eye[QVR_EYE_LEFT].eyeOpenness;
    pReturnPose->rightEyeOpenness = qvr_eye_pose->eye[QVR_EYE_RIGHT].eyeOpenness;

    // Calibration (temporary)
    pReturnPose->leftEyeGazeVector[0] =
            pReturnPose->leftEyeGazeVector[0] * gSensorEyeScaleX + gSensorEyeOffsetX;
    pReturnPose->leftEyeGazeVector[1] =
            pReturnPose->leftEyeGazeVector[1] * gSensorEyeScaleY + gSensorEyeOffsetY;

    pReturnPose->rightEyeGazeVector[0] =
            pReturnPose->rightEyeGazeVector[0] * gSensorEyeScaleX + gSensorEyeOffsetX;
    pReturnPose->rightEyeGazeVector[1] =
            pReturnPose->rightEyeGazeVector[1] * gSensorEyeScaleY + gSensorEyeOffsetY;

    pReturnPose->combinedEyeGazeVector[0] =
            pReturnPose->combinedEyeGazeVector[0] * gSensorEyeScaleX + gSensorEyeOffsetX;
    pReturnPose->combinedEyeGazeVector[1] =
            pReturnPose->combinedEyeGazeVector[1] * gSensorEyeScaleY + gSensorEyeOffsetY;

    return SVR_ERROR_NONE;
}


#ifdef POINT_CLOUD_SUPPORT
// Right now the number of points returned can change every time.
// There is no way to get the maximum points that can come back
#define MAX_CLOUD_POINTS    10000
float gPointCloudData[3 * MAX_CLOUD_POINTS];

//-----------------------------------------------------------------------------
SvrResult svrGetPointCloudData(uint32_t *pNumPoints, float **pData)
//-----------------------------------------------------------------------------
{
    // Start with clean return values in case it fails later
    *pNumPoints = 0;
    *pData = NULL;
    memset(gPointCloudData, 0, 3 * MAX_CLOUD_POINTS * sizeof(float));


    if (gAppContext == NULL)
    {
        LOGE("svrGetPointCloudData Failed: SnapdragonVR not initialized!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    if (gAppContext == NULL || gAppContext->inVrMode == false)
    {
        LOGE("svrGetPointCloudData Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    if (gAppContext->modeContext == NULL)
    {
        LOGE("svrGetPointCloudData Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED; 
    }

    if (gAppContext->qvrHelper == NULL)
    {
        LOGE("svrGetPointCloudData() not supported on this device!");
        return SVR_ERROR_UNSUPPORTED;
    }

    int32_t qvrReturn;

    // Local variables to get data from the service
    int pointCount = 0;
    float* pPoints = NULL;
    qvrReturn = QVRServiceClient_GetPointCloud(gAppContext->qvrHelper, &pointCount, &pPoints);
    if (qvrReturn != QVR_SUCCESS)
    {
        switch (qvrReturn)
        {
        case QVR_CALLBACK_NOT_SUPPORTED:
            LOGE("Error from QVRServiceClient_GetPointCloud: QVR_CALLBACK_NOT_SUPPORTED");
            return SVR_ERROR_UNSUPPORTED;
            break;

        case QVR_API_NOT_SUPPORTED:
            LOGE("Error from QVRServiceClient_GetPointCloud: QVR_API_NOT_SUPPORTED");
            return SVR_ERROR_UNSUPPORTED;
            break;

        case QVR_INVALID_PARAM:
            LOGE("Error from QVRServiceClient_GetPointCloud: QVR_INVALID_PARAM");
            return SVR_ERROR_UNKNOWN;
            break;

        default:
            LOGE("Error from QVRServiceClient_GetPointCloud: Unknown = %d", qvrReturn);
            break;
        }

        return SVR_ERROR_UNKNOWN;
    }

    // We now how a pointer to QVR memory.  We need to handle rotation to get into correct mode and then
    // move data into actual return buffer.
    if (pointCount > MAX_CLOUD_POINTS)
    {
        LOGE("QVR returned %d points, but SVR only supports %d points!", pointCount, MAX_CLOUD_POINTS);
        pointCount = MAX_CLOUD_POINTS;
    }
    for(int iIndx = 0; iIndx < pointCount; ++iIndx)
    {
        // For example:
        float x = -pPoints[3* iIndx + 1];
        float y =  pPoints[3* iIndx + 0];
        float z =  pPoints[3* iIndx + 2];

        gPointCloudData[3* iIndx + 1] = x;
        gPointCloudData[3* iIndx + 0] = y;
        gPointCloudData[3* iIndx + 2] = z;
    }

    // Set the return values
    LOGE("QVRServiceClient_GetPointCloud() returned %d points", pointCount);
    *pNumPoints = (uint32_t) pointCount;
    *pData = gPointCloudData;

    return SVR_ERROR_NONE;
}
#endif // POINT_CLOUD_SUPPORT


//-----------------------------------------------------------------------------
SvrResult svrRecenterPose()
//-----------------------------------------------------------------------------
{
    SvrResult svrResult;

    if (gAppContext == NULL) {
        LOGE("svrRecenterPose Failed: SnapdragonVR not initialized!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    // Recenter both position and orientation
    if (gAppContext->currentTrackingMode & kTrackingPosition) {
        svrResult = svrRecenterPosition();
        if (svrResult != SVR_ERROR_NONE)
            return svrResult;
    }

    svrResult = svrRecenterOrientation();
    if (svrResult != SVR_ERROR_NONE)
        return svrResult;

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrRecenterPosition()
//-----------------------------------------------------------------------------
{
    // We want to disable reprojection for a few frames after recenter has been called
    gRecenterTransition = 0;

    float predictedTimeMs = 0.0f;

    if (gAppContext == NULL || gAppContext->inVrMode == false || gAppContext->modeContext == NULL) {
        LOGE("svrRecenterPosition Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s Failed: ivslamHelper was not initialized!", __FUNCTION__);
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrRecenterPosition Failed: SnapdragonVR not initialized!");
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    }

    LOGI("Recentering Position");

    svrHeadPoseState poseState;
    glm::fquat poseRot;     // Identity
    glm::vec3 posePos;     // Identity

    // Set the pose status and let it be changed by quality settings
    poseState.pose.rotation.x = poseRot.x;
    poseState.pose.rotation.y = poseRot.y;
    poseState.pose.rotation.z = poseRot.z;
    poseState.pose.rotation.w = poseRot.w;
    poseState.pose.position.x = posePos.x;
    poseState.pose.position.y = posePos.y;
    poseState.pose.position.z = posePos.z;
    poseState.poseStatus = gAppContext->currentTrackingMode;

    uint64_t sampleTimeStamp;
    SvrResult resultCode = GetTrackingFromPredictiveSensor(predictedTimeMs, &sampleTimeStamp,
                                                           poseState);
    if (SVR_ERROR_NONE != resultCode) {
        poseState.poseStatus = 0;
        return resultCode;
    }

    if (poseState.poseStatus & kTrackingPosition) {
        gAppContext->modeContext->recenterPos.x = poseState.pose.position.x;
        gAppContext->modeContext->recenterPos.y = poseState.pose.position.y;
        gAppContext->modeContext->recenterPos.z = poseState.pose.position.z;
    } else {
        LOGE("svrRecenterPosition Failed: Pose quality too low! (Pose Status = %d)",
             poseState.poseStatus);
        return SVR_ERROR_UNKNOWN;
    }

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SvrResult svrRecenterOrientation(bool yawOnly)
//-----------------------------------------------------------------------------
{
    // We want to disable reprojection for a few frames after recenter has been called
    gRecenterTransition = 0;

    float predictedTimeMs = 0.0f;

    if (gAppContext == NULL || gAppContext->inVrMode == false || gAppContext->modeContext == NULL) {
        LOGE("svrRecenterOrientation Failed: Called when not in VR mode!");
        return SVR_ERROR_VRMODE_NOT_STARTED;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s Failed: ivslamHelper was not initialized!", __FUNCTION__);
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrRecenterOrientation Failed: SnapdragonVR not initialized!");
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    }

    LOGI("Recentering Orientation");

    svrHeadPoseState poseState;
    glm::fquat poseRot;     // Identity
    glm::vec3 posePos;     // Identity

    // Set the pose status and let it be changed by quality settings
    poseState.pose.rotation.x = poseRot.x;
    poseState.pose.rotation.y = poseRot.y;
    poseState.pose.rotation.z = poseRot.z;
    poseState.pose.rotation.w = poseRot.w;
    poseState.pose.position.x = posePos.x;
    poseState.pose.position.y = posePos.y;
    poseState.pose.position.z = posePos.z;
    poseState.poseStatus = gAppContext->currentTrackingMode;

    uint64_t sampleTimeStamp;
    SvrResult resultCode = GetTrackingFromPredictiveSensor(predictedTimeMs, &sampleTimeStamp,
                                                           poseState);
    if (SVR_ERROR_NONE != resultCode) {
        poseState.poseStatus = 0;
        return resultCode;
    }

    if (!(poseState.poseStatus & kTrackingRotation)) {
        LOGE("svrRecenterOrientation Failed: Pose quality too low! (Pose Status = %d)",
             poseState.poseStatus);
        return SVR_ERROR_UNKNOWN;
    }

    if (yawOnly) {
        // We really want to only adjust for Yaw
        glm::vec3 Forward = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::fquat CurrentQuat;
        CurrentQuat.x = poseState.pose.rotation.x;
        CurrentQuat.y = poseState.pose.rotation.y;
        CurrentQuat.z = poseState.pose.rotation.z;
        CurrentQuat.w = poseState.pose.rotation.w;

        glm::vec3 LookDir = CurrentQuat * Forward;

        float RotateRads = atan2(LookDir.x, LookDir.z);

        glm::fquat IdentityQuat;
        gAppContext->modeContext->recenterRot = glm::rotate(IdentityQuat, -RotateRads,
                                                            glm::vec3(0.0f, 1.0f, 0.0f));
    } else {
        // Recenter is whatever it takes to get back to center
        glm::fquat CurrentQuat;
        CurrentQuat.x = poseState.pose.rotation.x;
        CurrentQuat.y = poseState.pose.rotation.y;
        CurrentQuat.z = poseState.pose.rotation.z;
        CurrentQuat.w = poseState.pose.rotation.w;

        gAppContext->modeContext->recenterRot = inverse(CurrentQuat);
    }

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT uint32_t svrGetSupportedTrackingModes()
//-----------------------------------------------------------------------------
{
    uint32_t result = 0;

    if (gAppContext == NULL) {
        LOGE("svrGetSupportedTrackingModes failed: SnapdragonVR not initialized!");
        return 0;
    }

    if (gAppContext->mUseIvSlam) {
        //TODO: Check whether it is need implement more logic code
        result |= kTrackingRotation;
        result |= kTrackingPosition;
        return result;
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrGetSupportedTrackingModes failed: SnapdragonVR not initialized!");
            return 0;
        }
    }
    //Get the supported tracking modes
    uint32_t supportedTrackingModes;
    int qRes = QVRServiceClient_GetTrackingMode(gAppContext->qvrHelper, NULL,
                                                &supportedTrackingModes);
    if (qRes != QVR_SUCCESS) {
        L_LogQvrError("QVRServiceClient_GetTrackingMode", qRes);
    }
    if (supportedTrackingModes & TRACKING_MODE_ROTATIONAL ||
        supportedTrackingModes & TRACKING_MODE_ROTATIONAL_MAG) {
        result |= kTrackingRotation;
    }

    if (supportedTrackingModes & TRACKING_MODE_POSITIONAL) {
        //Note at this time the QVR service will report that it is capable of positional tracking
        //due to the presence of a second camera even though that second camera may not be a 6DOF module
        result |= kTrackingPosition;
    }

    // Get supported eye tracking
    uint32_t currentEyeMode;
    uint32_t supportedEyeModes;
    qRes = QVRServiceClient_GetEyeTrackingMode(gAppContext->qvrHelper, &currentEyeMode,
                                               &supportedEyeModes);
    if (qRes != QVR_SUCCESS) {
        L_LogQvrError("QVRServiceClient_GetEyeTrackingMode", qRes);
    } else if (supportedEyeModes & QVRSERVICE_EYE_TRACKING_MODE_DUAL) {
        result |= kTrackingEye;
    }

    return result;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT SvrResult svrSetTrackingMode(uint32_t trackingModes)
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("svrSetTrackingMode failed: gAppContext was not initialized!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper == nullptr) {
            LOGE("%s failed: ivslamHelper was not initialized!", __FUNCTION__);
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    } else {
        if (gAppContext->qvrHelper == NULL) {
            LOGE("svrSetTrackingMode failed: SnapdragonVR was not initialized!");
            return SVR_ERROR_VRMODE_NOT_INITIALIZED;
        }
    }

    // Don't want to break here in case they are setting eye tracking mode later.
    // Which may or may not depend on the VR State.

    // QVRSERVICE_VRMODE_STATE qvrState = QVRServiceClient_GetVRMode(gAppContext->qvrHelper);
    // if (qvrState != VRMODE_STOPPED)
    // {
    //     LOGE("svrSetTrackingMode failed: tracking mode can only be changed when VR mode is in a stopped state");
    //     return SVR_ERROR_VRMODE_NOT_STOPPED;
    // }

    // Validate what is passed in
    if ((trackingModes & kTrackingPosition) != 0) {
        // Because at this point having position without rotation is not supported
        trackingModes |= kTrackingRotation;
        trackingModes |= kTrackingPosition;
    }

    //Check for a forced tracking mode from the config file
    uint32_t actualTrackingMode = trackingModes;
    if (!gAppContext->mUseIvSlam) {
        if (gForceTrackingMode != 0) {
            actualTrackingMode = 0;
            LOGI("svrSetTrackingMode : Forcing to %d from config file", gForceTrackingMode);
            if (gForceTrackingMode & kTrackingRotation) {
                actualTrackingMode |= kTrackingRotation;
            }
            if (gForceTrackingMode & kTrackingPosition) {
                // Because at this point having position without rotation is not supported
                actualTrackingMode |= kTrackingRotation;
                actualTrackingMode |= kTrackingPosition;
            }
            if (gForceTrackingMode & kTrackingEye) {
                actualTrackingMode |= kTrackingEye;
            }
        } else {
            LOGI("svrSetTrackingMode : Using application defined tracking mode (%d).",
                 trackingModes);
        }
    }

    //Make sure the device actually supports the tracking mode
    uint32_t supportedModes = svrGetSupportedTrackingModes();

    //QVR Service accepts only a direct positional or rotational value, not a bitmask
    QVRSERVICE_TRACKING_MODE qvrTrackingMode = TRACKING_MODE_NONE;
    if ((actualTrackingMode & kTrackingPosition) != 0) {
        if ((supportedModes & kTrackingPosition) != 0) {
            LOGI("svrSetTrackingMode : Setting tracking mode to positional");
            qvrTrackingMode = TRACKING_MODE_POSITIONAL;
        } else {
            LOGI("svrSetTrackingMode: Requested positional tracking but device doesn't support, falling back to orientation only.");

            if (gUseMagneticRotationFlag)
                qvrTrackingMode = TRACKING_MODE_ROTATIONAL_MAG;
            else
                qvrTrackingMode = TRACKING_MODE_ROTATIONAL;

            actualTrackingMode &= ~kTrackingPosition;
        }

    } else if ((actualTrackingMode & kTrackingRotation) != 0) {
        LOGI("svrSetTrackingMode : Setting tracking mode to rotational");

        if (gUseMagneticRotationFlag)
            qvrTrackingMode = TRACKING_MODE_ROTATIONAL_MAG;
        else
            qvrTrackingMode = TRACKING_MODE_ROTATIONAL;

        actualTrackingMode &= ~kTrackingPosition;
    }

    int qRes = 0;
    if (!gAppContext->mUseIvSlam) {
        // Set normal tracking...
        int qRes = QVRServiceClient_SetTrackingMode(gAppContext->qvrHelper, qvrTrackingMode);
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_SetTrackingMode", qRes);
        }

        // ...and eye tracking
        if ((actualTrackingMode & kTrackingEye) != 0) {
            LOGI("svrSetTrackingMode : Eye tracking mode requested");

            // Get the current mode and supported modes
            uint32_t currentEyeMode;
            uint32_t supportedEyeModes;
            qRes = QVRServiceClient_GetEyeTrackingMode(gAppContext->qvrHelper, &currentEyeMode,
                                                       &supportedEyeModes);
            if (qRes != QVR_SUCCESS) {
                L_LogQvrError("QVRServiceClient_GetEyeTrackingMode", qRes);

                // Eye tracking not supported
                LOGI("svrSetTrackingMode: Eye tracking mode not supported");
                actualTrackingMode &= ~kTrackingEye;
            } else if ((supportedEyeModes & QVRSERVICE_EYE_TRACKING_MODE_DUAL) == 0) {
                LOGE("Error from QVRServiceClient_GetEyeTrackingMode: QVRSERVICE_EYE_TRACKING_MODE_DUAL is not supported!");
                LOGI("svrSetTrackingMode: Eye tracking mode not supported");
                actualTrackingMode &= ~kTrackingEye;
            }

            // If the current mode is not what we want then set it now
            if ((actualTrackingMode & kTrackingEye) != 0 &&
                (currentEyeMode & QVRSERVICE_EYE_TRACKING_MODE_DUAL) == 0) {
                LOGI("svrSetTrackingMode : Enabling eye tracking mode");
                currentEyeMode = QVRSERVICE_EYE_TRACKING_MODE_DUAL;
                qRes = QVRServiceClient_SetEyeTrackingMode(gAppContext->qvrHelper, currentEyeMode);
                if (qRes != QVR_SUCCESS) {
                    L_LogQvrError("QVRServiceClient_SetEyeTrackingMode (Dual)", qRes);

                    // Eye tracking not supported
                    LOGI("svrSetTrackingMode: Eye tracking mode not supported");
                    actualTrackingMode &= ~kTrackingEye;
                }
            }
        } // Trying to enable eye tracking
        else {
            // Eye tracking is NOT enabled, make sure it is off.
            qRes = QVRServiceClient_SetEyeTrackingMode(gAppContext->qvrHelper,
                                                       QVRSERVICE_EYE_TRACKING_MODE_NONE);
            if (qRes != QVR_SUCCESS) {
                L_LogQvrError("QVRServiceClient_SetEyeTrackingMode (Off)", qRes);
            }

            // Always pull out this bit if eye tracking turned off
            actualTrackingMode &= ~kTrackingEye;
        }
    }

    gAppContext->currentTrackingMode = actualTrackingMode;

    return SVR_ERROR_NONE;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT uint32_t svrGetTrackingMode()
//-----------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("svrGetTrackingMode failed: SnapdragonVR not initialized!");
        return 0;
    }

    return gAppContext->currentTrackingMode;
}

//-----------------------------------------------------------------------------
SVRP_EXPORT bool svrPollEvent(svrEvent *pEvent)
//-----------------------------------------------------------------------------
{
    if (gAppContext != NULL &&
        gAppContext->modeContext != NULL &&
        gAppContext->modeContext->eventManager != NULL) {
        return gAppContext->modeContext->eventManager->PollEvent(*pEvent);
    }

    return false;
}

SVRP_EXPORT float svrFetchDeflection() {
    return gDeflection;
}

// willie change start 2020-8-20
SVRP_EXPORT void svrUpdateRelativeDeflection(int deflection) {
    LOGI("scUpdateRelativeDeflection start set abstract deflection=%d, gMediatorHelper=%p",
         deflection,
         gMediatorHelper);
    if (nullptr == gMediatorHelper) {
        gMediatorHelper = scCreateMediator();
    }
    int currDeflection = 0;
    int res = scFetchDeflection(gMediatorHelper, currDeflection);
    LOGI("scUpdateRelativeDeflection currDeflection=%d res=%d", currDeflection, res);
    if (0 != res) {
        return;
    }
    int newDeflection = deflection;
//    int newDeflection = currDeflection + deflection;
    scUpdateDeflection(gMediatorHelper, newDeflection);
    LOGI("scUpdateRelativeDeflection done newDeflection=%d", newDeflection);
}
// willie change done 2020-8-20

SVRP_EXPORT int scGetOfflineMapRelocState() {
    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper) {
            return IvSlamClient_GetOfflineMapRelocState(gAppContext->ivslamHelper);
        }
    }
    return 1;
}

SVRP_EXPORT void scResaveMap(const char *path) {
    LOGI("scResaveMap path=%s", path);
    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper) {
            IvSlamClient_ReSaveMapMap(gAppContext->ivslamHelper, path);
        }
    }
}

SVRP_EXPORT void scGetGnss(double &dt, float *gnss) {
    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper) {
            IvSlamClient_GetGnss(gAppContext->ivslamHelper, dt, gnss);
        }
    }
}

SVRP_EXPORT int scGetPanel() {
    if (gAppContext->mUseIvSlam) {
        if (gAppContext->ivslamHelper) {
            IvSlamClient_GetPanel(gAppContext->ivslamHelper, mPanelSize, mPanelInfo);
            return mPanelSize;
        }
    } else {
#if 1   // for debug
        // float debugINfo[68*3] = {0};
        float panel1[
                2 + 16 * 3] = {0, 16, 7.92049, -0.700962, -4.44806, 7.76381, -0.700962, -5.2852,
                               7.61105, -0.700962, -5.4551, 7.1606, -0.700962, -5.82642, 5.68592,
                               -0.700962, -6.77381, 5.24447, -0.700962, -6.96169, 3.60803,
                               -0.700962, -5.63783, 3.0853, -0.700962, -5.16815, 2.5626, -0.700962,
                               -4.62051, -1.73509, -0.700962, 1.0777, -1.80119, -0.700962, 1.19153,
                               -1.77043, -0.700962, 1.60757, -1.40965, -0.700962, 2.66776, -1.01016,
                               -0.700962, 2.54142, -0.421896, -0.700962, 2.25137, 7.56967,
                               -0.700962, -4.14651};
        float panel2[
                2 + 15 * 3] = {1, 15, 7.18651, 0.0408566, -5.29466, 6.66501, 0.0408566, -8.08702,
                               6.64586, 0.0408566, -8.16221, 6.48848, 0.0408566, -8.45531, 5.49706,
                               0.0408566, -9.90318, 5.39325, 0.0408566, -9.98688, 5.04886,
                               0.0408566, -10.2294, 3.08607, 0.0408566, -10.7128, 2.2504, 0.0408566,
                               -10.6664, -1.59317, 0.0408566, -8.22763, -1.64592, 0.0408566,
                               -8.10943, -4.62121, 0.0408566, 1.00143, -2.6467, 0.0408566, 2.6492,
                               0.687408, 0.0408566, 0.917342, 7.05291, 0.0408566, -4.61748};
        float panel3[
                2 + 11 * 3] = {2, 11, 2.28344, -0.715485, -6.18769, 1.91045, -0.715485, -6.43061,
                               1.7313, -0.715485, -6.43367, 0.545626, -0.715485, -6.1697, -1.03625,
                               -0.715485, -1.53992, -0.993108, -0.715485, -1.3585, -0.589472,
                               -0.715485, -0.649404, -0.460017, -0.715485, -0.659332, 0.708583,
                               -0.715485, -1.28122, 2.19297, -0.715485, -4.83388, 2.27938,
                               -0.715485, -5.34684};
        memcpy(mPanelInfo, panel1, sizeof(float) * (2 + 16 * 3));
        memcpy(mPanelInfo + 68, panel2, sizeof(float) * (2 + 15 * 3));
        memcpy(mPanelInfo + 68 + 68, panel3, sizeof(float) * (2 + 11 * 3));

        mPanelSize = 3;
        return 3;
#endif
    }
    mPanelSize = 0;
    return 0;
}

SVRP_EXPORT void scGetPanelInfo(float *info) {
    if (mPanelSize > 0) {
        memcpy(info, mPanelInfo, sizeof(float) * mPanelSize * 68);
    }
}

//add zengchuiguo 20200724 (start)
static uint64_t last_index = 0;
SVRP_EXPORT int scHANDTRACK_Start(LOW_POWER_WARNING_CALLBACK callback) {
    LOGI("%s start", __FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            last_index = 0;
            gAppContext->mIvGestureStarted = true;
            IvGestureClient_Start(gAppContext->ivgestureClientHelper, callback);
        }
    }
    return 0;
}

SVRP_EXPORT int scHANDTRACK_Stop() {
    LOGI("%s start", __FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            LOGI("%s try stop the gesture client", __FUNCTION__);
            IvGestureClient_Stop(gAppContext->ivgestureClientHelper);
            gAppContext->mIvGestureStarted = false;
        }
    } else {
        LOGE("%s gAppContext was invalid!!!", __FUNCTION__);
    }
    LOGI("%s end", __FUNCTION__);
    return 0;
}


SVRP_EXPORT int scHANDTRACK_GetGesture(uint64_t *index, float *model, float *pose) {
    LOGI("%s start2233",__FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            IvGestureClient_GetGesture(gAppContext->ivgestureClientHelper, index, model, pose);
            int mindex = 0;
            int handnum = model[mindex++];
            LOGI("%s handnum = {%f %f %f %f} {%f %f %f}}",__FUNCTION__,model[0],model[1],model[2],model[3],pose[0],pose[1],pose[2]);
            if(handnum > 0){
                svrHeadPoseState poseState;
                glm::quat poseRot{1, 0, 0, 0};
                glm::vec3 posePos{0};
                poseState.poseStatus = 0;
                poseState.pose.rotation.x = poseRot.x;
                poseState.pose.rotation.y = poseRot.y;
                poseState.pose.rotation.z = poseRot.z;
                poseState.pose.rotation.w = poseRot.w;
                poseState.pose.position.x = posePos.x;
                poseState.pose.position.y = posePos.y;
                poseState.pose.position.z = posePos.z;
                glm::mat4 transformLeftM = glm::mat4(1);
                glm::mat4 transformRightM = glm::mat4(1);
                poseState = svrGetPredictedHeadPoseExt(svrGetPredictedDisplayTime(),true);

                glm::quat rotation;
                rotation.x = poseState.pose.rotation.x;
                rotation.y = poseState.pose.rotation.y;
                rotation.z = poseState.pose.rotation.z;
                rotation.w = poseState.pose.rotation.w;
                if(gAppContext->useTransformMatrix){
                    rotation = glm::inverse(gAppContext->correctYQuat) * rotation;
                }

                glm::vec3 position;
                position.x = poseState.pose.position.x;
                position.y = poseState.pose.position.y;
                position.z = poseState.pose.position.z;

                glm::mat4 poseM = glm::mat4_cast(rotation);
                glm::mat4 viewM = glm::translate(glm::mat4_cast(rotation), position);


                float LandRotLeft[16] = {0.0f, 1.0f, 0.0f, 0.0f,
                                         -1.0f, 0.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f};
                glm::mat4 LandRotLeft_glm = glm::make_mat4(LandRotLeft);

                glm::mat4 changeM = glm::inverse(viewM); //* glm::transpose(LandRotLeft_glm);

                //glm::mat4 changeM = viewM * LandRotLeft_glm;

                LOGE("MY_TAG","do not inverse");

                for(int i=0;i<handnum;i++){
                    int c = model[mindex++];
                    int jointNum = model[mindex++];
                    for(int j = 0;j < jointNum;j++){
                        glm::vec4 p(model[mindex] ,model[mindex+1],model[mindex+2],1.0);
                        p = changeM * p;
                        model[mindex] = p[0];
                        model[mindex+ 1] = p[1];
                        model[mindex+ 2] = -p[2];
                        mindex +=3;
                    }
                }


            }


            if (last_index != *index) {
                last_index = *index;
#ifdef DUMP_HAND_INFO
                dump_float_buffer_to_txt("data_svr",*index,model,256);
#endif
            }
        }
    }
    return 0;
}

static void destroyIvGesture() {
    LOGI("%s", __FUNCTION__);
    if (gAppContext->ivgestureClientHelper) {
        if (gAppContext->mIvGestureStarted) {
            LOGI("Stop the gesture services and destory it");
            IvGestureClient_Stop(gAppContext->ivgestureClientHelper);
            gAppContext->mIvGestureStarted = false;
        }
        IvGestureClient_Destroy(gAppContext->ivgestureClientHelper);
    }
}

SVRP_EXPORT void scHANDTRACK_SetGestureCallback(GESTURE_CHANGE_CALLBACK callback) {
    LOGI("%s start",__FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            IvGestureClient_SetGestureChangeCallback(gAppContext->ivgestureClientHelper, callback);
        }
    }
}

SVRP_EXPORT void
scHANDTRACK_SetGestureModelDataCallback(GESTURE_MODEL_DATA_CHANGE_CALLBACK callback) {
    LOGI("%s start",__FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            IvGestureClient_SetGestureModelDataChangeCallback(gAppContext->ivgestureClientHelper,
                                                              callback);
        }
    }
}

SVRP_EXPORT void scHANDTRACK_SetLowPowerWarningCallback(LOW_POWER_WARNING_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            LOGI("%s, set low power warning callback", __FUNCTION__);
            IvGestureClient_SetLowPowerWarningCallback(gAppContext->ivgestureClientHelper,
                                                       callback);
        } else {
            LOGE("%s, ivgestureClientHelper was invalid", __FUNCTION__);
        }
    } else {
        LOGE("%s, gAppContext was invalid", __FUNCTION__);
    }
}

SVRP_EXPORT void scHANDTRACK_setGestureData(float *model, float *pose) {
    LOGE("%s start",__FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivgestureClientHelper != nullptr) {
            IvGestureClient_SetGestureData(gAppContext->ivgestureClientHelper, model, pose);
        }
    }
}

SVRP_EXPORT void sc_setAppExitCallback(APP_EXIT_CALLBACK callback) {
    LOGI("%s start", __FUNCTION__);
    if (gAppContext) {
        if (gAppContext->ivslamHelper != nullptr) {
            IvSlamClient_SetAppExitCallback(gAppContext->ivslamHelper, callback);
        } else {
            LOGE("%s, slam helper was invalid!!!", __FUNCTION__);
        }
    } else {
        LOGE("%s, gAppContext was invalid!!!", __FUNCTION__);
    }
}
//add zengchuiguo 20200724 (end)

// add by chenweihua 20201026 (start)

SVRP_EXPORT int scFetchHandShank(float *orientationArray, int lr) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            if (gAppContext->bond == 0) {
                gAppContext->bond = scHandShank_Getbond();
            }

            if (gAppContext->bond == 34) { // K11
                return scFetch3dofHandShankNew(orientationArray, lr);
            } else if (gAppContext->bond == 17) { // K102
                return scFetch6dofHandShank(orientationArray, lr);
            }
        }
    }

    return -1;
}

SVRP_EXPORT int scFetch6dofHandShank(float *orientationArray, int lr) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            double time;

            int result = HandShankClient_GetPose(gAppContext->ivhandshankHelper, orientationArray,
                                                 time, lr);

            if (result < 0) {
                return result;
            }

            svrHeadPoseState poseState;
            poseState.poseStatus |= kTrackingPosition;

            float translation[3] = {0};
            float rotation[4] = {0};

            glm::mat4 poseMatrix = glm::make_mat4(orientationArray);
            translation[0] = poseMatrix[3][0];
            translation[1] = poseMatrix[3][1];
            translation[2] = poseMatrix[3][2];

            poseMatrix[3][0] = 0;
            poseMatrix[3][1] = 0;
            poseMatrix[3][2] = 0;

            glm::quat outData_rotationQuaternion = glm::quat_cast(poseMatrix);
            rotation[0] = outData_rotationQuaternion.x;
            rotation[1] = outData_rotationQuaternion.y;
            rotation[2] = outData_rotationQuaternion.z;
            rotation[3] = outData_rotationQuaternion.w;

            L_TrackingToReturn_for_handshank(poseState, NULL, NULL, NULL, NULL, 0.0f, rotation,
                                             translation);
            orientationArray[0] = poseState.pose.position.x;
            orientationArray[1] = poseState.pose.position.y;
            orientationArray[2] = poseState.pose.position.z;
            orientationArray[3] = poseState.pose.rotation.x;
            orientationArray[4] = poseState.pose.rotation.y;
            orientationArray[5] = poseState.pose.rotation.z;
            orientationArray[6] = poseState.pose.rotation.w;

            return 0;
        }
    }
    return -1;
}

SVRP_EXPORT int scFetch3dofHandShankNew(float *orientationArray, int lr) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_GetImu(gAppContext->ivhandshankHelper, gAppContext->handshank_imu, lr);

            if (gAppContext->handshank_imu.size() > 0) {
                float x = gAppContext->handshank_imu.back().quaternion[0];
                float y = gAppContext->handshank_imu.back().quaternion[1];
                float z = gAppContext->handshank_imu.back().quaternion[2];
                float w = gAppContext->handshank_imu.back().quaternion[3];

                // gAppContext->handshank_imu.erase(gAppContext->handshank_imu.begin(), gAppContext->handshank_imu.begin() + gAppContext->handshank_imu.size());
                gAppContext->handshank_imu.clear();

                svrHeadPoseState poseState = svrGetPredictedHeadPose(svrGetPredictedDisplayTimePipelined(1));

                if (lr == 0) {
                    orientationArray[0] = poseState.pose.position.x + 0.15f;
                    orientationArray[1] = poseState.pose.position.y + 0.25f;
                    orientationArray[2] = poseState.pose.position.z + 0.25f;
                } else {
                    orientationArray[0] = poseState.pose.position.x - 0.15f;
                    orientationArray[1] = poseState.pose.position.y + 0.25f;
                    orientationArray[2] = poseState.pose.position.z + 0.25f;
                }

                orientationArray[3] = -x;
                orientationArray[4] = -y;
                orientationArray[5] = z;
                orientationArray[6] = w;

                return 0;
            }
        }
    }
    return -1;
}

SVRP_EXPORT int scFetch3dofHandShank(float *orientationArray, int lr) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_GetImu(gAppContext->ivhandshankHelper, gAppContext->handshank_imu, lr);

            if (gAppContext->handshank_imu.size() > 0) {
                float x = gAppContext->handshank_imu.back().quaternion[0];
                float y = gAppContext->handshank_imu.back().quaternion[1];
                float z = gAppContext->handshank_imu.back().quaternion[2];
                float w = gAppContext->handshank_imu.back().quaternion[3];

                // gAppContext->handshank_imu.erase(gAppContext->handshank_imu.begin(), gAppContext->handshank_imu.begin() + gAppContext->handshank_imu.size());
                gAppContext->handshank_imu.clear();

                orientationArray[0] = 1.0f - 2.0f * y * y - 2.0f * z * z;
                orientationArray[4] = 2.0f * x * y - 2.0f * z * w;
                orientationArray[8] = 2.0f * x * z + 2.0f * y * w;
                orientationArray[12] = 0.0f;
                orientationArray[1] = 2.0f * x * y + 2.0f * z * w;
                orientationArray[5] = 1.0f - 2.0f * x * x - 2.0f * z * z;
                orientationArray[9] = 2.0f * y * z - 2.0f * x * w;
                orientationArray[13] = 0.0f;
                orientationArray[2] = 2.0f * x * z - 2.0f * y * w;
                orientationArray[6] = 2.0f * y * z + 2.0f * x * w;
                orientationArray[10] = 1.0f - 2.0f * x * x - 2.0f * y * y;
                orientationArray[14] = 0.0f;
                orientationArray[3] = 0.0f;
                orientationArray[7] = 0.0f;
                orientationArray[11] = 0.0f;
                orientationArray[15] = 1.0f;
                return 0;
            }
        }
    }
    return -1;
}

SVRP_EXPORT void scHandShank_SetKeyEventCallback(HANDSHANK_KEY_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterKeyEventCB(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT void scHandShank_SetKeyTouchEventCallback(HANDSHANK_KEY_TOUCH_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterKeyTouchEventCB(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT void scHandShank_SetTouchEventCallback(HANDSHANK_TOUCH_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterTouchEventCB(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT void scHandShank_SetHallEventCallback(HANDSHANK_HALL_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterHallEventCB(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT void scHandShank_SetChargingEventCallback(HANDSHANK_CHARGING_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterChargingEventCallback(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT void scHandShank_SetBatteryEventCallback(HANDSHANK_BATTERY_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterBatteryEventCallback(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT void scHandShank_SetConnectEventCallback(HANDSHANK_CONNECT_EVENT_CALLBACK callback) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_RegisterConnectEventCallback(gAppContext->ivhandshankHelper, callback);
        }
    }
}

SVRP_EXPORT int scHandShank_GetBattery(int lr) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            return HandShankClient_GetBattery(gAppContext->ivhandshankHelper, lr);
        }
    }

    return 0;
}

SVRP_EXPORT bool scHandShank_GetConnectState(int lr) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            return HandShankClient_GetConnectState(gAppContext->ivhandshankHelper, lr);
        }
    }

    return false;
}

SVRP_EXPORT int scHandShank_GetVersion() {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            return HandShankClient_GetVersion(gAppContext->ivhandshankHelper);
        }
    }

    return 0;
}

SVRP_EXPORT int scHandShank_Getbond() {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            return HandShankClient_Getbond(gAppContext->ivhandshankHelper);
        }
    }

    return 0;
}

SVRP_EXPORT void scHandShank_LedControl(int enable) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_LedControl(gAppContext->ivhandshankHelper, enable);
        }
    }
}

SVRP_EXPORT void scHandShank_VibrateControl(int value) {
    if (gAppContext) {
        if (gAppContext->ivhandshankHelper != nullptr) {
            HandShankClient_VibrateControl(gAppContext->ivhandshankHelper, value);
        }
    }
}

SvrResult svrGetTransformMatrix(bool &outBLoaded, float *outTransformArray) {
    LOGI("svrGetTransformMatrix start gAppContext=%p", gAppContext);
    if (nullptr == gAppContext) {
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }
    outBLoaded = gAppContext->useTransformMatrix;
    if (gAppContext->useTransformMatrix) {
        memcpy(outTransformArray, glm::value_ptr(gAppContext->transformMatrixArray[0]),
               sizeof(glm::mat4));
        memcpy(outTransformArray + 16, glm::value_ptr(gAppContext->transformMatrixArray[1]),
               sizeof(glm::mat4));
    }
    return SVR_ERROR_NONE;
}

svrHeadPoseState
svrGetLatestEyeMatrices(float *outLeftEyeMatrix, float *outRightEyeMatrix, float *outT, float *outR,
                        float predictedTimeMs) {
    svrHeadPoseState poseState;
    glm::quat poseRot{1, 0, 0, 0};
    glm::vec3 posePos{0};
    poseState.poseStatus = 0;
    poseState.pose.rotation.x = poseRot.x;
    poseState.pose.rotation.y = poseRot.y;
    poseState.pose.rotation.z = poseRot.z;
    poseState.pose.rotation.w = poseRot.w;
    poseState.pose.position.x = posePos.x;
    poseState.pose.position.y = posePos.y;
    poseState.pose.position.z = posePos.z;
    if (nullptr == gAppContext) {
        return poseState;
    }
    glm::mat4 transformLeftM = glm::mat4(1);
    glm::mat4 transformRightM = glm::mat4(1);
    poseState = svrGetPredictedHeadPose(predictedTimeMs);
    glm::quat rotation;
    rotation.x = poseState.pose.rotation.x;
    rotation.y = poseState.pose.rotation.y;
    rotation.z = poseState.pose.rotation.z;
    rotation.w = poseState.pose.rotation.w;
    glm::vec3 position;
    position.x = poseState.pose.position.x;
    position.y = poseState.pose.position.y;
    position.z = poseState.pose.position.z;

    constexpr float IPD = 0.064f;
    float deflectionRadians = glm::radians(gDeflection);
    glm::mat4 leftEyeOffsetMat = glm::rotate(glm::mat4(1), deflectionRadians, glm::vec3(0, 1, 0));
    glm::mat4 rightEyeOffsetMat = glm::rotate(glm::mat4(1), -deflectionRadians, glm::vec3(0, 1, 0));
    leftEyeOffsetMat = glm::translate(leftEyeOffsetMat, glm::vec3(0.5f * IPD, 0, 0));
    rightEyeOffsetMat = glm::translate(rightEyeOffsetMat, glm::vec3(-0.5f * IPD, 0, 0));
//    glm::mat4 poseM = glm::mat4_cast(rotation);
    glm::mat4 viewM = glm::translate(glm::mat4_cast(rotation), position);
    glm::mat4 leftEyeMatrix = gAppContext->transformMatrixArray[0] * leftEyeOffsetMat * viewM;
    glm::mat4 rightEyeMatrix = gAppContext->transformMatrixArray[1] * rightEyeOffsetMat * viewM;
    memcpy(outLeftEyeMatrix, glm::value_ptr(leftEyeMatrix), sizeof(float) * 16);
    memcpy(outRightEyeMatrix, glm::value_ptr(rightEyeMatrix), sizeof(float) * 16);
    memcpy(outT, glm::value_ptr(position), sizeof(glm::vec3));
    memcpy(outR, glm::value_ptr(rotation), sizeof(glm::quat));
//    LOGI("outLeftEyeMatrix = [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
//         leftEyeMatrix[0][0], leftEyeMatrix[1][0], leftEyeMatrix[2][0], leftEyeMatrix[3][0],
//         leftEyeMatrix[0][1], leftEyeMatrix[1][1], leftEyeMatrix[2][1], leftEyeMatrix[3][1],
//         leftEyeMatrix[0][2], leftEyeMatrix[1][2], leftEyeMatrix[2][2], leftEyeMatrix[3][2],
//         leftEyeMatrix[0][3], leftEyeMatrix[1][3], leftEyeMatrix[2][3], leftEyeMatrix[3][3]);
//    LOGI("rightEyeMatrix = [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
//         rightEyeMatrix[0][0], rightEyeMatrix[1][0], rightEyeMatrix[2][0], rightEyeMatrix[3][0],
//         rightEyeMatrix[0][1], rightEyeMatrix[1][1], rightEyeMatrix[2][1], rightEyeMatrix[3][1],
//         rightEyeMatrix[0][2], rightEyeMatrix[1][2], rightEyeMatrix[2][2], rightEyeMatrix[3][2],
//         rightEyeMatrix[0][3], rightEyeMatrix[1][3], rightEyeMatrix[2][3], rightEyeMatrix[3][3]);
//    LOGI("outT=[%f, %f, %f], outR=[%f, %f, %f, %f]", outT[0], outT[1], outT[2], outR[0], outR[1], outR[2], outR[3]);
    return poseState;
}

SvrResult svrGetLatestCameraBinocularData(bool &outBUpdate, uint32_t &outFrameIndex,
                                          uint64_t &outFrameExposureNano,
                                          uint8_t *outLeftFrameData,
                                          uint8_t *outRightFrameData) {
    outBUpdate = false;
    int availableIndex = -1;
    {
        std::unique_lock<std::mutex> autoLock(gAppContext->cameraMutex);
        availableIndex = gAppContext->availableCameraBufferIdx;
    }
    if (-1 == availableIndex || -1 == gCameraFrameIndexArray[availableIndex]) {
        LOGI("svrGetLatestCameraBinocularData skip for availableIndex=%d", availableIndex);
        return SVR_ERROR_NONE;
    }
    if (outFrameIndex != gCameraFrameIndexArray[availableIndex]) {
        // copy left half qvr camera data
        int halfWidth = gAppContext->cameraWidth / 2;
        for (int idx = 0; idx != gAppContext->cameraHeight; ++idx) {
            memcpy(outLeftFrameData + idx * halfWidth,
                   gCameraFrameDataArray[availableIndex].dataArray +
                   idx * gAppContext->cameraWidth, sizeof(uint8_t) * halfWidth);
            memcpy(outRightFrameData + idx * halfWidth,
                   gCameraFrameDataArray[availableIndex].dataArray +
                   halfWidth + idx * gAppContext->cameraWidth, sizeof(uint8_t) * halfWidth);
        }
        outFrameIndex = gCameraFrameIndexArray[availableIndex];
        outFrameExposureNano = gCameraFrameDataArray[availableIndex].exposureNano;
        outBUpdate = true;
    }
    return SVR_ERROR_NONE;
}

SVRP_EXPORT int scInitLayer() {
    WLOGI("svrApiCore.cpp scInitLayer start");
    mLayerFetcher = new LayerFetcher();
    int res = mLayerFetcher->init();
    if (res < 0) {
        WLOGE("svrApiCore.cpp scInitLayer Failed for init layerFetcher");
        return -1;
    }
    WLOGI("svrApiCore.cpp scInitLayer done successfully");
    return 0;
}

// for A11B
void updateLayerVertex(LayerVertex &outLayerVertex,
                       LayerData &layerData) {
    float windowWidth = gAppContext->deviceInfo.displayWidthPixels / 2;
    float windowHeight = gAppContext->deviceInfo.displayHeightPixels;
    int left = 0;
    int top = 0;
    int width = 1200;
    int height = 1080;
    int real_left = layerData.regionArray[0];
    int real_top = layerData.regionArray[1];
    int real_width = layerData.regionArray[2];
    int real_height = layerData.regionArray[3];

    if (0 == real_width) {
        real_width = width;
    }
    if (0 == real_height) {
        real_height = height;
    }

    float offsetX = (windowWidth - width) / 2;
    float offsetY = (windowHeight - height) / 2;
    float oriLeft = (left + offsetX) / windowWidth;
    float oriRight = (left + width + offsetX) / windowWidth;
    float oriTop = (windowHeight - top - offsetY) / windowHeight;
    float oriBottom = (windowHeight - top - height - offsetY) / windowHeight;
    float y = (windowHeight - height) / 2 / windowHeight;
    float oriWidth = width / windowWidth;
    float oriHeight = height / windowHeight;

    if (oriBottom < y) {
        oriBottom = y;
    }

    if (oriRight > 1) {
        oriRight = 1;
    }
    float vertexLeft = (oriRight - oriLeft) * real_left / width + oriLeft;
    float vertexRight = (oriRight - oriLeft) * real_width / width + oriLeft;
    float vertexTop = (oriBottom - oriTop) * real_top / height + oriTop;
    float vertexBottom = (oriBottom - oriTop) * real_height / height + oriTop;
    float aspectRatio = 1.0f;
    vertexTop = (1.0f - aspectRatio) / 2 + vertexTop * aspectRatio;
    vertexBottom = (1.0f - aspectRatio) / 2 + vertexBottom * aspectRatio;

    float xRatio = gAppContext->deviceInfo.leftEyeFrustum.right /
                   gAppContext->deviceInfo.leftEyeFrustum.near;
    float yRatio = gAppContext->deviceInfo.leftEyeFrustum.top /
                   gAppContext->deviceInfo.leftEyeFrustum.near;
    vertexLeft = (vertexLeft * 2 - 1) * xRatio;
    vertexRight = (vertexRight * 2 - 1) * xRatio;
    vertexTop = (vertexTop * 2 - 1) * yRatio;
    vertexBottom = (vertexBottom * 2 - 1) * yRatio;

    outLayerVertex.vertexPosition[0] = vertexLeft;
    outLayerVertex.vertexPosition[1] = vertexBottom;
    outLayerVertex.vertexPosition[2] = 0;
    outLayerVertex.vertexPosition[3] = vertexRight;
    outLayerVertex.vertexPosition[4] = vertexBottom;
    outLayerVertex.vertexPosition[5] = 0;
    outLayerVertex.vertexPosition[6] = vertexLeft;
    outLayerVertex.vertexPosition[7] = vertexTop;
    outLayerVertex.vertexPosition[8] = 0;
    outLayerVertex.vertexPosition[9] = vertexRight;
    outLayerVertex.vertexPosition[10] = vertexTop;
    outLayerVertex.vertexPosition[11] = 0;

    float uvLeft = 0;
    float uvRight = 1;
    float uvTop = 0;
    float uvBottom = 1;
    if (layerData.bHasTransparentRegion) {
        if (0 == layerData.textureTransform) {
            uvBottom = oriHeight;
        } else if (4 == layerData.textureTransform) {
            uvRight = oriHeight;
        } else if (7 == layerData.textureTransform) {
            uvLeft = 1 - oriHeight;
        }
    }
    outLayerVertex.vertexUV[0] = uvLeft;
    outLayerVertex.vertexUV[1] = uvBottom;
    outLayerVertex.vertexUV[2] = uvRight;
    outLayerVertex.vertexUV[3] = uvBottom;
    outLayerVertex.vertexUV[4] = uvLeft;
    outLayerVertex.vertexUV[5] = uvTop;
    outLayerVertex.vertexUV[6] = uvRight;
    outLayerVertex.vertexUV[7] = uvTop;
    if (7 == layerData.textureTransform) {
        outLayerVertex.vertexUV[0] = uvLeft;
        outLayerVertex.vertexUV[1] = uvTop;
        outLayerVertex.vertexUV[2] = uvLeft;
        outLayerVertex.vertexUV[3] = uvBottom;
        outLayerVertex.vertexUV[4] = uvRight;
        outLayerVertex.vertexUV[5] = uvTop;
        outLayerVertex.vertexUV[6] = uvRight;
        outLayerVertex.vertexUV[7] = uvBottom;
    } else if (4 == layerData.textureTransform) {
        outLayerVertex.vertexUV[0] = uvRight;
        outLayerVertex.vertexUV[1] = uvBottom;
        outLayerVertex.vertexUV[2] = uvRight;
        outLayerVertex.vertexUV[3] = uvTop;
        outLayerVertex.vertexUV[4] = uvLeft;
        outLayerVertex.vertexUV[5] = uvBottom;
        outLayerVertex.vertexUV[6] = uvLeft;
        outLayerVertex.vertexUV[7] = uvTop;
    }
}
/*
void updateLayerVertex(LayerVertex &outLayerVertex,
                       LayerData &layerData) {
    float windowWidth = gAppContext->deviceInfo.displayWidthPixels / 2;
    float windowHeight = gAppContext->deviceInfo.displayHeightPixels;
    int left = layerData.regionArray[0];
    int top = layerData.regionArray[1];
    int width = layerData.regionArray[2];
    int height = layerData.regionArray[3];
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }

    if (0 == width) {
        width = windowWidth;
    }
    if (0 == height) {
        height = windowHeight;
    }
    float oriLeft = left / windowWidth;
    float oriTop = (windowHeight - top) / windowHeight;
    float oriRight = (left + width) / windowWidth;
    float oriBottom = (windowHeight - top - height) / windowHeight;
    float oriWidth = width / windowWidth;
    float oriHeight = height / windowHeight;
    if (oriBottom < 0) {
        oriBottom = 0;
    }
    if (oriRight > 1) {
        oriRight = 1;
    }
    float vertexLeft = oriLeft;
    float vertexRight = oriRight;
    float vertexTop = oriTop;
    float vertexBottom = oriBottom;
    float aspectRatio = 1.0f;
    vertexTop = (1.0f - aspectRatio) / 2 + vertexTop * aspectRatio;
    vertexBottom = (1.0f - aspectRatio) / 2 + vertexBottom * aspectRatio;

    float xRatio = gAppContext->deviceInfo.leftEyeFrustum.right /
                   gAppContext->deviceInfo.leftEyeFrustum.near;
    float yRatio = gAppContext->deviceInfo.leftEyeFrustum.top /
                   gAppContext->deviceInfo.leftEyeFrustum.near;
    vertexLeft = (vertexLeft * 2 - 1) * xRatio;
    vertexRight = (vertexRight * 2 - 1) * xRatio;
    vertexTop = (vertexTop * 2 - 1) * yRatio;
    vertexBottom = (vertexBottom * 2 - 1) * yRatio;
    outLayerVertex.vertexPosition[0] = vertexLeft;
    outLayerVertex.vertexPosition[1] = vertexBottom;
    outLayerVertex.vertexPosition[2] = 0;
    outLayerVertex.vertexPosition[3] = vertexRight;
    outLayerVertex.vertexPosition[4] = vertexBottom;
    outLayerVertex.vertexPosition[5] = 0;
    outLayerVertex.vertexPosition[6] = vertexLeft;
    outLayerVertex.vertexPosition[7] = vertexTop;
    outLayerVertex.vertexPosition[8] = 0;
    outLayerVertex.vertexPosition[9] = vertexRight;
    outLayerVertex.vertexPosition[10] = vertexTop;
    outLayerVertex.vertexPosition[11] = 0;

    float uvLeft = 0;
    float uvRight = 1;
    float uvTop = 0;
    float uvBottom = 1;
    if (layerData.bHasTransparentRegion) {
        if (0 == layerData.textureTransform) {
            uvBottom = oriHeight;
        } else if (4 == layerData.textureTransform) {
            uvRight = oriHeight;
        } else if (7 == layerData.textureTransform) {
            uvLeft = 1 - oriHeight;
        }
    }
    outLayerVertex.vertexUV[0] = uvLeft;
    outLayerVertex.vertexUV[1] = uvBottom;
    outLayerVertex.vertexUV[2] = uvRight;
    outLayerVertex.vertexUV[3] = uvBottom;
    outLayerVertex.vertexUV[4] = uvLeft;
    outLayerVertex.vertexUV[5] = uvTop;
    outLayerVertex.vertexUV[6] = uvRight;
    outLayerVertex.vertexUV[7] = uvTop;
    if (7 == layerData.textureTransform) {
        outLayerVertex.vertexUV[0] = uvLeft;
        outLayerVertex.vertexUV[1] = uvTop;
        outLayerVertex.vertexUV[2] = uvLeft;
        outLayerVertex.vertexUV[3] = uvBottom;
        outLayerVertex.vertexUV[4] = uvRight;
        outLayerVertex.vertexUV[5] = uvTop;
        outLayerVertex.vertexUV[6] = uvRight;
        outLayerVertex.vertexUV[7] = uvBottom;
    } else if (4 == layerData.textureTransform) {
        outLayerVertex.vertexUV[0] = uvRight;
        outLayerVertex.vertexUV[1] = uvBottom;
        outLayerVertex.vertexUV[2] = uvRight;
        outLayerVertex.vertexUV[3] = uvTop;
        outLayerVertex.vertexUV[4] = uvLeft;
        outLayerVertex.vertexUV[5] = uvBottom;
        outLayerVertex.vertexUV[6] = uvLeft;
        outLayerVertex.vertexUV[7] = uvTop;
    }
}
*/
SVRP_EXPORT int scStartLayerRendering() {
    if (nullptr == mLayerFetcher) {
        return -1;
    }

    int currLayerNum = 0;
    {
        std::unique_lock<std::mutex> autoLock(*mLayerFetcher->getDataMutex());
        auto imageMap = *(mLayerFetcher->getEGLImageMap());
        auto layerDataMap = *(mLayerFetcher->getLayerDataMap());
        WLOGI("scStartLayerRendering imageMap.size=%zu, layerDataMap.size=%zu, mLayerTextureIdVector.size=%zu",
              imageMap.size(), layerDataMap.size(), mLayerTextureIdVector.size());
        int index = 0;
        for (auto &img : imageMap) {
            WLOGI("TMP scStartLayerRendering No.%u imageMap layerId=%u", index++, img.first);
        }
        index = 0;
        for (auto &layerData : layerDataMap) {
            WLOGI("TMP scStartLayerRendering No.%u layerData keyLayerId=%u valueLayerId=%u bOpaque=%d",
                  index++, layerData.first, layerData.second.layerId, layerData.second.bOpaque);
        }
        currLayerNum = layerDataMap.size();
        // prepare layer data.
        if (currLayerNum > mLayerTextureIdVector.size()) {
            int extraLayerNum = currLayerNum - mLayerTextureIdVector.size();
            uint *textureIdArray = new uint[extraLayerNum];
            WLOGI("scStartLayerRendering generate %d layers", extraLayerNum);
            glGenTextures(extraLayerNum, textureIdArray);
            for (int idx = 0; idx != extraLayerNum; ++idx) {
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureIdArray[idx]);
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
                mLayerTextureIdVector.emplace_back(textureIdArray[idx]);
            }
            delete[] textureIdArray;
        }
        mLayerDataVector.clear();
        mLayerVertexVector.clear();

        std::vector<std::pair<uint32_t, LayerData>> sortedLayerDatas;
        for (auto &i : layerDataMap) {
            sortedLayerDatas.push_back(i);
        }

        std::sort(sortedLayerDatas.begin(), sortedLayerDatas.end(),
                  [](std::pair<uint32_t, LayerData> &a, std::pair<uint32_t, LayerData> &b) {
                      return a.second.z < b.second.z;
                  });
        index = 0;
        for (const auto &element : sortedLayerDatas) {
            WLOGI("scStartLayerRendering sortedLayerDatas layerId=%u ", element.first);
            if (imageMap.find(element.first) == imageMap.end()) {
                WLOGI("scStartLayerRendering layerId=%u not exist in imageMap", element.first);
                continue;
            }
            LayerData currLayerData = element.second;
            LayerVertex currLayerVertex;
            updateLayerVertex(currLayerVertex, currLayerData);
            mLayerDataVector.emplace_back(currLayerData);
            mLayerVertexVector.emplace_back(currLayerVertex);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, mLayerTextureIdVector[index]);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES,
                                         static_cast<GLeglImageOES>(imageMap[element.first]));
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            WLOGI("TEST scStartLayerRendering add No.%d layerId=%u, layerTextureId=%u", index,
                  currLayerData.layerId, mLayerTextureIdVector[index]);
            ++index;
        }
        currLayerNum = index;
    }
    WLOGI("TEST scStartLayerRendering done currLayerNum=%d", currLayerNum);
    return currLayerNum;
}

SVRP_EXPORT int scGetAllLayersData(SCAllLayers *outAllLayers) {
    memcpy(outAllLayers->viewMatrixData, glm::value_ptr(mViewMatrix), 64);
    WLOGI("TEST scGetAllLayersData start outAllLayers->layerNum=%u mLayerDataVector.size=%zu mLayerTextureIdVector.size=%zu mLayerVertexVector.size=%zu",
          outAllLayers->layerNum, mLayerDataVector.size(), mLayerTextureIdVector.size(),
          mLayerVertexVector.size());
    for (int layerIdx = 0; layerIdx != outAllLayers->layerNum; ++layerIdx) {
        outAllLayers->layerData[layerIdx].layerId = mLayerDataVector[layerIdx].layerId;
        outAllLayers->layerData[layerIdx].parentLayerId = mLayerDataVector[layerIdx].parentLayerId;
        outAllLayers->layerData[layerIdx].layerTextureId = mLayerTextureIdVector[layerIdx];
        memcpy(outAllLayers->layerData[layerIdx].modelMatrixData,
               glm::value_ptr(mLayerDataVector[layerIdx].modelMatrix), 64);
        outAllLayers->layerData[layerIdx].editFlag = mLayerDataVector[layerIdx].editFlag;
        outAllLayers->layerData[layerIdx].z = mLayerDataVector[layerIdx].z;
        memcpy(outAllLayers->layerData[layerIdx].vertexPosition,
               mLayerVertexVector[layerIdx].vertexPosition, 48);
        memcpy(outAllLayers->layerData[layerIdx].vertexUV, mLayerVertexVector[layerIdx].vertexUV,
               32);
        outAllLayers->layerData[layerIdx].alpha = mLayerDataVector[layerIdx].alpha;
        outAllLayers->layerData[layerIdx].bOpaque = mLayerDataVector[layerIdx].bOpaque;
        outAllLayers->layerData[layerIdx].taskId = mLayerDataVector[layerIdx].taskId;
        WLOGI("TEST scGetAllLayersData No.%d layerId=%u, parentId=%d layerTextureId=%u, editFlag=%d, taskId=%d, z=%d, alpha=%f, bOpaque=%d, "
              "modelMatrixData=[%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
              layerIdx, outAllLayers->layerData[layerIdx].layerId,
              outAllLayers->layerData[layerIdx].parentLayerId,
              outAllLayers->layerData[layerIdx].layerTextureId,
              outAllLayers->layerData[layerIdx].editFlag,
              outAllLayers->layerData[layerIdx].taskId,
              outAllLayers->layerData[layerIdx].z,
              outAllLayers->layerData[layerIdx].alpha,
              outAllLayers->layerData[layerIdx].bOpaque,
              outAllLayers->layerData[layerIdx].modelMatrixData[0],
              outAllLayers->layerData[layerIdx].modelMatrixData[4],
              outAllLayers->layerData[layerIdx].modelMatrixData[8],
              outAllLayers->layerData[layerIdx].modelMatrixData[12],
              outAllLayers->layerData[layerIdx].modelMatrixData[1],
              outAllLayers->layerData[layerIdx].modelMatrixData[5],
              outAllLayers->layerData[layerIdx].modelMatrixData[9],
              outAllLayers->layerData[layerIdx].modelMatrixData[13],
              outAllLayers->layerData[layerIdx].modelMatrixData[2],
              outAllLayers->layerData[layerIdx].modelMatrixData[6],
              outAllLayers->layerData[layerIdx].modelMatrixData[10],
              outAllLayers->layerData[layerIdx].modelMatrixData[14],
              outAllLayers->layerData[layerIdx].modelMatrixData[3],
              outAllLayers->layerData[layerIdx].modelMatrixData[7],
              outAllLayers->layerData[layerIdx].modelMatrixData[11],
              outAllLayers->layerData[layerIdx].modelMatrixData[15]
        );
        WLOGI("TEST scGetAllLayersData No.%d layerId=%u, pos=[%f, %f, %f; %f, %f, %f; %f, %f, %f; %f, %f, %f] uv=[%f, %f; %f, %f; %f, %f; %f; %f]",
              layerIdx, outAllLayers->layerData[layerIdx].layerId,
              outAllLayers->layerData[layerIdx].vertexPosition[0],
              outAllLayers->layerData[layerIdx].vertexPosition[1],
              outAllLayers->layerData[layerIdx].vertexPosition[2],
              outAllLayers->layerData[layerIdx].vertexPosition[3],
              outAllLayers->layerData[layerIdx].vertexPosition[4],
              outAllLayers->layerData[layerIdx].vertexPosition[5],
              outAllLayers->layerData[layerIdx].vertexPosition[6],
              outAllLayers->layerData[layerIdx].vertexPosition[7],
              outAllLayers->layerData[layerIdx].vertexPosition[8],
              outAllLayers->layerData[layerIdx].vertexPosition[9],
              outAllLayers->layerData[layerIdx].vertexPosition[10],
              outAllLayers->layerData[layerIdx].vertexPosition[11],
              outAllLayers->layerData[layerIdx].vertexUV[0],
              outAllLayers->layerData[layerIdx].vertexUV[1],
              outAllLayers->layerData[layerIdx].vertexUV[2],
              outAllLayers->layerData[layerIdx].vertexUV[3],
              outAllLayers->layerData[layerIdx].vertexUV[4],
              outAllLayers->layerData[layerIdx].vertexUV[5],
              outAllLayers->layerData[layerIdx].vertexUV[6],
              outAllLayers->layerData[layerIdx].vertexUV[7]
        );
    }
    return 0;
}

SVRP_EXPORT int scEndLayerRendering(SCAllLayers *allLayers) {
    WLOGI("TEST scEndLayerRendering start allLayers->layerNum=%u allLayers->layerData=%p",
          allLayers->layerNum, allLayers->layerData);
    if (allLayers->layerNum >= 0) {
        if (allLayers->layerData != nullptr) {
            delete allLayers->layerData;
            allLayers->layerData = nullptr;
        }
    }

    return 0;
}

SVRP_EXPORT int scUpdateModelMatrix(uint32_t layerId, float *modelMatrixArray) {
    WLOGI("scUpdateModelMatrix start layerId=%d modelMatrixArray=%p", layerId,
          modelMatrixArray);
    WLOGI("scUpdateModelMatrix modelMatrixArray[0-3]=[%f, %f, %f, %f]",
          modelMatrixArray[0], modelMatrixArray[1], modelMatrixArray[2], modelMatrixArray[3]);
    if (nullptr == mLayerFetcher) {
        return -1;
    }
    mLayerFetcher->updateModelMatrix(layerId, modelMatrixArray);
    WLOGI("scUpdateModelMatrix done");
    return 0;
}

SVRP_EXPORT int scSendActionBarCMD(uint32_t layerId, int cmd) {
    WLOGI("scSendActionBarCMD start layerId=%d cmd=%d", layerId, cmd);
    if (nullptr == mLayerFetcher) {
        return -1;
    }
    mLayerFetcher->sendActionBarCmd(layerId, cmd);
    WLOGI("scSendActionBarCMD done");
    return 0;
}

long getCurrentTime() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000LL + now.tv_nsec / 1000000LL);
}

SVRP_EXPORT int scInjectMotionEvent(uint32_t layerId, int displayID, int action, float x, float y) {
    if (nullptr == mLayerFetcher) {
        return -1;
    }
    long ts = getCurrentTime();
    WLOGI("scInjectMotionEvent start layerId=%d action=%d time =%ld position={%f,%f}", layerId, action, ts, x, y);
    mLayerFetcher->injectMotionEvent(layerId, displayID, action, ts, x, y);
    WLOGI("scInjectMotionEvent done");
    return 0;
}

SVRP_EXPORT int scDestroyLayer() {
    WLOGI("scDestroyLayer start");
    delete mLayerFetcher;
    mLayerFetcher = nullptr;
    WLOGI("scDestroyLayer done");
    return 0;
}

SVRP_EXPORT bool scEnableDebugWithProperty() {
    return gDebugWithProperty;
}
