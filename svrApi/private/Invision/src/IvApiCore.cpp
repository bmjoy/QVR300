//
// Created by zengchuiguo on 5/21/2020.
//

#include <fstream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <private/svrApiCore.h>
#include "private/Invision/inc/IvApiCore.h"
#include "private/Invision/inc/IvApiCoreExternVars.h"
#include "private/svrApiCore.h"
#include "svrUtil.h"
#include "svrConfig.h"
#include "svrCpuTimer.h"
#include "svrProfile.h"
#include "private/svrApiTimeWarp.h"
#include "private/svrApiDebugServer.h"
#include "private/svrApiHelper.h"

#define MAX_LINE 1024

static const std::string PATH_DATA_PREFIX = "/data/test/";
static const std::string PATH_SUFFIX_TXT = ".txt";

using namespace Svr;

extern void L_SetThreadPriority(const char *pName, int policy, int priority);
extern SvrResult svrSetPerformanceLevelsInternal(svrPerfLevel cpuPerfLevel, svrPerfLevel gpuPerfLevel);

static const std::string QCOM_GPU_BUSY_FILE_PATH = "/sys/class/kgsl/kgsl-3d0/gpubusy";
static const char *SVRAPI_CONFIG_FILE_PATH = "/vendor/etc/qvr/svrapi_config.txt";

#if 0
SvrResult invisionBeginVr(const svrBeginParams *pBeginParams){

    LOGI("%s",__FUNCTION__);
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

    if(gAppContext->ivslamHelper == nullptr){
        LOGE("%s Failed: Invision VR not initialized!",__FUNCTION__);
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
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
        LOGI("Configuring QVR Vsync Interrupt...");
        qvrservice_lineptr_interrupt_config_t interruptConfig;
        memset(&interruptConfig, 0, sizeof(qvrservice_lineptr_interrupt_config_t));
        interruptConfig.cb = 0;
        interruptConfig.line = 1;

        //TODO: Implement SetDisplayInterruptConfig of Ivslam?
        LOGI("%s, SetDisplayInterruptConfig has not been implemented",__FUNCTION__);
#if 0
        int qRes = QVRServiceClient_SetDisplayInterruptConfig(gAppContext->qvrHelper,
                                                              DISP_INTERRUPT_LINEPTR,
                                                              (void *) &interruptConfig,
                                                              sizeof(qvrservice_lineptr_interrupt_config_t));
        if (qRes != QVR_SUCCESS) {
            L_LogQvrError("QVRServiceClient_SetDisplayInterruptConfig", qRes);
        }
#endif
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


    IvSlamClient_Start(gAppContext->ivslamHelper);

//    int qRes;
    //TODO:Implement API to query slam version
    //qRes = QVRServiceClient_GetParam(gAppContext->qvrHelper, QVRSERVICE_SERVICE_VERSION, &retLen,
    //                                 NULL);


    //TODO: implement API to enable thermal notifications
    //qRes = QVRServiceClient_RegisterForNotification(gAppContext->qvrHelper,
    //                                                NOTIFICATION_THERMAL_INFO,
    //                                                qvrClientThermalNotificationCallback, 0);


    LOGI("Setting CPU/GPU Performance Levels...");
    svrSetPerformanceLevels(pBeginParams->cpuPerfLevel, pBeginParams->gpuPerfLevel);

    if (gEnableTimeWarp) {
        LOGI("Starting Timewarp...");

        svrBeginTimeWarp();
    } else {
        LOGI("Timewarp not started.  Disabled by config file.");
    }

    if (gEnableDebugServer) {
        svrStartDebugServer();
    }

#if 0
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
#endif
    // Only now are we truly in VR mode
    gAppContext->inVrMode = true;

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
    LOGI("%s done successfully",__FUNCTION__);

    return SVR_ERROR_NONE;
}

SvrResult invisionEndVr(){
    LOGI("%s",__FUNCTION__);
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

    if (pQueue != NULL)
        pQueue->SubmitEvent(kEventVrModeStopped, data);

    if(gAppContext->ivslamHelper != nullptr){
        IvSlamClient_Stop(gAppContext->ivslamHelper);
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

    LOGI("%s, VR mode ended",__FUNCTION__);
    return SVR_ERROR_NONE;
}

SVRP_EXPORT SvrResult invisionSetTrackingMode(uint32_t trackingModes){
    // Validate what is passed in
    if ((trackingModes & kTrackingPosition) != 0) {
        // Because at this point having position without rotation is not supported
        trackingModes |= kTrackingRotation;
        trackingModes |= kTrackingPosition;
    }

#if 1
    //Check for a forced tracking mode from the config file
    uint32_t actualTrackingMode = trackingModes;
#if 0
    if (gForceTrackingMode != 0) {
//        actualTrackingMode = 0;
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
            LOGI("svrSetTrackingMode : Using application defined tracking mode (%d).", trackingModes);
        }
#endif

    //Make sure the device actually supports the tracking mode
    uint32_t supportedModes = svrGetSupportedTrackingModes();

    //QVR Service accepts only a direct positional or rotational value, not a bitmask
    QVRSERVICE_TRACKING_MODE qvrTrackingMode = TRACKING_MODE_NONE;
    if ((actualTrackingMode & kTrackingPosition) != 0) {
        if ((supportedModes & kTrackingPosition) != 0) {
            LOGI("%s : Setting tracking mode to positional",__FUNCTION__);
            qvrTrackingMode = TRACKING_MODE_POSITIONAL;
        } else {
            LOGI("%s: Requested positional tracking but device doesn't support, falling back to orientation only.",__FUNCTION__);

            if (gUseMagneticRotationFlag)
                qvrTrackingMode = TRACKING_MODE_ROTATIONAL_MAG;
            else
                qvrTrackingMode = TRACKING_MODE_ROTATIONAL;

            actualTrackingMode &= ~kTrackingPosition;
        }

    } else if ((actualTrackingMode & kTrackingRotation) != 0) {
        LOGI("%s: Setting tracking mode to rotational",__FUNCTION__);

        if (gUseMagneticRotationFlag)
            qvrTrackingMode = TRACKING_MODE_ROTATIONAL_MAG;
        else
            qvrTrackingMode = TRACKING_MODE_ROTATIONAL;

        actualTrackingMode &= ~kTrackingPosition;
    }
#endif
    gAppContext->currentTrackingMode = trackingModes;
    return SVR_ERROR_NONE;
}
#endif

int invisionGetParam(char* pBuffer, uint32_t* pLen){
    int ret = QVR_ERROR;
    *pLen = 0;
    FILE *fp;
    if((fp = fopen(SVRAPI_CONFIG_FILE_PATH,"r")) == NULL){
        LOGI("%s, read file %s fail",__FUNCTION__,SVRAPI_CONFIG_FILE_PATH);
        return ret;
    }

    const int buf_len = sizeof(char)*MAX_LINE + 1;
    char *buf = (char*)malloc(buf_len);
    if(!buf){
        LOGE("%s error, insufficient memory ",__FUNCTION__,SVRAPI_CONFIG_FILE_PATH);
        return ret;
    }

    int read_len = 0;
    memset(buf,0x00,buf_len);
    while(fgets(buf,MAX_LINE,fp) != NULL){
        read_len = strlen(buf);
//        LOGE("%s, read_len = %d, %s ",__FUNCTION__,read_len,buf);
        if(pBuffer){
            memcpy(pBuffer + (*pLen),buf,read_len);
        }
        *pLen += read_len;
        memset(buf,0x00,buf_len);
    }
    fclose(fp);

    ret = QVR_SUCCESS;
    return ret;
}

int dump_buffer_to_txt(const char* file_name,const char* data, const int len){
    int ret = -1;
    std::stringstream ss;
    ss << PATH_DATA_PREFIX << file_name << PATH_SUFFIX_TXT;
    LOGI("%s, dump to file: %s",__FUNCTION__,ss.str().c_str());
    FILE *fp = fopen(ss.str().c_str(),"w");
    if(fp == NULL){
        LOGI("%s, open file %s fail",__FUNCTION__,ss.str().c_str());
        return ret;
    }

    fwrite(data, len, 1, fp);
    fclose(fp);
    return ret;
}

int dump_float_buffer_to_txt(const char* file_name, const uint64_t index, const float* data, const int total_len){
    int ret = -1;
    std::stringstream ss;
    ss << PATH_DATA_PREFIX << file_name << PATH_SUFFIX_TXT;
    LOGI("%s, dump to file: %s",__FUNCTION__,ss.str().c_str());
    FILE *fp = fopen(ss.str().c_str(),"a");
    if(fp == NULL){
        LOGI("%s, open file %s fail",__FUNCTION__,ss.str().c_str());
        return ret;
    }

    fprintf(fp,"%ld: ",index);
    for (int i = 0; i < total_len; ++i) {
        fprintf(fp,"%.8f ",data[i]);
    }
    fprintf(fp,"\n");
//    fwrite(data, total_len, 1, fp);
    fclose(fp);
    return ret;
}