//=============================================================================
// FILE: app.cpp
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================
//           Getting Started with Snapdragon VRSDK Native app
//=============================================================================
//This simple app demonstrates  how to create a native VR app based on the
//Snapdragon VRSDK.
//Native Snapdragon VRSDK app is based on Android NativeActivity, please refer
// https://github.com/googlesamples/android-ndk/tree/master/native-activity if you
// don't konw how to render something with OpenGLES on Android NativeActivity.
//
//Based on the Android NativeActivity, Snapdragon VRSDK  provide svr framework
// layer, which already  help handle the Activity lifecycle and VR modes
// switch, this layer provide a base class SvrApplication, it is easy to inherit
// this class and implement the related callbacks(Initialize,Update,Render) to
// create your own VR apps.
//
//The execution order of the important functions:
// android_main() ==> Svr::CreateApplication() ==> svrInitialize() ==>
// SvrApplication::LoadConfiguration() ==> NativeActivity Resume ==>
// SvrApplication::Initialize() ==> svrBeginVR ==> SvrApplication::Update() ==>
// SvrApplication::Render() ==> NativeActivity Pause ==> svrEndVR ==>
// SvrApplication::Shutdown()
//
//android_main(): this is the native entry point provided by android_native_app_glue
// android_native_app_glue is a native library for NativeActivity
// refer https://developer.android.com/ndk/samples/sample_na.html to know more
// about it
//
//Svr::CreateApplication(): this is the function you should implement to create and
// return your own SvrApplication instance.
//
//svrInitialize(), svrBeginVR(), svrEndVR() are the APIs from svrapi.so library
// these APIs are called in svr framework to handle the VR mode switch, you don't
// have to change them if you have no special requirements.
//
//SvrApplication::LoadConfiguration(): this function is intent to  read
// configurations from a file before the render process, skip it if you don't
// have such things.
//
//SvrApplication::Initialize(): this function is intent to implement some
// initialization before the render process, for example, prepare your model
// (vertex data), your shaders and so on.
//
//SvrApplication::Update(), SvrApplication::Render(): these two functions are
// the main rendering part , they will be called in a loop , once each frame.
// You can update your  data in Update and render you scene in Render(). There
// are some difference between a VR app and normal OpenGLES app, in VR app you
// need get the head pose each frame(refer svrGetPredictedHeadPose and
// SvrGetEyeViewMatrices used in Update) and apply to your view matrix ,
// similarly update your projection matrix as well.Another difference is the
// Render function, in VR app you need to render for each eye(refer the RenderEye)
// Don't foget to submit your frame (refer SubmitFrame) at the end of the Render().
// refer https://learnopengl.com/#!Getting-started/Coordinate-Systems if you are't
// familiar with the model,view and projection in rendering.
//
//SvrApplication::Shutdown(): called when VR stopped, you can clean up
// something here.
//
//Summary:
//To create your first native VRSDK app , follow the below steps:
//1.Create a project like this simple app.
//2.Create your own SvrApplication class, implement Svr::CreateApplication and
// return your SvrApplication instance.
//3.Implement Initialize(), Update(),Render(),Shutdown(), prepare the rendering
// data in Initialize(), update the view matrix, projection matrix as well as
// other changed data in Update(), draw your stuff in the Render() function and
// finally SubmitFrame when finish render. Clean up in the Shutdown().
//=============================================================================
//                        Introduction for this app
//=============================================================================
//In this app, we will render six torus in each direction(up,down,left,right,
// backward, forward), the torus model is loaded from torus.obj file. Each torus
// will rotate around the X axis, rotations speed is defined by ROTATION_SPEED
//The distance between the viewer and the torus object is defined by
// MODEL_POSITION_RADIUS.
//=============================================================================
#include <unistd.h>
#include "app.h"

//#include "ARCORE_API.h"
#include "dlfcn.h"
#include <sys/system_properties.h>

using namespace Svr;

#define MODEL_POSITION_RADIUS 4.0f
#define BUFFER_SIZE 512
#define ROTATION_SPEED 5.0f
//#define INVISION_SLAM_LIB "libuvc_native.so"
bool mUpdateAnchor = false;
bool mUpdatePlan = false;
SimpleApp::SimpleApp() {
    mLastRenderTime = 0;
    mModelTexture = 0;
    mPlaneRender = new PlaneRender();
    mCubeRender = new CubeRender();
//    libHandle = dlopen(INVISION_SLAM_LIB, RTLD_NOW);
//
//    typedef int (*ivslam_getplan_fn)(int x, int y, float &A, float &B, float &C, float &pointX,
//                                     float &pointY, float &pointZ);
//    ivslam_getplan = (ivslam_getplan_fn) dlsym(libHandle, "ARCORE_get_plane");
//
//    typedef int (*ivslam_getanchor_fn)(float &pointX,float &pointY, float &pointZ);
//    ivslam_getanchor = (ivslam_getanchor_fn) dlsym(libHandle, "ARCORE_get_anchor");

}

//Callback function of SvrApplication, it will be called before the VR mode start
// prepare the rendering data here.
void SimpleApp::Initialize() {
    SvrApplication::Initialize();
    InitializeModel();
    mFindPlane = false;
    if (mRenderType > 0) {
        svrDeviceInfo di = svrGetDeviceInfo();
        LOGI("SimpleApp::Initialize di.displayWidthPixels=%d, displayHeightPixels=%d",
             di.displayWidthPixels, di.displayHeightPixels);
        mCubeRender->init(di.displayWidthPixels, di.displayHeightPixels);
        mPlaneRender->init(di.displayWidthPixels, di.displayHeightPixels);
    }
}

//Initialize  model for rendering: load the model from .obj file, load the shaders,
// load the texture file, prepare the model matrix and model color
void SimpleApp::InitializeModel() {
    //load the model from torus.obj file
    SvrGeometry *pObjGeom;
    int nObjGeom;
    char tmpFilePath[BUFFER_SIZE];
    char tmpFilePath2[BUFFER_SIZE];
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "torus.obj");
    SvrGeometry::CreateFromObjFile(tmpFilePath, &pObjGeom, nObjGeom);
    mModel = &pObjGeom[0];


    //load the vertex shader model_v.glsl and fragment shader model_f.glsl
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "model_v.glsl");
    sprintf(tmpFilePath2, "%s/%s", mAppContext.externalPath, "model_f.glsl");
    Svr::InitializeShader(mShader, tmpFilePath, tmpFilePath2, "Vs", "Fs");

    //load the texture white.ktx
    const char *pModelTexFile = "white.ktx";
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, pModelTexFile);
    Svr::LoadTextureCommon(&mModelTexture, tmpFilePath);

    //prepare the model matrix and model color
    float colorScale = 0.8f;
    mModelMatrix[0] = glm::translate(glm::mat4(1.0f), glm::vec3(MODEL_POSITION_RADIUS, 0.0f, 0.0f));
    mModelColor[0] = colorScale * glm::vec3(1.0f, 0.0f, 0.0f);

    mModelMatrix[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, MODEL_POSITION_RADIUS, 0.0f));
    mModelColor[1] = colorScale * glm::vec3(0.0f, 1.0f, 0.0f);

    mModelMatrix[2] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, MODEL_POSITION_RADIUS));
    mModelColor[2] = colorScale * glm::vec3(0.0f, 0.0f, 1.0f);

    mModelMatrix[3] = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(-MODEL_POSITION_RADIUS, 0.0f, 0.0f));
    mModelColor[3] = colorScale * glm::vec3(0.0f, 1.0f, 1.0f);

    mModelMatrix[4] = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(0.0f, -MODEL_POSITION_RADIUS, 0.0f));
    mModelColor[4] = colorScale * glm::vec3(1.0f, 0.0f, 1.0f);

    mModelMatrix[5] = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(0.0f, 0.0f, -MODEL_POSITION_RADIUS));
    mModelColor[5] = colorScale * glm::vec3(1.0f, 1.0f, 0.0f);
}

//Submit the rendered frame
void SimpleApp::SubmitFrame() {
    svrFrameParams frameParams;
    memset(&frameParams, 0, sizeof(frameParams));
    frameParams.frameIndex = mAppContext.frameCount;

    frameParams.renderLayers[0].imageType = kTypeTexture;
    frameParams.renderLayers[0].imageHandle = mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[Svr::kLeft].GetColorAttachment();
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[0].imageCoords);
    frameParams.renderLayers[0].eyeMask = kEyeMaskLeft;

    frameParams.renderLayers[1].imageType = kTypeTexture;
    frameParams.renderLayers[1].imageHandle = mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[Svr::kRight].GetColorAttachment();
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[1].imageCoords);
    frameParams.renderLayers[1].eyeMask = kEyeMaskRight;

    frameParams.headPoseState = mPoseState;
    frameParams.minVsyncs = 1;
    svrSubmitFrame(&frameParams);
    mAppContext.eyeBufferIndex = (mAppContext.eyeBufferIndex + 1) % SVR_NUM_EYE_BUFFERS;
}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame. Update view matrix and projection matrix here.
void SimpleApp::Update() {
    //base class Update
    SvrApplication::Update();

//if(mUpdateAnchor) {
//    LOGI("@@@ mReceiveBoardCast = true ");
//    if (mFindPlane) {
//        int anchorRes = ivslam_getanchor(anchorX, anchorY, anchorZ);
//        if (anchorRes == 0) {
//            mCubeRender->updateCubePosition(anchorY, anchorZ, anchorX);
//        }
//    }
//    mUpdateAnchor = false;
//}
//if(mUpdatePlan){
//        int res = ivslam_getplan(320, 240, A, B, C, X, Y, Z);
//        LOGI("@@@ res = %d,A,B,C,X,Y,Z = %f,%f,%f,%f,%f,%f", res, A, B, C, X, Y, Z);
//        if (res == 0) {
//            mPlaneRender->setPlanInfo(B, C, A, Y, Z, X);
//            mFindPlane = true;
//            mCubeRender->setFindPlan(true);
//        }
//    mUpdatePlan = false;
//}




    //update view matrix
    float predDispTime = svrGetPredictedDisplayTime();
    mPoseState = svrGetPredictedHeadPose(predDispTime);
    SvrGetEyeViewMatrices(mPoseState, true,
                          DEFAULT_IPD, DEFAULT_HEAD_HEIGHT, DEFAULT_HEAD_DEPTH, mViewMatrix[kLeft],
                          mViewMatrix[kRight]);

    //update projection matrix
    svrDeviceInfo di = svrGetDeviceInfo();
    svrViewFrustum *pLeftFrust = &di.leftEyeFrustum;
    mProjectionMatrix[kLeft] = glm::frustum(pLeftFrust->left, pLeftFrust->right, pLeftFrust->bottom,
                                            pLeftFrust->top, pLeftFrust->near, pLeftFrust->far);

    svrViewFrustum *pRightFrust = &di.rightEyeFrustum;
    mProjectionMatrix[kRight] = glm::frustum(pRightFrust->left, pRightFrust->right,
                                             pRightFrust->bottom, pRightFrust->top,
                                             pRightFrust->near, pRightFrust->far);

    if (mRenderType > 0) {
        mCubeRender->updateData();
        mPlaneRender->updateData();
    }
}

//Render content for one eye
//SvrEyeId: the id of the eye, kLeft for the left eye, kRight for the right eye.
void SimpleApp::RenderEye(Svr::SvrEyeId eyeId) {

    float rotateAmount = GetRotationAmount(ROTATION_SPEED);
    rotateAmount = 0;

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].Bind();
    glViewport(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
    glScissor(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
//    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    svrBeginEye((svrWhichEye) eyeId);
    mShader.Bind();
    mShader.SetUniformMat4("projectionMatrix", mProjectionMatrix[eyeId]);
    mShader.SetUniformMat4("viewMatrix", mViewMatrix[eyeId]);
    mShader.SetUniformSampler("srcTex", mModelTexture, GL_TEXTURE_2D, 0);
    glm::vec3 eyePos = glm::vec3(-mViewMatrix[eyeId][3][0], -mViewMatrix[eyeId][3][1],
                                 -mViewMatrix[eyeId][3][2]);
    mShader.SetUniformVec3("eyePos", eyePos);

    for (int j = 0; j < MODEL_SIZE; j++) {
        mModelMatrix[j] = glm::rotate(mModelMatrix[j], glm::radians(rotateAmount),
                                      glm::vec3(1.0f, 0.0f, 0.0f));
        mShader.SetUniformMat4("modelMatrix", mModelMatrix[j]);
        mShader.SetUniformVec3("modelColor", mModelColor[j]);
        mModel->Submit();
    }
    mShader.Unbind();

    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].Unbind();
    svrEndEye((svrWhichEye) eyeId);

}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame after Update(), render for two eyes and submit the frame.
void SimpleApp::Render() {
    if (0 == mRenderType) {
        RenderEye(kLeft);
        RenderEye(kRight);
    } else if (mRenderType > 0) {
        mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[kLeft].Bind();
        glViewport(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
        glScissor(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);

        mCubeRender->render(glm::value_ptr(mProjectionMatrix[kLeft]),
                            glm::value_ptr(mViewMatrix[kLeft]));
        if (mFindPlane) {
        mPlaneRender->render(glm::value_ptr(mProjectionMatrix[kLeft]),
                             glm::value_ptr(mViewMatrix[kLeft]));

        }
        mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[kLeft].Unbind();

        mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[kRight].Bind();
        glViewport(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
        glScissor(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);

        mCubeRender->render(glm::value_ptr(mProjectionMatrix[kRight]),
                            glm::value_ptr(mViewMatrix[kRight]));
        if (mFindPlane) {
        mPlaneRender->render(glm::value_ptr(mProjectionMatrix[kRight]),
                             glm::value_ptr(mViewMatrix[kRight]));
        }
        mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[kRight].Unbind();
    }
    SubmitFrame();
//    usleep(1e6);
}

//Get rotation amount for rotationRate
// rotationRate: the rotation rate, for example rotationRate = 5 means 5 degree every second
float SimpleApp::GetRotationAmount(float rotationRate) {
    unsigned int timeNow = GetTimeMS();
    if (mLastRenderTime == 0) {
        mLastRenderTime;
    }

    float elapsedTime = (float) (timeNow - mLastRenderTime) / 1000.0f;
    float rotateAmount = elapsedTime * (360.0f / rotationRate);

    mLastRenderTime = timeNow;
    return rotateAmount;
}

//Callback function of SvrApplication, called when VR mode stop
//Clean up the model texture
void SimpleApp::Shutdown() {
    if (mModelTexture != 0) {
        GL(glDeleteTextures(1, &mModelTexture));
        mModelTexture = 0;
    }
    if (mRenderType > 0) {
        mCubeRender->release();
        mPlaneRender->release();
    }
}

namespace Svr {
    //Return your own SvrApplication instance
    Svr::SvrApplication *CreateApplication() {
        return new SimpleApp();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_qualcomm_svr_simple_SvrNativeActivity_NativeUpdateAnchor(
        JNIEnv* env,
        jclass /* this */) {
    LOGI("@@@ Java_com_qualcomm_svr_simple_SvrNativeActivity_BtnClick");
    mUpdateAnchor = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_qualcomm_svr_simple_SvrNativeActivity_NativeUpdatePlan(
        JNIEnv* env,
        jclass /* this */) {
    LOGI("@@@ Java_com_qualcomm_svr_simple_SvrNativeActivity_BtnClick");
    mUpdatePlan = true;
}