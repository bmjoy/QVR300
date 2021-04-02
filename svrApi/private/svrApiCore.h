//=============================================================================
// FILE: svrApiCore.h
//
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#ifndef _SVR_API_CORE_H_
#define _SVR_API_CORE_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <android/looper.h>
#include <android/native_window.h>
#include <android/sensor.h>

#include <pthread.h>
#include <jni.h>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

#include "svrApi.h"
#include "svrGpuTimer.h"
#include "private/svrApiEvent.h"
#include "QVRServiceClient.h"

#include "svrRenderExt.h"

#include "SvrServiceClient.h"
#include "ControllerManager.h"

#include "QVRCameraClient.h"

#include "slam/InvisionCameraHelper.h"
#include "slam/InvisionSlamClient.h"
#include "gesture/InvisionGestureClient.h"
#include "handshank/HandShankHelper.h"
#include "slam/A11GDeviceHelper.h"
#define NUM_SWAP_FRAMES 5

#include "utils/VeraSignal.h"
#include "utils/CpuTimer.h"
#include <thread>

namespace Svr {
    struct svrFrameParamsInternal {
        svrFrameParams frameParams;
        GLsync frameSync;
        EGLSyncKHR externalSync;
        uint64_t frameSubmitTimeStamp;
        uint64_t warpFrameLeftTimeStamp;
        uint64_t warpFrameRightTimeStamp;
        uint64_t minVSyncCount;
    };

    struct SvrModeContext {
        EGLDisplay display;
        ANativeWindow *nativeWindow;
        EGLint colorspace;

        uint64_t vsyncCount;
        uint64_t vsyncTimeNano;
        pthread_mutex_t vsyncMutex;

        //Warp Thread/Context data
        EGLSurface eyeRenderWarpSurface;
        EGLSurface eyeRenderOrigSurface;
        EGLint eyeRenderOrigConfigId;
        EGLContext eyeRenderContext;

        EGLContext warpRenderContext;
        EGLSurface warpRenderSurface;
        int warpRenderSurfaceWidth;
        int warpRenderSurfaceHeight;

        pthread_cond_t warpThreadContextCv;
        pthread_mutex_t warpThreadContextMutex;
        bool warpContextCreated;

        pthread_t warpThread;
        bool warpThreadExit;

        pthread_t vsyncThread;
        bool vsyncThreadExit;

        pthread_cond_t warpBufferConsumedCv;
        pthread_mutex_t warpBufferConsumedMutex;

        svrFrameParamsInternal frameParams[NUM_SWAP_FRAMES];
        unsigned int submitFrameCount;
        unsigned int warpFrameCount;
        uint64_t prevSubmitVsyncCount;

        pid_t renderThreadId;
        pid_t warpThreadId;

        // Recenter transforms
        glm::fquat recenterRot;
        glm::vec3 recenterPos;

        // Protected content
        bool isProtectedContent;

        // Event Manager
        svrEventManager *eventManager;

        // Performance Counters
        unsigned int fpsFrameCounter = 0;
        unsigned int fpsPrevTimeMs = 0;

        // Pose error tracking
        svrHeadPoseState prevPoseState;

    };

    struct SvrAppContext {
        ALooper *looper;

        JavaVM *javaVm;
        JNIEnv *javaEnv;
        jobject javaActivityObject;

        jclass javaSvrApiClass;
        jmethodID javaSvrApiStartVsyncMethodId;
        jmethodID javaSvrApiStopVsyncMethodId;

        qvrservice_client_helper_t *qvrHelper;

        ivslam_client_helper_t *ivslamHelper;
        //add zengchuiguo 20200724 (start)
        bool mIvGestureStarted;
        ivgesture_client_helper_t *ivgestureClientHelper = nullptr;
        a11gdevice_client_helper_t *a11GdeviceClientHelper = nullptr;
        //add zengchuiguo 20200724 (end)

        // add by chenweihua 20201026 (start)
        handshank_client_helper_t * ivhandshankHelper = nullptr;
        std::vector<HandShank_IMUDATA> handshank_imu;
        int bond = 0;
        // add by chenweihua 20201026 (end)

        SvrServiceClient *svrServiceClient;
        ControllerManager *controllerManager;

        SvrModeContext *modeContext;

        svrDeviceInfo deviceInfo;
        unsigned int currentTrackingMode;

        bool inVrMode;
        bool mUseIvSlam;

        qvrcamera_client_helper_t *qvrCameraClient = nullptr;
        qvrcamera_device_helper_t *qvrDeviceClient = nullptr;
        int qvrCameraApiVersion = 0;
        uint16_t cameraWidth = 0;
        uint16_t cameraHeight = 0;
        std::mutex cameraMutex;
        std::thread qvrCameraThread;
        Vera::VeraSignal qvrCameraStartSignal{false};
        Vera::VeraSignal qvrCameraTerminalSignal{false};
        int newCameraBufferIdx = -1;
        int availableCameraBufferIdx = -1;

        bool useTransformMatrix = false;
        glm::mat4 transformMatrixArray[2];
        glm::quat correctYQuat;
    };

    struct CameraFrameData {
        uint8_t *dataArray = nullptr;
        uint32_t frameIndex = 0;
        uint64_t exposureNano = 0;
    };

    extern SvrAppContext *gAppContext;
}

#endif //_SVR_API_CORE_H_