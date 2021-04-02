#ifndef _SVR_UNITY_H_
#define _SVR_UNITY_H_

#include <jni.h>
#include <sys/syscall.h>
#include <memory.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <android/native_window_jni.h>
#include <android/log.h>

#include "IUnityInterface.h"
#include "IUnityGraphics.h"

#include "svrApi.h"

#define SVR_EXPORT

#define MAX_EYE_BUFFERS 128

#define DEBUG 0

#if DEBUG
#define LogDebug(...)	__android_log_print(ANDROID_LOG_INFO, "svrUnity", __VA_ARGS__ )
#else
#define LogDebug(...)
#endif

#define Log(...)        __android_log_print(ANDROID_LOG_INFO, "svrUnity", __VA_ARGS__ )
#define LogError(...)    __android_log_print(ANDROID_LOG_ERROR, "svrUnity",__VA_ARGS__ )


#ifndef GL_EXT_framebuffer_foveated
#define GL_FOVEATION_ENABLE_BIT_QCOM                    0x0001
#define GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM         0x0002
#define GL_TEXTURE_PREVIOUS_SOURCE_TEXTURE_QCOM         0x8BE8
#define GL_TEXTURE_FOVEATED_FRAME_OFFSET_QCOM           0x8BE9
#define GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM           0x8BFB
#define GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM      0x8BFC
#define GL_TEXTURE_FOVEATED_FEATURE_QUERY_QCOM          0x8BFD
#define GL_TEXTURE_FOVEATED_NUM_FOCAL_POINTS_QUERY_QCOM 0x8BFE
#define GL_FRAMEBUFFER_INCOMPLETE_FOVEATION_QCOM        0x8BFF

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glFramebufferFoveationConfigQCOM(GLuint fbo, GLuint numLayers, GLuint focalPointsPerLayer, GLuint requiredFeatures, GLuint* gotFeatures);
GL_APICALL void GL_APIENTRY glFramebufferFoveationParametersQCOM(GLuint fbo, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
GL_APICALL void GL_APIENTRY glTextureFoveationParametersQCOM(GLuint texure, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
#endif

#define GL_APIENTRYP GL_APIENTRY*

typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERFOVEATIONCONFIGEXT)(GLuint fbo, GLuint numLayers,
                                                               GLuint focalPointsPerLayer,
                                                               GLuint requiredFeatures,
                                                               GLuint *gotFeatures);

typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERFOVEATIONPARAMETERSEXT)(GLuint fbo, GLuint layer,
                                                                   GLuint focalPoint,
                                                                   GLfloat focalX, GLfloat focalY,
                                                                   GLfloat gainX, GLfloat gainY,
                                                                   GLfloat foveaArea);

typedef void (GL_APIENTRYP PFNGLTEXTUREFOVEATIONPARAMETERSEXT)(GLuint texture, GLuint layer,
                                                               GLuint focalPoint, GLfloat focalX,
                                                               GLfloat focalY, GLfloat gainX,
                                                               GLfloat gainY, GLfloat foveaArea);

PFNGLFRAMEBUFFERFOVEATIONCONFIGEXT glFramebufferFoveationConfigQCOM = NULL;
PFNGLFRAMEBUFFERFOVEATIONPARAMETERSEXT glFramebufferFoveationParametersQCOM = NULL;
PFNGLFRAMEBUFFERFOVEATIONPARAMETERSEXT glTextureFoveationParametersQCOM = NULL;
bool glTextureFoveationFrameOffsetQCOM = false;

#endif

struct SvrInitParams {
    JavaVM *javaVm;
    JNIEnv *javaEnv;
    jobject javaActivityObject;
};

class SvrUnity {
public:
    bool isInitialized;
    bool isRunning;
    jobject activity;
    pid_t threadTid;
    JavaVM *javaVm;
    bool isInitializeDataSet;
    int frameType;
    svrFrameParams frameParams;
    bool isFrameParamsDataSet;
    svrHeadPoseState headPoses[3];
    svrEyePoseState eyePoses[3];
    int trackingPoseMode;
    bool isSetTrackingPoseDataSet;
    int cpuPerfLevel;
    int gpuPerfLevel;
    int colorSpace;
    bool isSetPerformanceLevelsDataSet;
    int eyeSideMask;
    int eyeLayerMask;
    bool isEyeDataSet;
    int eyeBufferList[MAX_EYE_BUFFERS];
    int nEyeBuffers;
    int textureId;
    int previousId;
    float frameOffset[9];
    float focalPointX;
    float focalPointY;
    float foveationGainX;
    float foveationGainY;
    float foveationArea;
    float foveationMinimum;
    int surfaceValidWaitFramesCounter;
    const int kSurfaceValidWaitFrames;
    const int kPoseCount;

public:
    SvrUnity() :
            isInitialized(false),
            isRunning(false),
            activity(0),
            threadTid(0),
            javaVm(0),
            isInitializeDataSet(false),
            isFrameParamsDataSet(false),
            isSetTrackingPoseDataSet(false),
            trackingPoseMode(1),
            cpuPerfLevel(0),
            gpuPerfLevel(0),
            colorSpace(0),
            isSetPerformanceLevelsDataSet(false),
            eyeSideMask(0),
            eyeLayerMask(0),
            isEyeDataSet(false),
            nEyeBuffers(0),
            textureId(0),
            previousId(0),
            focalPointX(0.0f),
            focalPointY(0.0f),
            foveationGainX(1.0f),
            foveationGainY(1.0f),
            foveationArea(0.0f),
            foveationMinimum(0.25f),
            surfaceValidWaitFramesCounter(0),
            kSurfaceValidWaitFrames(30),
            kPoseCount(3) {
        memset(&frameParams, 0, sizeof(frameParams));
        memset(&headPoses, 0, sizeof(headPoses));
        memset(&eyePoses, 0, sizeof(eyePoses));
        memset(&eyeBufferList[0], 0, sizeof(eyeBufferList));
        memset(&frameOffset[0], 0.0f, sizeof(frameOffset));
    }

    bool IsFoveatedEnabled() {
        return (foveationGainX >= 1 && foveationGainY >= 1 /*&& foveationArea >= 0*/);
    }

    void ResetFoveatedData() {
        nEyeBuffers = 0;
        memset(&eyeBufferList[0], 0, sizeof(eyeBufferList));
    }
};

SvrUnity plugin;

void SvrEndVrEvent();

void SvrShutdownEvent();

JNIEnv *GetJNIEnv(JavaVM *javaVM) {
    JNIEnv *jniEnv;
    javaVM->AttachCurrentThread(&jniEnv, NULL);
    return jniEnv;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LogDebug("JNI_OnLoad() called!");
    JNIEnv *javaEnv;
    if (vm->GetEnv(reinterpret_cast<void **>(&javaEnv), JNI_VERSION_1_2) != JNI_OK) {
        return -1;
    }

    // Get jclass with env->FindClass.
    // Register methods with env->RegisterNatives.
    plugin.javaVm = vm;
    return JNI_VERSION_1_2;
}

jobject GetViewSurface(JNIEnv *javaEnv, jobject activity) {
    jclass activityClass = javaEnv->GetObjectClass(activity);
    if (activityClass == NULL) {
        LogError("activityClass == NULL!");
        return NULL;
    }

    jfieldID fid = javaEnv->GetFieldID(activityClass, "mUnityPlayer",
                                       "Lcom/unity3d/player/UnityPlayer;");
    if (fid == NULL) {
        LogError("mUnityPlayer not found!");
        return NULL;
    }

    jobject unityPlayerObj = javaEnv->GetObjectField(activity, fid);
    if (unityPlayerObj == NULL) {
        LogError("unityPlayer object not found!");
        return NULL;
    }

    jclass unityPlayerClass = javaEnv->GetObjectClass(unityPlayerObj);
    if (unityPlayerClass == NULL) {
        LogError("unityPlayer class not found!");
        return NULL;
    }

    jmethodID mid = javaEnv->GetMethodID(unityPlayerClass, "getChildAt", "(I)Landroid/view/View;");
    if (mid == NULL) {
        LogError("getChildAt methodID not found!");
        return NULL;
    }

    jboolean param = 0;
    jobject surfaceViewObj = javaEnv->CallObjectMethod(unityPlayerObj, mid, param);
    if (surfaceViewObj == NULL) {
        LogError("surfaceView object not found!");
        return NULL;
    }

    jclass surfaceViewClass = javaEnv->GetObjectClass(surfaceViewObj);
    mid = javaEnv->GetMethodID(surfaceViewClass, "getHolder", "()Landroid/view/SurfaceHolder;");
    if (mid == NULL) {
        LogError("getHolder methodID not found!");
        return NULL;
    }

    jobject surfaceHolderObj = javaEnv->CallObjectMethod(surfaceViewObj, mid);
    if (surfaceHolderObj == NULL) {
        LogError("surfaceHolder object not found!");
        return NULL;
    }

    jclass surfaceHolderClass = javaEnv->GetObjectClass(surfaceHolderObj);
    mid = javaEnv->GetMethodID(surfaceHolderClass, "getSurface", "()Landroid/view/Surface;");
    if (mid == NULL) {
        LogError("getSurface methodID not found!");
        return NULL;
    }
    jobject surface = javaEnv->CallObjectMethod(surfaceHolderObj, mid);
    javaEnv->DeleteLocalRef(activityClass);
    javaEnv->DeleteLocalRef(unityPlayerObj);
    javaEnv->DeleteLocalRef(unityPlayerClass);
    javaEnv->DeleteLocalRef(surfaceViewObj);
    javaEnv->DeleteLocalRef(surfaceViewClass);
    javaEnv->DeleteLocalRef(surfaceHolderObj);
    javaEnv->DeleteLocalRef(surfaceHolderClass);
    return surface;
}

void SvrInitializeEvent() {
    LogDebug("SvrInitializeEvent() called!");

    if (!plugin.isInitializeDataSet) {
        LogError(
                "SvrInitializeEventData() must be called first before calling SvrInitializeEvent!");
        return;
    }

    svrInitParams params;
    memset(&params, 0, sizeof(params));
    params.javaActivityObject = plugin.activity;
    params.javaVm = plugin.javaVm;
    params.javaEnv = GetJNIEnv(plugin.javaVm);

    if (params.javaActivityObject == NULL) {
        LogError("params.javaActivityObject is NULL");
        return;
    }

    if (params.javaVm == NULL) {
        LogError("params.javaVm is NULL");
        return;
    }

    if (params.javaEnv == NULL) {
        LogError("params.javaEnv is NULL");
        return;
    }

    if (svrInitialize(&params) != SVR_ERROR_NONE) {
        LogError("svr initialization failed!");
        return;
    }

    Log("Checking for glTextureFoveationParametersQCOM support...");
    glTextureFoveationParametersQCOM = (PFNGLTEXTUREFOVEATIONPARAMETERSEXT) eglGetProcAddress(
            "glTextureFoveationParametersQCOM");
    if (glTextureFoveationParametersQCOM != NULL) {
        Log("    glTextureFoveationParametersQCOM SUPPORTED");
    } else {
        LogError("    glTextureFoveationParametersQCOM is NOT SUPPORTED");
    }

    if (glTextureFoveationParametersQCOM == NULL) {
        Log("Checking for glFramebufferFoveationConfigQCOM support...");
        glFramebufferFoveationConfigQCOM = (PFNGLFRAMEBUFFERFOVEATIONCONFIGEXT) eglGetProcAddress(
                "glFramebufferFoveationConfigQCOM");
        if (glFramebufferFoveationConfigQCOM != 0) {
            Log("    glFramebufferFoveationConfigQCOM SUPPORTED");
        } else {
            LogError("    glFramebufferFoveationConfigQCOM is NOT SUPPORTED");
        }

        Log("Checking for glFramebufferFoveationParametersEXT support...");
        glFramebufferFoveationParametersQCOM = (PFNGLFRAMEBUFFERFOVEATIONPARAMETERSEXT) eglGetProcAddress(
                "glFramebufferFoveationParametersQCOM");
        if (glFramebufferFoveationParametersQCOM != 0) {
            Log("    glFramebufferFoveationParametersQCOM SUPPORTED");
        } else {
            LogError("    glFramebufferFoveationParametersQCOM is NOT SUPPORTED");
        }
    }

    plugin.ResetFoveatedData();

    plugin.isInitialized = true;
}

void SvrBeginVrEvent() {
    LogDebug("SvrBeginVrEvent!");

    if (!plugin.isInitialized) {
        LogError("Initialize svrApi with SvrInitializeEvent before calling SvrBeginVrEvent!");
        return;
    }

    JNIEnv *javaEnv = GetJNIEnv(plugin.javaVm);
    jobject surface = GetViewSurface(javaEnv, plugin.activity);
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(javaEnv, surface);

    svrBeginParams params;
    memset(&params, 0, sizeof(params));
    params.cpuPerfLevel = (svrPerfLevel) plugin.cpuPerfLevel;
    params.gpuPerfLevel = (svrPerfLevel) plugin.gpuPerfLevel;
    params.mainThreadId = plugin.threadTid;
    params.nativeWindow = nativeWindow;
    params.colorSpace = (svrColorSpace) plugin.colorSpace;

    if (svrBeginVr(&params) != SVR_ERROR_NONE) {
        LogError("svr begin VR failed!");
        ANativeWindow_release(nativeWindow);
        javaEnv->DeleteLocalRef(surface);
        return;
    }

    ANativeWindow_release(nativeWindow);
    javaEnv->DeleteLocalRef(surface);

    plugin.surfaceValidWaitFramesCounter = 0;
    plugin.isRunning = true;
}

void SvrEndVrEvent() {
    LogDebug("SvrEndVrEvent!");

    if (svrEndVr() != SVR_ERROR_NONE) {
        LogError("svr end VR failed!");
        return;
    }

    plugin.isRunning = false;
}

void SvrBeginEyeEvent() {
    LogDebug("SvrBeginEyeEvent!");

    if (!plugin.isRunning) {
        LogError("SvrBeginEyeEvent called prior to successful SvrBeginVrEvent!");
        return;
    }

    GLenum err;
    if (glTextureFoveationParametersQCOM != NULL && plugin.textureId > 0 &&
        plugin.IsFoveatedEnabled()) {
        bool bEyeInitialized = false;
        for (int i = 0; i < plugin.nEyeBuffers; i++) {
            if (plugin.eyeBufferList[i] == plugin.textureId) {
                bEyeInitialized = true;
                break;
            }
        }

        if (!bEyeInitialized) {
            Log("BeginEye Texture Foveation - textureId %d, previousId %d", plugin.textureId,
                plugin.previousId);

            glBindTexture(GL_TEXTURE_2D, plugin.textureId);
            glTexParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM,
                            GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM);
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PREVIOUS_SOURCE_TEXTURE_QCOM, plugin.previousId);
//            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM, plugin.foveationMinimum);
//            err = glGetError();
//            if (err != GL_NO_ERROR)
//            {
//                LogError("glTexParameteri Error : 0x%x", err);
//            }

            glTextureFoveationFrameOffsetQCOM = true;
//            glBindTexture(GL_TEXTURE_2D, plugin.textureId);
//            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_FOVEATED_FRAME_OFFSET_QCOM, plugin.frameOffset);
//            err = glGetError();
//            if (err != GL_NO_ERROR)
//            {
//                LogError("glTexParameterfv Error : 0x%x", err);
//                glTextureFoveationFrameOffsetQCOM = false;
//            }

            int eyeBufferIndex = plugin.nEyeBuffers++ % MAX_EYE_BUFFERS; // wrap index
            plugin.eyeBufferList[eyeBufferIndex] = plugin.textureId;
        }

        if (glTextureFoveationFrameOffsetQCOM == true) {
            glBindTexture(GL_TEXTURE_2D, plugin.textureId);
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_FOVEATED_FRAME_OFFSET_QCOM,
                             plugin.frameOffset);
            err = glGetError();
            if (err != GL_NO_ERROR) {
                LogError("glTexParameterfv Error : %d", err);
            }
        }
    }

    //Intentionally commented - No point to call svrBeginEye for stencil because clobbered by unity camera screen clear anyway
    //if (!plugin.isEyeDataSet)
    //{
    //    LogError("SvrBeginEyeEvent called prior to SvrSetEyeEventData!");
    //    return;
    //}

    //// Call svrBeginEye(whichEye) given the current eye mask
    //svrWhichEye whichEye = (plugin.eyeSideMask & kEyeMaskRight) == kEyeMaskRight ? kRightEye : kLeftEye;
    //if (svrBeginEye(whichEye) != SVR_ERROR_NONE)
    //{
    //    LogError("svr begin eye failed!");
    //}

    //plugin.isEyeDataSet = false;
}

void SvrEndEyeEvent() {
    LogDebug("SvrEndEyeEvent!");

    if (!plugin.isRunning) {
        LogError("SvrEndEyeEvent called prior to successful SvrBeginVrEvent!");
        return;
    }
    GLenum err;

    if ((glFramebufferFoveationConfigQCOM != NULL) &&
        (glFramebufferFoveationParametersQCOM != NULL) && plugin.IsFoveatedEnabled()) {
        GLint drawFboId;

        //Get the currently bound fbo
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &drawFboId);

        bool bEyeInitialized = false;
        for (int i = 0; i < plugin.nEyeBuffers; i++) {
            if (plugin.eyeBufferList[i] == drawFboId) {
                bEyeInitialized = true;
                break;
            }
        }

        err = glGetError();
        if (err != GL_NO_ERROR) {
            LogDebug("    Pre Foveation Error : %d", err);
        }

        if (!bEyeInitialized) {
            Log("EndEye Framebuffer Foveation - fboId %d", drawFboId);

            GLuint requested = GL_FOVEATION_ENABLE_BIT_QCOM;
            GLuint supported = 0;

            glFramebufferFoveationConfigQCOM(drawFboId, 1, 1, requested, &supported);
            Log("   Foveation Support : 0x%x", supported);

            err = glGetError();
            if (err != GL_NO_ERROR) {
                LogError("glFramebufferFoveationConfigQCOM Error : %d", err);
            } else {
                int eyeBufferIndex = plugin.nEyeBuffers++ % MAX_EYE_BUFFERS; // wrap index
                plugin.eyeBufferList[eyeBufferIndex] = drawFboId;
            }
        }

        glFramebufferFoveationParametersQCOM(drawFboId, 0, 0, plugin.focalPointX,
                                             plugin.focalPointY,
                                             plugin.foveationGainX, plugin.foveationGainY,
                                             plugin.foveationArea);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LogError("glFramebufferFoveationParametersQCOM Error : %d", err);
        }
    }

    GLenum invalidateList[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    GLsizei invalidateListCount = 2;
    glInvalidateFramebuffer(GL_FRAMEBUFFER, invalidateListCount, invalidateList);

    //glFlush();

    if (!plugin.isEyeDataSet) {
        LogError("SvrEndEyeEvent called prior to SvrSetEyeEventData!");
        return;
    }

    // Call svrEndEye(whichEye) given the current eye mask for non-overlay layers
    if ((plugin.eyeLayerMask & kLayerFlagHeadLocked) == 0) {
        svrWhichEye whichEye =
                (plugin.eyeSideMask & kEyeMaskRight) == kEyeMaskRight ? kRightEye : kLeftEye;
        if (svrEndEye(whichEye) != SVR_ERROR_NONE) {
            LogError("svr end eye failed!");
        }
    }

    plugin.isEyeDataSet = false;
}

void SvrSubmitFrameEvent() {
    LogDebug("SvrSubmitFrameEvent!");

    if (!plugin.isInitialized || !plugin.isRunning || !plugin.isFrameParamsDataSet)
        return;

    svrFrameParams params = plugin.frameParams;
    params.frameIndex = plugin.frameParams.frameIndex;
    int poseIndex = params.frameIndex % plugin.kPoseCount;
    params.headPoseState = plugin.headPoses[poseIndex];
    params.warpType = kSimple;
    svrSubmitFrame(&params);

    plugin.isFrameParamsDataSet = false;    // reset for this frame
}

void SvrFoveationEvent() {
    LogDebug("SvrFoveationEvent!");

    if (glTextureFoveationParametersQCOM != NULL && plugin.textureId > 0 &&
        plugin.IsFoveatedEnabled()) {
        glTextureFoveationParametersQCOM(plugin.textureId, 0, 0, plugin.focalPointX,
                                         plugin.focalPointY,
                                         plugin.foveationGainX, plugin.foveationGainY,
                                         plugin.foveationArea);
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            LogError("glTextureFoveationParametersQCOM Error : %d", err);
        }
    }
}

void SvrShutdownEvent() {
    LogDebug("SvrShutdownEvent!");

    svrShutdown();

    JNIEnv *javaEnv = GetJNIEnv(plugin.javaVm);
    javaEnv->DeleteGlobalRef(plugin.activity);
    plugin.activity = NULL;

    plugin.isInitialized = false;
}

void SvrRecenterTrackingEvent() {
    LogDebug("SvrRecenterTrackingEvent!");
    svrRecenterPose();
}

void SvrSetTrackingModeEvent() {
    LogDebug("SvrSetTrackingModeEvent!");

    if (!plugin.isSetTrackingPoseDataSet) {
        LogError(
                "SvrSetTrackingModeEventData() must be called first before calling SvrSetTrackingModeEvent!");
        return;
    }

    svrSetTrackingMode(plugin.trackingPoseMode);    // Request the desired tracking mode

    plugin.trackingPoseMode = svrGetTrackingMode(); // Read the actual tracking mode

    plugin.isSetTrackingPoseDataSet = false;
}

void SvrSetPerformanceLevelsEvent() {
    LogDebug("SvrSetPerformanceLevelsEvent!");

    if (!plugin.isSetPerformanceLevelsDataSet) {
        LogError(
                "SvrSetPerformanceLevelsEventData() must be called first before calling SvrSetPerformanceLevelsEvent!");
        return;
    }

    svrPerfLevel cpuPerfLevel = kPerfSystem;
    svrPerfLevel gpuPerfLevel = kPerfSystem;

    // SVR version of perf level
    switch (plugin.cpuPerfLevel) {
        case 0:
        default:
            cpuPerfLevel = kPerfSystem;
            break;

        case 1:
            cpuPerfLevel = kPerfMinimum;
            break;

        case 2:
            cpuPerfLevel = kPerfMedium;
            break;

        case 3:
            cpuPerfLevel = kPerfMaximum;
            break;
    }

    // SVR version of perf level
    switch (plugin.gpuPerfLevel) {
        case 0:
        default:
            gpuPerfLevel = kPerfSystem;
            break;

        case 1:
            gpuPerfLevel = kPerfMinimum;
            break;

        case 2:
            gpuPerfLevel = kPerfMedium;
            break;

        case 3:
            gpuPerfLevel = kPerfMaximum;
            break;
    }

    // Set SVR values
    svrSetPerformanceLevels(cpuPerfLevel, gpuPerfLevel);

    plugin.isSetPerformanceLevelsDataSet = false;
}


extern "C"
{

SVR_EXPORT bool SvrIsInitialized() {
    return plugin.isInitialized;
}

SVR_EXPORT bool SvrIsRunning() {
    return plugin.isRunning;
}

SVR_EXPORT bool SvrCanBeginVR() {
    JNIEnv *javaEnv = GetJNIEnv(plugin.javaVm);
    jobject surface = GetViewSurface(javaEnv, plugin.activity);

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(javaEnv, surface);

    int currentSurfaceHeight = ANativeWindow_getHeight(nativeWindow);
    svrDeviceInfo info = svrGetDeviceInfo();

    if (currentSurfaceHeight != info.displayHeightPixels) {
        LogDebug("Current Surface Height: %d  - DeviceInfoHeight: %d", (int) currentSurfaceHeight,
                 info.displayHeightPixels);

        ANativeWindow_release(nativeWindow);
        javaEnv->DeleteLocalRef(surface);
        return false;
    }


    LogDebug("Current Surface Height: %d  - DeviceInfoHeight: %d", (int) currentSurfaceHeight,
             info.displayHeightPixels);

    ANativeWindow_release(nativeWindow);
    plugin.surfaceValidWaitFramesCounter++;
    javaEnv->DeleteLocalRef(surface);

    if (plugin.surfaceValidWaitFramesCounter >= plugin.kSurfaceValidWaitFrames) {
        return true;
    }

    return false;
}

SVR_EXPORT void SvrInitializeEventData(jobject activity) {
    LogDebug("SvrSetInitParams() called!");
    JNIEnv *jniEnv = GetJNIEnv(plugin.javaVm);
    plugin.threadTid = gettid();
    plugin.activity = jniEnv->NewGlobalRef(activity);
    plugin.isInitializeDataSet = true;
}

SVR_EXPORT void SvrSubmitFrameEventData(int frameIndex, float fieldOfView = 0/*use device fov*/,
                                        int frameType = 1/*kEyeBufferStereoSeparate*/) {
    LogDebug("SvrSubmitFrameEventData() called!");

    plugin.frameParams.frameIndex = frameIndex;
    plugin.frameParams.fieldOfView = fieldOfView;
    plugin.frameType = frameType;

    plugin.isFrameParamsDataSet = true;
}

SVR_EXPORT void SvrSetFrameOffset(float *frameDelta) {
    for (int i = 0; i < 9; i++) plugin.frameOffset[i] = frameDelta[i];
}

SVR_EXPORT void
SvrSetFoveationParameters(int textureId, int previousId, float focalPointX, float focalPointY,
                          float foveationGainX, float foveationGainY, float foveationArea,
                          float foveationMinimum) {
    plugin.textureId = textureId;
    plugin.previousId = previousId;
    plugin.focalPointX = focalPointX;
    plugin.focalPointY = focalPointY;
    plugin.foveationGainX = foveationGainX;
    plugin.foveationGainY = foveationGainY;
    plugin.foveationArea = foveationArea;
    plugin.foveationMinimum = foveationMinimum;
}

SVR_EXPORT void SvrSetTrackingModeEventData(int mode) {
    LogDebug("SvrSetTrackingModeEventData() called!");

    plugin.trackingPoseMode = mode;
    plugin.isSetTrackingPoseDataSet = true;
}

SVR_EXPORT void SvrSetPerformanceLevelsEventData(int newCpuPerfLevel, int newGpuPerfLevel) {
    LogDebug("SvrSetPerformanceLevelsEventData() called!");

    plugin.cpuPerfLevel = newCpuPerfLevel;
    plugin.gpuPerfLevel = newGpuPerfLevel;
    plugin.isSetPerformanceLevelsDataSet = true;
}

SVR_EXPORT void SvrSetEyeEventData(int sideMask, int layerMask) {
    LogDebug("SvrSetEyeEventData() called!");

    plugin.eyeSideMask = sideMask;
    plugin.eyeLayerMask = layerMask;
    plugin.isEyeDataSet = true;
}

SVR_EXPORT void SvrSetColorSpace(int colorSpace) {
    //Note that the incoming colorspace is the desired color space for the display buffer which is the inverse of 
    //the color space used during rendering.  A gamma eyebuffer requires a linear display surface and vise versa.
    plugin.colorSpace = colorSpace;
}

SVR_EXPORT void SvrSetFrameOption(unsigned int frameOption) {
    plugin.frameParams.frameOptions |= frameOption;
}

SVR_EXPORT void SvrUnsetFrameOption(unsigned int frameOption) {
    plugin.frameParams.frameOptions &= ~frameOption;
}

SVR_EXPORT void SvrSetVSyncCount(int vSyncCount) {
    plugin.frameParams.minVsyncs = vSyncCount;
}

SVR_EXPORT int SvrGetPredictedPose(float &rotX, float &rotY, float &rotZ, float &rotW,
                                   float &posX, float &posY, float &posZ,
                                   int frameIndex, bool isMultiThreadedRender) {
    LogDebug("SvrGetPredictedPose() called!");

    int pipelineDepth = isMultiThreadedRender ? 2 : 1;

    float predictedTimeMs = svrGetPredictedDisplayTimePipelined(pipelineDepth);
    svrHeadPoseState poseState = svrGetPredictedHeadPose(predictedTimeMs);
    svrHeadPose *poseHead = &poseState.pose;
    int poseStatus = poseState.poseStatus;

    if ((poseStatus & kTrackingRotation) == kTrackingRotation) {
        rotX = poseHead->rotation.x;
        rotY = poseHead->rotation.y;
        rotZ = poseHead->rotation.z;
        rotW = poseHead->rotation.w;
    }

    if ((poseStatus & kTrackingPosition) == kTrackingPosition) {
        posX = poseHead->position.x;
        posY = poseHead->position.y;
        posZ = poseHead->position.z;
    }

    // Cache svrHeadPoseState if frameIndex provided
    if (frameIndex >= 0) {
        int poseIndex = frameIndex % plugin.kPoseCount;
        plugin.headPoses[poseIndex] = poseState;
    }

    return poseStatus;
}


SVR_EXPORT int SvrGetPointCloudData(int &dataNum, uint64_t &dataTimestamp, float *dataArray) {
    int res = svrGetPointCloudData(dataNum, dataTimestamp, dataArray);
    LogError("TEST SvrGetPointCloudData dataNum=%d res=%d", dataNum, res);
    if (res != 0) {
        return -1;
    }
    LogError("TEST SvrGetPointCloudData done");
    return 0;
}

SVR_EXPORT int SvrGetEyePose(
        int &leftStatus, int &rightStatus, int &combinedStatus,
        float &leftOpenness, float &rightOpenness,
        float &leftDirX, float &leftDirY, float &leftDirZ,
        float &leftPosX, float &leftPosY, float &leftPosZ,
        float &rightDirX, float &rightDirY, float &rightDirZ,
        float &rightPosX, float &rightPosY, float &rightPosZ,
        float &combinedDirX, float &combinedDirY, float &combinedDirZ,
        float &combinedPosX, float &combinedPosY, float &combinedPosZ,
        int frameIndex) {
    LogDebug("SvrGetEyePose() called!");

    svrEyePoseState poseState;
    if (svrGetEyePose(&poseState) != SVR_ERROR_NONE) {
        return 0;
    }

    int poseStatus = 0;

    //If any data for each of the left/right/combined eyes is valid,
    //use it.
    //Note that we will return success (kTrackingEye bit set)
    //as long as one of the eyes has valid data.

    rightOpenness = poseState.rightEyeOpenness;
    leftOpenness = poseState.leftEyeOpenness;

    leftStatus = poseState.leftEyePoseStatus;
    if ((poseState.leftEyePoseStatus & kGazePointValid) &&
        (poseState.leftEyePoseStatus & kGazeVectorValid)) {
        leftPosX = poseState.leftEyeGazePoint[0];
        leftPosY = poseState.leftEyeGazePoint[1];
        leftPosZ = poseState.leftEyeGazePoint[2];

        leftDirX = poseState.leftEyeGazeVector[0];
        leftDirY = poseState.leftEyeGazeVector[1];
        leftDirZ = poseState.leftEyeGazeVector[2];

        poseStatus |= kTrackingEye;
    }

    rightStatus = poseState.rightEyePoseStatus;
    if ((poseState.rightEyePoseStatus & kGazePointValid) &&
        (poseState.rightEyePoseStatus & kGazeVectorValid)) {
        rightPosX = poseState.rightEyeGazePoint[0];
        rightPosY = poseState.rightEyeGazePoint[1];
        rightPosZ = poseState.rightEyeGazePoint[2];

        rightDirX = poseState.rightEyeGazeVector[0];
        rightDirY = poseState.rightEyeGazeVector[1];
        rightDirZ = poseState.rightEyeGazeVector[2];

        poseStatus |= kTrackingEye;
    }

    combinedStatus = poseState.combinedEyePoseStatus;
    if ((poseState.combinedEyePoseStatus & kGazePointValid) &&
        (poseState.combinedEyePoseStatus & kGazeVectorValid)) {
        combinedPosX = poseState.combinedEyeGazePoint[0];
        combinedPosY = poseState.combinedEyeGazePoint[1];
        combinedPosZ = poseState.combinedEyeGazePoint[2];

        combinedDirX = poseState.combinedEyeGazeVector[0];
        combinedDirY = poseState.combinedEyeGazeVector[1];
        combinedDirZ = poseState.combinedEyeGazeVector[2];

        poseStatus |= kTrackingEye;
    }

    // Cache svrEyePoseState if frameIndex provided
    if (frameIndex >= 0) {
        int poseIndex = frameIndex % plugin.kPoseCount;
        plugin.eyePoses[poseIndex] = poseState;
    }

    return poseStatus;
}

SVR_EXPORT bool SvrRecenterTrackingPose() {
    LogDebug("SvrRecenterTrackingPose() called!");

    return svrRecenterPose() == SVR_ERROR_NONE;
}

SVR_EXPORT int SvrGetTrackingMode() {
    return plugin.trackingPoseMode;
}

SVR_EXPORT void
SvrGetDeviceInfo(int &displayWidthPixels, int &displayHeightPixels, float &displayRefreshRateHz,
                 int &targetEyeWidthPixels, int &targetEyeHeightPixels,
                 float &targetFovXRad, float &targetFovYRad,
                 float &leftEyeFrustumLeft, float &leftEyeFrustumRight, float &leftEyeFrustumBottom,
                 float &leftEyeFrustumTop, float &leftEyeFrustumNear, float &leftEyeFrustumFar,
                 float &rightEyeFrustumLeft, float &rightEyeFrustumRight,
                 float &rightEyeFrustumBottom, float &rightEyeFrustumTop,
                 float &rightEyeFrustumNear, float &rightEyeFrustumFar,
                 float &targetEyeConvergence, float &targetEyePitch) {
    LogDebug("SvrGetDeviceInfo() called!");

    svrDeviceInfo info = svrGetDeviceInfo();

    displayWidthPixels = info.displayWidthPixels;
    displayHeightPixels = info.displayHeightPixels;
    displayRefreshRateHz = info.displayRefreshRateHz;
    targetEyeWidthPixels = info.targetEyeWidthPixels;
    targetEyeHeightPixels = info.targetEyeHeightPixels;
    targetFovXRad = info.targetFovXRad;
    targetFovYRad = info.targetFovYRad;
    leftEyeFrustumLeft = info.leftEyeFrustum.left;
    leftEyeFrustumRight = info.leftEyeFrustum.right;
    leftEyeFrustumTop = info.leftEyeFrustum.top;
    leftEyeFrustumBottom = info.leftEyeFrustum.bottom;
    leftEyeFrustumNear = info.leftEyeFrustum.near;
    leftEyeFrustumFar = info.leftEyeFrustum.far;
    rightEyeFrustumLeft = info.rightEyeFrustum.left;
    rightEyeFrustumRight = info.rightEyeFrustum.right;
    rightEyeFrustumTop = info.rightEyeFrustum.top;
    rightEyeFrustumBottom = info.rightEyeFrustum.bottom;
    rightEyeFrustumNear = info.rightEyeFrustum.near;
    rightEyeFrustumFar = info.rightEyeFrustum.far;
    targetEyeConvergence = info.targetEyeConvergence;
    targetEyePitch = info.targetEyePitch;
}

SVR_EXPORT void
SvrSetupLayerData(int layerIndex, int sideMask, int textureId, int textureType, int layerFlags) {
    svrRenderLayer *pLayers = plugin.frameParams.renderLayers;
    int numLayers = SVR_MAX_RENDER_LAYERS;

    // -1/layerIndex => apply to all
    if (layerIndex < 0) {
        for (int i = 0; i < numLayers; i++) {
            svrRenderLayer *pLayer = &pLayers[i];
            pLayer->layerFlags = (svrLayerFlags) layerFlags;
            pLayer->eyeMask = (svrEyeMask) sideMask;
            pLayer->imageHandle = textureId;
            pLayer->imageType = (svrTextureType) textureType;
        }
    } else {
        if (layerIndex < numLayers) {
            svrRenderLayer *pLayer = &pLayers[layerIndex];
            pLayer->layerFlags = (svrLayerFlags) layerFlags;
            pLayer->eyeMask = (svrEyeMask) sideMask;
            pLayer->imageHandle = textureId;
            pLayer->imageType = (svrTextureType) textureType;
        }
    }
}

SVR_EXPORT void
SvrSetupLayerCoords(int layerIndex, float lowerLeft[], float lowerRight[], float upperLeft[],
                    float upperRight[]) {
    LogDebug("SvrSetLayerCoords() called for index: %d!", layerIndex);

    float lowerLeftPos[4] = {lowerLeft[0], lowerLeft[1], lowerLeft[2], lowerLeft[3]};
    float lowerRightPos[4] = {lowerRight[0], lowerRight[1], lowerRight[2], lowerRight[3]};
    float upperLeftPos[4] = {upperLeft[0], upperLeft[1], upperLeft[2], upperLeft[3]};
    float upperRightPos[4] = {upperRight[0], upperRight[1], upperRight[2], upperRight[3]};

    float lowerUVs[4] = {lowerLeft[4], lowerLeft[5], lowerRight[4], lowerRight[5]};
    float upperUVs[4] = {upperLeft[4], upperLeft[5], upperRight[4], upperRight[5]};

    float identTransform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};

    svrRenderLayer *pLayers = plugin.frameParams.renderLayers;
    int numLayers = SVR_MAX_RENDER_LAYERS;

    // -1/layerIndex => apply to all
    if (layerIndex < 0) {
        for (int i = 0; i < numLayers; i++) {
            svrLayoutCoords *pLayout = &pLayers[i].imageCoords;

            memcpy(pLayout->LowerLeftPos, lowerLeftPos, sizeof(lowerLeftPos));
            memcpy(pLayout->LowerRightPos, lowerRightPos, sizeof(lowerRightPos));
            memcpy(pLayout->UpperLeftPos, upperLeftPos, sizeof(upperLeftPos));
            memcpy(pLayout->UpperRightPos, upperRightPos, sizeof(upperRightPos));

            memcpy(pLayout->LowerUVs, lowerUVs, sizeof(lowerUVs));
            memcpy(pLayout->UpperUVs, upperUVs, sizeof(upperUVs));

            memcpy(pLayout->TransformMatrix, identTransform, sizeof(pLayout->TransformMatrix));
        }
    } else {
        if (layerIndex < numLayers) {
            svrLayoutCoords *pLayout = &pLayers[layerIndex].imageCoords;

            memcpy(pLayout->LowerLeftPos, lowerLeftPos, sizeof(lowerLeftPos));
            memcpy(pLayout->LowerRightPos, lowerRightPos, sizeof(lowerRightPos));
            memcpy(pLayout->UpperLeftPos, upperLeftPos, sizeof(upperLeftPos));
            memcpy(pLayout->UpperRightPos, upperRightPos, sizeof(upperRightPos));

            memcpy(pLayout->LowerUVs, lowerUVs, sizeof(lowerUVs));
            memcpy(pLayout->UpperUVs, upperUVs, sizeof(upperUVs));

            memcpy(pLayout->TransformMatrix, identTransform, sizeof(pLayout->TransformMatrix));
        }
    }
}

//struct svrEvent
//{
//    svrEventType    eventType;              //!< Type of event
//    unsigned int    deviceId;               //!< An identifier for the device that generated the event (0 == HMD)
//    float           eventTimeStamp;         //!< Time stamp for the event in seconds since the last svrBeginVr call
//    svrEventData    eventData;              //!< Event specific data payload
//};
SVR_EXPORT bool
SvrPollEvent(int &eventType, unsigned int &deviceId, float &eventTimeStamp, int eventDataCount,
             unsigned int eventData[]) {
    svrEvent frameEvent;

    bool isEvent = svrPollEvent(&frameEvent);
    if (isEvent) {
        LogDebug("SvrPollEvent() type: %d, deviceId: %d, timeStamp: %f, data: %d %d!",
                 frameEvent.eventType, frameEvent.deviceId, frameEvent.eventTimeStamp,
                 frameEvent.eventData.data[0], frameEvent.eventData.data[1]);

        eventType = frameEvent.eventType;
        deviceId = frameEvent.deviceId;
        eventTimeStamp = frameEvent.eventTimeStamp;
        //eventData = frameEvent.eventData.data;
        for (int i = 0; i < eventDataCount; i++) eventData[i] = frameEvent.eventData.data[i];
    }
    return isEvent;
}


enum RenderEventType {
    InitializeEvent,
    BeginVrEvent,
    EndVrEvent,
    BeginEyeEvent,
    EndEyeEvent,
    SubmitFrameEvent,
    FoveationEvent,
    ShutdownEvent,
    RecenterTrackingEvent,
    SetTrackingModeEvent,
    SetPerformanceLevelsEvent
};

// Plugin function to handle a specific rendering event
static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
    switch (eventID) {
        case InitializeEvent:
            SvrInitializeEvent();
            break;
        case BeginVrEvent:
            SvrBeginVrEvent();
            break;
        case EndVrEvent:
            SvrEndVrEvent();
            break;
        case BeginEyeEvent:
            SvrBeginEyeEvent();
            break;
        case EndEyeEvent:
            SvrEndEyeEvent();
            break;
        case SubmitFrameEvent:
            SvrSubmitFrameEvent();
            break;
        case FoveationEvent:
            SvrFoveationEvent();
            break;
        case ShutdownEvent:
            SvrShutdownEvent();
            break;
        case RecenterTrackingEvent:
            SvrRecenterTrackingEvent();
            break;
        case SetTrackingModeEvent:
            SvrSetTrackingModeEvent();
            break;
        case SetPerformanceLevelsEvent:
            SvrSetPerformanceLevelsEvent();
            break;
        default:
            LogError("Event with id:%i not supported!", eventID);
            break;
    }
}

// Freely defined function to pass a callback to plugin-specific scripts
UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
    return OnRenderEvent;
}

//----------------------------------------------------------------------------------------------------
SVR_EXPORT int SvrControllerStartTracking(const char *controllerId)
//----------------------------------------------------------------------------------------------------
{
    return svrControllerStartTracking(controllerId);
}

//----------------------------------------------------------------------------------------------------
SVR_EXPORT void SvrControllerGetState(int handle, int space, svrControllerState &data)
//----------------------------------------------------------------------------------------------------
{
    data = svrControllerGetState(handle, space);
}

//----------------------------------------------------------------------------------------------------
SVR_EXPORT void SvrControllerStopTracking(int handle)
//----------------------------------------------------------------------------------------------------
{
    svrControllerStopTracking(handle);
}

//----------------------------------------------------------------------------------------------------
SVR_EXPORT void SvrControllerSendMessage(int handle, int what, int arg1, int arg2)
//----------------------------------------------------------------------------------------------------
{
    svrControllerSendMessage(handle, what, arg1, arg2);
}

//----------------------------------------------------------------------------------------------------
SVR_EXPORT int SvrControllerQuery(int handle, int what, int *memory, unsigned int memSize)
//----------------------------------------------------------------------------------------------------
{
    return svrControllerQuery(handle, what, memory, memSize);
}

SVR_EXPORT int ScInitLayer() {
    return scInitLayer();
}

SVR_EXPORT int ScStartLayerRendering() {
    return scStartLayerRendering();
}

SVR_EXPORT int ScGetAllLayersData(SCAllLayers *outAllLayers) {
    int res = scGetAllLayersData(outAllLayers);
    Log("svrUnity.cpp ScGetAllLayersData outAllLayers->num=%d res=%d", outAllLayers->layerNum, res);
    for (int layerIdx = 0; layerIdx != outAllLayers->layerNum; ++layerIdx) {
        Log("svrUnity.cpp ScGetAllLayersData No.%d layerId=%u, parentId=%d layerTextureId=%u, editFlag=%d, z=%d, alpha=%f, bOpaque=%d, "
            "modelMatrixData=[%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
            layerIdx, outAllLayers->layerData[layerIdx].layerId,
            outAllLayers->layerData[layerIdx].parentLayerId,
            outAllLayers->layerData[layerIdx].layerTextureId,
            outAllLayers->layerData[layerIdx].editFlag, outAllLayers->layerData[layerIdx].z,
            outAllLayers->layerData[layerIdx].alpha, outAllLayers->layerData[layerIdx].bOpaque,
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
    }
    return res;
}

SVR_EXPORT int ScEndLayerRendering(SCAllLayers *allLayers) {
    return scEndLayerRendering(allLayers);
}

SVRP_EXPORT int ScUpdateModelMatrix(uint32_t layerId, float *modelMatrixArray) {
    return scUpdateModelMatrix(layerId, modelMatrixArray);
}

SVRP_EXPORT int ScSendActionBarCMD(uint32_t layerId, int cmd) {
    return scSendActionBarCMD(layerId, cmd);
}

SVRP_EXPORT int ScInjectMotionEvent(uint32_t layerId, int displayID, int action, float x, float y) {
    return scInjectMotionEvent(layerId, displayID, action, x, y);
}

SVR_EXPORT int ScDestroyLayer() {
    return scDestroyLayer();
}

SVRP_EXPORT int ScGetOfflineMapRelocState() {
    return scGetOfflineMapRelocState();
}

SVR_EXPORT int ScResaveMap(const char *path) {
    scResaveMap(path);
    return 1;
}

SVR_EXPORT bool IsTrackingDataLost() {
    return isTrackingDataLost();
}

SVRP_EXPORT int ScGetPanel() {
    return scGetPanel();
}

SVR_EXPORT int ScGetPanelInfo(float *info) {
    scGetPanelInfo(info);
    return 1;
}

// add by chenweihua 20201026 (start)
SVRP_EXPORT void ScHandShank_SetKeyEventCallback(HANDSHANK_KEY_EVENT_CALLBACK callback) {
    scHandShank_SetKeyEventCallback(callback);
}

SVRP_EXPORT void ScHandShank_SetKeyTouchEventCallback(HANDSHANK_KEY_TOUCH_EVENT_CALLBACK callback) {
    scHandShank_SetKeyTouchEventCallback(callback);
}

SVRP_EXPORT void ScHandShank_SetTouchEventCallback(HANDSHANK_TOUCH_EVENT_CALLBACK callback) {
    scHandShank_SetTouchEventCallback(callback);
}

SVRP_EXPORT void ScHandShank_SetHallEventCallback(HANDSHANK_HALL_EVENT_CALLBACK callback) {
    scHandShank_SetHallEventCallback(callback);
}

SVRP_EXPORT void ScHandShank_SetChargingEventCallback(HANDSHANK_CHARGING_EVENT_CALLBACK callback) {
    scHandShank_SetChargingEventCallback(callback);
}

SVRP_EXPORT void ScHandShank_SetBatteryEventCallback(HANDSHANK_BATTERY_EVENT_CALLBACK callback) {
    scHandShank_SetBatteryEventCallback(callback);
}

SVRP_EXPORT void ScHandShank_SetConnectEventCallback(HANDSHANK_CONNECT_EVENT_CALLBACK callback) {
    scHandShank_SetConnectEventCallback(callback);
}

SVRP_EXPORT int ScHandShank_GetBattery(int lr) {
    return scHandShank_GetBattery(lr);
}

SVRP_EXPORT int ScHandShank_GetVersion() {
    return scHandShank_GetVersion();
}

SVRP_EXPORT int ScHandShank_Getbond() {
    return scHandShank_Getbond();
}

SVRP_EXPORT int ScHandShank_GetConnectState(int lr) {
    return scHandShank_GetConnectState(lr);
}

SVRP_EXPORT void ScHandShank_LedControl(int enable) {
    scHandShank_LedControl(enable);
}

SVRP_EXPORT void ScHandShank_VibrateControl(int value) {
    scHandShank_VibrateControl(value);
}

SVRP_EXPORT int ScFetch3dofHandShank(float *orientationArray, int lr) {
    return scFetch3dofHandShank(orientationArray, lr);
}

SVRP_EXPORT int ScFetch6dofHandShank(float *orientationArray, int lr) {
    return scFetch6dofHandShank(orientationArray, lr);
}
// add by chenweihua 20201026 (end)

//add zengchuiguo 20200724 (start)
SVRP_EXPORT void Sc_setAppExitCallback(APP_EXIT_CALLBACK callback) {
    sc_setAppExitCallback(callback);
}

SVR_EXPORT int ScHANDTRACK_Start(LOW_POWER_WARNING_CALLBACK callback) {
    scHANDTRACK_Start(callback);
    return 1;
}

SVR_EXPORT int ScHANDTRACK_Stop() {
    scHANDTRACK_Stop();
    return 1;
}

SVR_EXPORT int ScHANDTRACK_GetGesture(float *model, float *pose) {
    uint64_t index;
    scHANDTRACK_GetGesture(&index, model, pose);
    return 1;
}

SVR_EXPORT int ScHANDTRACK_GetGestureWithIdx(uint64_t *index, float *model, float *pose) {
    scHANDTRACK_GetGesture(index, model, pose);
    return 1;
}

SVR_EXPORT void ScHANDTRACK_SetGestureData(float *model, float *pose) {
    scHANDTRACK_setGestureData(model, pose);
}

SVRP_EXPORT void ScHANDTRACK_SetGestureCallback(GESTURE_CHANGE_CALLBACK callback) {
    scHANDTRACK_SetGestureCallback(callback);
}

SVRP_EXPORT void ScHANDTRACK_SetGestureModelDataCallback(GESTURE_MODEL_DATA_CHANGE_CALLBACK callback) {
    scHANDTRACK_SetGestureModelDataCallback(callback);
}

SVRP_EXPORT void ScHANDTRACK_SetLowPowerWarningCallback(LOW_POWER_WARNING_CALLBACK callback) {
    scHANDTRACK_SetLowPowerWarningCallback(callback);
}
//add zengchuiguo 20200724 (end)

}
#endif //_SVR_UNITY_H_
