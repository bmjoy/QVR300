//=============================================================================
// FILE: app.cpp
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================
//           Use MultiView in Snapdragon VRSDK Native app
//=============================================================================
//This simple app demonstrates  how to use MultiView in  VR app based on the
//Snapdragon VRSDK.
//
//In VR app , the method of stereo rendering is currently achieved by rendering
// to the two eye buffers sequentially.  This typically incurs double the application
// and driver overhead, despite the fact that the command streams and render
// states are almost identical.
//
// The  MultiView extension seeks to address the inefficiency of sequential multiview
// rendering by adding a means to render to multiple elements of a 2D texture
// array simultaneously.  In multiview rendering, draw calls are instanced into
// each corresponding element of the texture array.  The vertex program uses a
// new gl_ViewID_OVR variable to compute per-view values, typically the vertex
// position and view-dependent variables like reflection.
//
//
// Use MultiView in Snapdragon VR, follow below steps:
// 1.Check if the multiview extension is supported, load the multiview functions.
// 2.Create Texture array and bind it with framebuffer.
// 3.Passing  view matrix  array to vertex shader, use gl_ViewID_OVR index to access
//  view matrix, it will calculate twice and render in one draw call.
// 4.In SubmitFrame, eyeBufferType set to kEyeBufferArray and  eyeMask set to
// kEyeMaskBoth.
//
//=============================================================================
//                        Introduction for this app
//=============================================================================
//In this app, we will render six torus in each direction(up,down,left,right,
// backward, forward), the torus model is loaded from torus.obj file. Each torus
// will rotate around the X axis, rotations speed is defined by ROTATION_SPEED
//The distance between the viewer and the torus object is defined by
// MODEL_POSITION_RADIUS.
//=============================================================================
#include "app.h"

using namespace Svr;

// Functions that have to be queried from EGL
typedef void(*PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)(GLenum, GLenum, GLuint, GLint, GLint, GLsizei);

PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR glFramebufferTextureMultiviewOVR = NULL;

typedef void(*PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVR)(GLenum, GLenum, GLuint, GLint,
                                                              GLsizei, GLint, GLsizei);

PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVR glFramebufferTextureMultisampleMultiviewOVR = NULL;

typedef void(*PFNGLTEXSTORAGE3DMULTISAMPLEOES)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei,
                                               GLboolean);

PFNGLTEXSTORAGE3DMULTISAMPLEOES glTexStorage3DMultisampleOES = NULL;

#ifndef GL_TEXTURE_2D_MULTISAMPLE_ARRAY
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY        0x9102
#endif // GL_TEXTURE_2D_MULTISAMPLE_ARRAY

#ifndef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE       0x8D56
#endif // GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE


#define NUM_MULTIVIEW_SLICES    2


#define MODEL_POSITION_RADIUS 4.0f
#define BUFFER_SIZE 512
#define ROTATION_SPEED 5.0f



MultiViewApp::MultiViewApp()
{
    mLastRenderTime   = 0;
    gMsaaSamples      = 4;
    mCurrentEyeBuffer = 0;
}

//Callback function of SvrApplication, it will be called before the VR mode start
// prepare the rendering data here.
void MultiViewApp::Initialize()
{
    InitializeMultiviewFunctions();
    SvrApplication::Initialize();
    InitializeModel();
}

//Allocate eye buffers for multiview
void MultiViewApp::AllocateEyeBuffers()
{
    for (int whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        InitializeMultiviewBuffer(whichBuffer, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight, gMsaaSamples);

    }
}

//Initialize multiview extensions functions
void MultiViewApp::InitializeMultiviewFunctions()
{
    LOGE("InitializeMultiviewFunctions");
    glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
    if (!glFramebufferTextureMultiviewOVR)
    {
        LOGE("Failed to  get proc address for glFramebufferTextureMultiviewOVR!!");
        exit(EXIT_FAILURE);
    }

    glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVR)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR");
    if (!glFramebufferTextureMultisampleMultiviewOVR)
    {
        LOGE("Failed to  get proc address for glFramebufferTextureMultisampleMultiviewOVR!!");
        exit(EXIT_FAILURE);
    }

    glTexStorage3DMultisampleOES = (PFNGLTEXSTORAGE3DMULTISAMPLEOES)eglGetProcAddress("glTexStorage3DMultisampleOES");
    if (!glTexStorage3DMultisampleOES)
    {
        LOGE("Failed to  get proc address for glTexStorage3DMultisampleOES!!");
        exit(EXIT_FAILURE);
    }

}

//Create multiview framebuffer
//whichBuffer: buffer index
//width: width of the buffer
//height: height of the buffer
//samples: MSAA
void MultiViewApp::InitializeMultiviewBuffer(int whichBuffer, int width, int height, int samples)
{
    GLsizei NumLevels = 1;
    GLenum ColorFormat = GL_RGBA8;

    // Create the color attachment array
    GL(glGenTextures(1, &mMultiViewBuffers[whichBuffer].mColorId));
    GL(glBindTexture(GL_TEXTURE_2D_ARRAY, mMultiViewBuffers[whichBuffer].mColorId));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, NumLevels, ColorFormat, width, height, NUM_MULTIVIEW_SLICES));
    GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

    // Create the depth attachment array
    GL(glGenTextures(1, &mMultiViewBuffers[whichBuffer].mDepthId));
    GL(glBindTexture(GL_TEXTURE_2D_ARRAY, mMultiViewBuffers[whichBuffer].mDepthId));
    GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, NumLevels, GL_DEPTH_COMPONENT24, width, height, NUM_MULTIVIEW_SLICES));
    GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

    // Create the frame buffer object
    GL(glGenFramebuffers(1, &mMultiViewBuffers[whichBuffer].mFboId));

    // Attach the color and depth arrays
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mMultiViewBuffers[whichBuffer].mFboId));

    if (samples > 1)
    {
        GL(glFramebufferTextureMultisampleMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mMultiViewBuffers[whichBuffer].mColorId, 0, samples, 0, NUM_MULTIVIEW_SLICES));
        GL(glFramebufferTextureMultisampleMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mMultiViewBuffers[whichBuffer].mDepthId, 0, samples, 0, NUM_MULTIVIEW_SLICES));
    }
    else
    {
        GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mMultiViewBuffers[whichBuffer].mColorId, 0, 0, NUM_MULTIVIEW_SLICES));
        GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mMultiViewBuffers[whichBuffer].mDepthId, 0, 0, NUM_MULTIVIEW_SLICES));
    }


    // Verify the frame buffer was created
    GLenum eResult = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (eResult != GL_FRAMEBUFFER_COMPLETE)
    {
        const char *pPrefix = "MultiView Framebuffer";
        switch (eResult)
        {
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            LOGE("%s => Error (GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) setting up FBO", pPrefix);
            break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            LOGE("%s => Error (GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) setting up FBO", pPrefix);
            break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
            LOGE("%s => Error (GL_FRAMEBUFFER_UNSUPPORTED) setting up FBO", pPrefix);
            break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            LOGE("%s => Error (GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE) setting up FBO", pPrefix);
            break;
            default:
            LOGE("%s => Error (0x%X) setting up FBO", pPrefix, eResult);
            break;
        }
    }

    // No longer need to be bound
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));

}


//Initialize  model for rendering: load the model from .obj file, load the shaders,
// load the texture file, prepare the model matrix and model color
void MultiViewApp::InitializeModel()
{
    //load the model from torus.obj file
    SvrGeometry *pObjGeom;
    int nObjGeom;
    char tmpFilePath[BUFFER_SIZE];
    char tmpFilePath2[BUFFER_SIZE];
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "torus.obj");
    SvrGeometry::CreateFromObjFile(tmpFilePath, &pObjGeom, nObjGeom);
    mModel = &pObjGeom[0];


    //load the vertex shader multiview_v.glsl and fragment shader multiview_f.glsl
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "multiview_v.glsl");
    sprintf(tmpFilePath2, "%s/%s", mAppContext.externalPath, "multiview_f.glsl");
    Svr::InitializeShader(mMultiViewShader, tmpFilePath, tmpFilePath2, "Vs", "Fs");

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
void MultiViewApp::SubmitFrame()
{
    svrFrameParams frameParams;
    memset(&frameParams, 0, sizeof(frameParams));
    frameParams.frameIndex = mAppContext.frameCount;

    frameParams.renderLayers[0].imageType = kTypeTextureArray;
    frameParams.renderLayers[0].imageHandle = mMultiViewBuffers[mCurrentEyeBuffer].mColorId;
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[0].imageCoords);
    frameParams.renderLayers[0].eyeMask = kEyeMaskLeft;

    frameParams.renderLayers[1].imageType = kTypeTextureArray;
    frameParams.renderLayers[1].imageHandle = mMultiViewBuffers[mCurrentEyeBuffer].mColorId;
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[1].imageCoords);
    frameParams.renderLayers[1].eyeMask = kEyeMaskRight;

    frameParams.headPoseState = mPoseState;
    frameParams.minVsyncs = 1;
    svrSubmitFrame(&frameParams);
    mAppContext.eyeBufferIndex = (mAppContext.eyeBufferIndex + 1) % SVR_NUM_EYE_BUFFERS;
    mCurrentEyeBuffer = (mCurrentEyeBuffer + 1) % SVR_NUM_EYE_BUFFERS;
}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame. Update view matrix and projection matrix here.
void MultiViewApp::Update()
{
    //base class Update
    SvrApplication::Update();

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

}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame after Update(), render for two eyes and submit the frame.
void MultiViewApp::Render()
{

    float rotateAmount = GetRotationAmount(ROTATION_SPEED);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    GL( glClearColor(0.2f, 0.2f, 0.2f, 1.0) );
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mMultiViewBuffers[mCurrentEyeBuffer].mFboId));
    GL(glViewport(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight));
    GL(glScissor(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    mMultiViewShader.Bind();
    mMultiViewShader.SetUniformMat4("projMtx", mProjectionMatrix[0]);
    mMultiViewShader.SetUniformMat4fv("viewMtx[0]", 2, glm::value_ptr(mViewMatrix[0]));

    for (int j = 0; j < MODEL_SIZE; j++) {
        mModelMatrix[j] = glm::rotate(mModelMatrix[j], glm::radians(rotateAmount),
                                      glm::vec3(1.0f, 0.0f, 0.0f));
         mMultiViewShader.SetUniformMat4("mdlMtx", mModelMatrix[j]);
         mMultiViewShader.SetUniformVec3("modelColor", mModelColor[j]);
         mModel->Submit();
    }
    mMultiViewShader.Unbind();
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));

    SubmitFrame();
}

//Get rotation amount for rotationRate
// rotationRate: the rotation rate, for example rotationRate = 5 means 5 degree every second
float MultiViewApp::GetRotationAmount(float rotationRate)
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
void MultiViewApp::Shutdown()
{
    for (int whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        GL(glDeleteTextures(1, &mMultiViewBuffers[whichBuffer].mColorId));
        GL(glDeleteTextures(1, &mMultiViewBuffers[whichBuffer].mDepthId));
        GL(glDeleteFramebuffers(1, &mMultiViewBuffers[whichBuffer].mFboId));
    }

    mModel->Destroy();
    mMultiViewShader.Destroy();

}

namespace Svr {
    //Return your own SvrApplication instance
    Svr::SvrApplication *CreateApplication()
    {
        return new MultiViewApp();
    }
}