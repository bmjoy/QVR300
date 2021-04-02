//=============================================================================
// FILE: app.cpp
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//

//=============================================================================
//                        Introduction for this app
//=============================================================================
//In this app, we will render a controller model, and print controller data
//in logcat. 
//=============================================================================
#include "app.h"

using namespace Svr;

#define BUFFER_SIZE 512
#define ROTATION_SPEED 5.0f
#define  CONTROLLER_UNINIT  -1

ControllerApp::ControllerApp()
{
    mModelTexture = 0;
    mControllerHandler = 0;
    mControllerConnected = false;
}

//Callback function of SvrApplication, it will be called before the VR mode start
// prepare the rendering data here.
void ControllerApp::Initialize()
{
    SvrApplication::Initialize();
    InitializeModel();
}

//Initialize  model for rendering: load the model from .obj file, load the shaders,
// load the texture file, prepare the model matrix and model color
void ControllerApp::InitializeModel()
{
    //load the model from torus.obj file
    SvrGeometry *pObjGeom;
    int nObjGeom;
    char tmpFilePath[BUFFER_SIZE];
    char tmpFilePath2[BUFFER_SIZE];
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "controller.obj");
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
    mModelMatrix[0] = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
    //mModelMatrix[0] = glm::scale(mModelMatrix[0], glm::vec3(2.0f,2.0f,2.0f));
    //black
    mModelColor[0]  = colorScale * glm::vec3(0.0f, 0.0f, 0.0f);
}

//Submit the rendered frame
void ControllerApp::SubmitFrame()
{
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

//Log out Controller State 
void ControllerApp::LogControllerState(svrControllerState &tempState)
{
    LOGI("Controller Status:");

    LOGI("    Rotation: [%0.2f, %0.2f, %0.2f, %0.2f]", tempState.rotation.x, tempState.rotation.y, tempState.rotation.z, tempState.rotation.w);
    LOGI("    Position: [%0.2f, %0.2f, %0.2f]", tempState.position.x, tempState.position.y, tempState.position.z);
    LOGI("    Gyroscope: [%0.2f, %0.2f, %0.2f]", tempState.gyroscope.x, tempState.gyroscope.y, tempState.gyroscope.z);
    LOGI("    Accelerometer: [%0.2f, %0.2f, %0.2f]", tempState.accelerometer.x, tempState.accelerometer.y, tempState.accelerometer.z);
    LOGI("    Timestamp: %llu", (long long unsigned int)tempState.timestamp);
    LOGI("    Button State: 0x%08x", tempState.buttonState);
    LOGI("    Analog[0]: [%0.2f, %0.2f]", tempState.analog2D[0].x, tempState.analog2D[0].y);
    LOGI("    Analog[1]: [%0.2f, %0.2f]", tempState.analog2D[1].x, tempState.analog2D[1].y);
    LOGI("    Analog[2]: [%0.2f, %0.2f]", tempState.analog2D[2].x, tempState.analog2D[2].y);
    LOGI("    Analog[3]: [%0.2f, %0.2f]", tempState.analog2D[3].x, tempState.analog2D[3].y);
    LOGI("    Triggers: [%0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f]", tempState.analog1D[0], tempState.analog1D[1], tempState.analog1D[2], tempState.analog1D[3], tempState.analog1D[4], tempState.analog1D[5], tempState.analog1D[6], tempState.analog1D[7]);
    LOGI("    isTouching: 0x%x", tempState.isTouching);
}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame. Update view matrix and projection matrix here.
void ControllerApp::Update()
{
    //base class Update
    SvrApplication::Update();

    //update view matrix
    float predDispTime = svrGetPredictedDisplayTime();
    mPoseState = svrGetPredictedHeadPose(predDispTime);
    mPoseState.pose.position.x = 0.0f;
    mPoseState.pose.position.y = 0.0f;
    mPoseState.pose.position.z = 0.0f;
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

    ProcessEvents();

    if (mControllerHandler != CONTROLLER_UNINIT) {
        svrControllerState controllerState = svrControllerGetState(mControllerHandler);
        if (controllerState.connectionState != kConnected)
        {
            mControllerConnected = false;
            LOGE("controller is not connected!!!");
        } 
        else 
        {
            //LogControllerState(controllerState);
            mControllerConnected = true;
            glm::fquat poseQuat = glm::fquat(-controllerState.rotation.w, controllerState.rotation.x, controllerState.rotation.y, -controllerState.rotation.z);
            mControllerOrientation = glm::mat4_cast(poseQuat);
        }

    }
}



void ControllerApp::ProcessEvents()
{
    svrEvent evt;
    while (svrPollEvent(&evt))
    {
        if (evt.eventType == kEventVrModeStarted)
        {
            StartController();
        }
    }

}
//Render content for one eye
//SvrEyeId: the id of the eye, kLeft for the left eye, kRight for the right eye.
void ControllerApp::RenderEye(Svr::SvrEyeId eyeId)
{

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].Bind();
    glViewport(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
    glScissor(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    svrBeginEye((svrWhichEye) eyeId);
    mShader.Bind();
    mShader.SetUniformMat4("projectionMatrix", mProjectionMatrix[eyeId]);
    mShader.SetUniformMat4("viewMatrix", mViewMatrix[eyeId]);
    mShader.SetUniformSampler("srcTex", mModelTexture, GL_TEXTURE_2D, 0);
    glm::vec3 eyePos = glm::vec3(-mViewMatrix[eyeId][3][0], -mViewMatrix[eyeId][3][1],
                                 -mViewMatrix[eyeId][3][2]);
    mShader.SetUniformVec3("eyePos", eyePos);

    if (mControllerConnected)
    {

        glm::mat4 modelMatrix= mModelMatrix[0]*mControllerOrientation;
        modelMatrix = mViewMatrix[0]*modelMatrix;
        mShader.SetUniformMat4("modelMatrix", modelMatrix);
    } 
    else 
    {
        mShader.SetUniformMat4("modelMatrix", mModelMatrix[0]);
    }
   
    mShader.SetUniformVec3("modelColor", mModelColor[0]);
    mModel->Submit();

    mShader.Unbind();

    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].Unbind();
    svrEndEye((svrWhichEye) eyeId);

}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame after Update(), render for two eyes and submit the frame.
void ControllerApp::Render()
{
    RenderEye(kLeft);
    RenderEye(kRight);
    SubmitFrame();
}

//Callback function of SvrApplication, called when VR mode stop
//Clean up the model texture
void ControllerApp::Shutdown()
{
    if (mModelTexture != 0)
    {
        GL(glDeleteTextures(1, &mModelTexture));
        mModelTexture = 0;
    }
    mShader.Destroy();
    mModel->Destroy();
}

void ControllerApp::StartController()
{

    mControllerHandler = svrControllerStartTracking("controller-test");
    if (mControllerHandler == CONTROLLER_UNINIT) {
        LOGE("controller start fail..");
    }
    else
    {
        LOGI("controller start success..");
    }
    svrControllerState state = svrControllerGetState(mControllerHandler);

    if (state.connectionState != kConnected)
    {
        LOGE("controller is not connected!");
    }
}

namespace Svr {
    //Return your own SvrApplication instance
    Svr::SvrApplication *CreateApplication()
    {
        return new ControllerApp();
    }
}