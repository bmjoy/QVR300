//=============================================================================
// FILE: app.cpp
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================
//                         Simple Layer VR 
//=============================================================================
//This simple app demonstrates  how to create another layer in  VR app, you can 
// render some different things such as UI  with new layers. 

//=============================================================================
//                        Introduction for this app
//=============================================================================
//In this app, we will render six torus in each direction(up,down,left,right,
// backward, forward), the torus model is loaded from torus.obj file. Each torus
// will rotate around the X axis, rotations speed is defined by ROTATION_SPEED
//The distance between the viewer and the torus object is defined by
// MODEL_POSITION_RADIUS.
//Besides the default layer(torus), it created  another layer to render some text
// on top of the torus layer.
//=============================================================================
#include "app.h"

using namespace Svr;

#define MODEL_POSITION_RADIUS 4.0f
#define BUFFER_SIZE 512
#define ROTATION_SPEED 5.0f

SimpleLayerApp::SimpleLayerApp()
{
    mLastRenderTime = 0;
    mModelTexture = 0;
    mOverlayTexture = 0;
}

//Callback function of SvrApplication, it will be called before the VR mode start
// prepare the rendering data here.
void SimpleLayerApp::Initialize()
{
    SvrApplication::Initialize();
    InitializeModel();
}

//Initialize  model for rendering: load the model from .obj file, load the shaders,
// load the texture file, prepare the model matrix and model color
void SimpleLayerApp::InitializeModel()
{
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

    //load overlay texture 
    const char *pOverylayTexFile = "overlay_text_center.ktx";
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, pOverylayTexFile);
    Svr::LoadTextureCommon(&mOverlayTexture, tmpFilePath);


    //prepare the model matrix and model color
    float colorScale = 0.8f;
    mModelMatrix[0] = glm::translate(glm::mat4(1.0f), glm::vec3(MODEL_POSITION_RADIUS, 0.0f, 0.0f));
    mModelColor[0]  = colorScale * glm::vec3(1.0f, 0.0f, 0.0f);

    mModelMatrix[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, MODEL_POSITION_RADIUS, 0.0f));
    mModelColor[1]  = colorScale * glm::vec3(0.0f, 1.0f, 0.0f);

    mModelMatrix[2] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, MODEL_POSITION_RADIUS));
    mModelColor[2]  = colorScale * glm::vec3(0.0f, 0.0f, 1.0f);

    mModelMatrix[3] = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(-MODEL_POSITION_RADIUS, 0.0f, 0.0f));
    mModelColor[3]  = colorScale * glm::vec3(0.0f, 1.0f, 1.0f);

    mModelMatrix[4] = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(0.0f, -MODEL_POSITION_RADIUS, 0.0f));
    mModelColor[4]  = colorScale * glm::vec3(1.0f, 0.0f, 1.0f);

    mModelMatrix[5] = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(0.0f, 0.0f, -MODEL_POSITION_RADIUS));
    mModelColor[5]  = colorScale * glm::vec3(1.0f, 1.0f, 0.0f);
}

//Submit the rendered frame
void SimpleLayerApp::SubmitFrame()
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

    //overlay layer , render to both eyes
    frameParams.renderLayers[2].imageHandle = mOverlayTexture;
    frameParams.renderLayers[2].imageType = kTypeTexture;
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[2].imageCoords);

    frameParams.renderLayers[2].eyeMask = kEyeMaskBoth;
    frameParams.renderLayers[2].layerFlags |= kLayerFlagHeadLocked;

    frameParams.headPoseState = mPoseState;
    frameParams.minVsyncs = 1;
    svrSubmitFrame(&frameParams);
    mAppContext.eyeBufferIndex = (mAppContext.eyeBufferIndex + 1) % SVR_NUM_EYE_BUFFERS;
}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame. Update view matrix and projection matrix here.
void SimpleLayerApp::Update()
{
    //base class Update
    SvrApplication::Update();

    //update view matrix
    float predictedTimeMs = svrGetPredictedDisplayTime();
    mPoseState = svrGetPredictedHeadPose(predictedTimeMs);

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

}

//Render content for one eye
//SvrEyeId: the id of the eye, kLeft for the left eye, kRight for the right eye.
void SimpleLayerApp::RenderEye(Svr::SvrEyeId eyeId)
{

    float rotateAmount = GetRotationAmount(ROTATION_SPEED);

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
void SimpleLayerApp::Render()
{
    RenderEye(kLeft);
    RenderEye(kRight);
    SubmitFrame();
}

//Get rotation amount for rotationRate
// rotationRate: the rotation rate, for example rotationRate = 5 means 5 degree every second
float SimpleLayerApp::GetRotationAmount(float rotationRate)
{
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
void SimpleLayerApp::Shutdown()
{
    if (mModelTexture != 0)
    {
        GL(glDeleteTextures(1, &mModelTexture));
        mModelTexture = 0;
    }
    if (mOverlayTexture != 0)
    {
        GL(glDeleteTextures(1, &mOverlayTexture));
        mOverlayTexture = 0;
    }
    mShader.Destroy();
    mModel->Destroy();
}

namespace Svr {
    //Return your own SvrApplication instance
    Svr::SvrApplication *CreateApplication()
    {
        return new SimpleLayerApp();
    }
}