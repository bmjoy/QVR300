//=============================================================================
// FILE: app.cpp
//
//                  Copyright (c) 2017-2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================
//           Foveated Rendering with  Snapdragon VRSDK
//=============================================================================
//This simple app demonstrates  of  a native Snapdragon VR app with feature
// foveated rendering.
//
//Foveated rendering is a technique that aims to reduce fragment processing workload
// and bandwidth by reducing the average resolution of a framebuffer.
//Perceived image quality is kept high by leaving the focal point of rendering at full resolution.
//
//For OpenGLES , there is a extension to enable foveated rendering feature on
// framebuffer  and config focal point and other parameters.
//link: https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_framebuffer_foveated.txt
//
//There are two api for foveated rendering feature:
//
//1.void FramebufferFoveationConfigQCOM(uint fbo,uint numLayers,uint focalPointsPerLayer,
// uint requestedFeatures,uint *providedFeatures);
// This api is used to configure foveated rendering for the framebuffer object 'fbo' and to
// instruct the implementation to allocate any additional intermediate resources needed for
// foveated rendering. In order to enable foveation, this call must be issued prior to any
// operation which causes data to be written to a framebuffer attachment. Once this call is
// made for a framebuffer object, the fbo will remain a "foveated fbo".
//
//2.void FramebufferFoveationParametersQCOM(uint fbo,uint layer,uint focalPoint,float focalX,
// float focalY,float gainX,float gainY,float foveaArea);
// This api is used to control the falloff  of the foveated rendering of 'focalPoint' for layer
// 'layer' in the framebuffer object 'fbo'.Multiple focal points per layer are provided to enable
// foveated rendering when different regions of a framebuffer represent different views, such as
// with double wide rendering. Values of 0 to the framebuffers focalPointsPerLayer-1 are valid for
// the 'focalPoint' input and specify which focal point's data to updatefor the layer.
//    'focalX' and 'focalY' is used to specify the x and y coordinate of the focal point of the
// foveated framebuffer in normalized device coordinates. 'gainX' and 'gainY' are used to control
// how quickly the quality falls off as you get further away from the focal point in each axis.
// The larger these values are the faster the quality degrades. 'foveaArea' is used to control the
// minimum size of the fovea region, the area before the quality starts to fall off. These
// parameters should be modified to match the lens characteristics.
//
//Usage:
//It is easy to use in VRSDK apps, two steps:
//1. In AllocateEyeBuffers, after create the eye buffers , use FramebufferFoveationConfigQCOM to
// enable foveated rendering for these buffers.
//2. In Render, before your render commands, use glFramebufferFoveationParametersQCOM to set the
// parameters such as focal points positions gain area size and so on.
//That's it!

//=============================================================================
//                        Introduction for this app
//=============================================================================
//In this app, we will render sphere model and map a equirectangular texture on
// it, so you can view it 360 degree. All the UV are generated with the sphere model
// from blender. You can just replace the test.ktx with other equirectangular picture
// and view them 360 degree.
// Note! Only ktx file format is supported. You can use some tools such as PVRTexTool
// convert png/jpg files to ktx.
//=============================================================================
#include "app.h"


using namespace Svr;


#define BUFFER_SIZE 512

#define  CONTROLLER_BTN_START  0x00000100
#define  CONTROLLER_UNINIT  -1

#ifndef GL_EXT_framebuffer_foveated

#define GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM           0x8BFB

#define GL_FOVEATION_ENABLE_BIT_QCOM                    0x00000001
#define GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM         0x00000002

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTextureFoveationParametersQCOM(GLuint texure, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
#endif


#define GL_APIENTRYP GL_APIENTRY*

typedef void (GL_APIENTRYP PFNGLTEXTUREFOVEATIONPARAMETERSEXT)(GLuint texture, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);

PFNGLTEXTUREFOVEATIONPARAMETERSEXT	glTextureFoveationParametersQCOM = NULL;

#endif

FoveatedVR::FoveatedVR()
{
    mLastRenderTime = 0;
    mModelTexture = 0;
    mFocalPointX = 0.0f;
    mFocalPointY = 0.0f;
    mFoveationGainX = FOV_HIGH_GAIN_X;
    mFoveationGainY = FOV_HIGH_GAIN_Y;
    mFoveationArea = FOV_HIGH_AREA;
    mFoveationLevel = FOV_LEVEL_HIGH;

    mControllerHandler = 0;
    mStartBtnPressed = false;
}

//Callback function of SvrApplication, it will be called before the VR mode start
// prepare the rendering data here.
void FoveatedVR::Initialize()
{
    SvrApplication::Initialize();
    InitializeModel();
}

void FoveatedVR::AllocateEyeBuffers()
{
	glTextureFoveationParametersQCOM = (PFNGLTEXTUREFOVEATIONPARAMETERSEXT) eglGetProcAddress (
			"glTextureFoveationParametersQCOM");
	if (glTextureFoveationParametersQCOM == 0) {
		LOGE("glTextureFoveationParametersQCOM is not supported!");
	}

    unsigned int fovFeaturesFlag = 0;
    for (int i = 0; i < SVR_NUM_EYE_BUFFERS; i++) 
	{
        mAppContext.eyeBuffers[i].eyeTarget[kLeft].Initialize(mAppContext.targetEyeWidth,
                                                              mAppContext.targetEyeHeight, 1,
                                                              GL_RGBA8, true);
        mAppContext.eyeBuffers[i].eyeTarget[kRight].Initialize(mAppContext.targetEyeWidth,
                                                               mAppContext.targetEyeHeight, 1,
                                                               GL_RGBA8, true);
        if (IsFoveatedSupported()) 
		{
			GL(glBindTexture(GL_TEXTURE_2D, mAppContext.eyeBuffers[i].eyeTarget[kLeft].GetColorAttachment()));
            GL(glTexParameteri(GL_TEXTURE_2D,
                GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM,
                GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM));
				
			GL(glBindTexture(GL_TEXTURE_2D, mAppContext.eyeBuffers[i].eyeTarget[kRight].GetColorAttachment()));
            GL(glTexParameteri(GL_TEXTURE_2D,
                GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM,
                GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM));
				
			GL(glBindTexture(GL_TEXTURE_2D, 0));
        }
    }
}

bool FoveatedVR::IsFoveatedSupported()
{
    return glTextureFoveationParametersQCOM != 0;
}

//Initialize  model for rendering: load the model from .obj file, load the shaders,
// load the texture file, prepare the model matrix and model color
void FoveatedVR::InitializeModel() {
    //load the model from torus.obj file
    SvrGeometry *pObjGeom;
    int nObjGeom;
    char tmpFilePath[BUFFER_SIZE];
    char tmpFilePath2[BUFFER_SIZE];
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "sphere.obj");
    SvrGeometry::CreateFromObjFile(tmpFilePath, &pObjGeom, nObjGeom);
    mModel = &pObjGeom[0];


    //load the vertex shader model_v.glsl and fragment shader model_f.glsl
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, "model_v.glsl");
    sprintf(tmpFilePath2, "%s/%s", mAppContext.externalPath, "model_f.glsl");
    Svr::InitializeShader(mShader, tmpFilePath, tmpFilePath2, "Vs", "Fs");

    //load the texture white.ktx
    const char *pModelTexFile = "test.ktx";
    sprintf(tmpFilePath, "%s/%s", mAppContext.externalPath, pModelTexFile);
    Svr::LoadTextureCommon(&mModelTexture, tmpFilePath);
    //prepare the model matrix and model color
    float colorScale = 0.8f;
    mModelMatrix[0] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

}
//Submit the rendered frame
void FoveatedVR::SubmitFrame()
{
    svrFrameParams frameParams;
    memset(&frameParams, 0, sizeof(frameParams));
    frameParams.frameIndex = mAppContext.frameCount;

    frameParams.renderLayers[kLeft].imageHandle = mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[Svr::kLeft].GetColorAttachment();
    frameParams.renderLayers[kLeft].imageType = kTypeTexture;
     L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[kLeft].imageCoords);
    frameParams.renderLayers[kLeft].eyeMask = kEyeMaskLeft;

    frameParams.renderLayers[kRight].imageHandle = mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[Svr::kRight].GetColorAttachment();
    frameParams.renderLayers[kRight].imageType = kTypeTexture;
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[kRight].imageCoords);
    frameParams.renderLayers[kRight].eyeMask = kEyeMaskRight;

    frameParams.headPoseState = mPoseState;
    frameParams.minVsyncs = 1;
    svrSubmitFrame(&frameParams);
    mAppContext.eyeBufferIndex = (mAppContext.eyeBufferIndex + 1) % SVR_NUM_EYE_BUFFERS;
}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame. Update view matrix and projection matrix here.
void FoveatedVR::Update()
{
    //base class Update
    SvrApplication::Update();

    //update view matrix
    float predDispTime = svrGetPredictedDisplayTime();
    mPoseState = svrGetPredictedHeadPose(predDispTime);

    mPoseState.pose.position.x = 0.0f;
    mPoseState.pose.position.y = 0.0f;
    mPoseState.pose.position.z = 0.0f;
	
	//Using a 0.0 value for an IPD since we are only rendering a mono cube map which looks incorrect if rendered in stereo
    SvrGetEyeViewMatrices(mPoseState, true,
                          0.0f, DEFAULT_HEAD_HEIGHT, DEFAULT_HEAD_DEPTH, mViewMatrix[kLeft],
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

        //LOGI("Button State: 0x%x", controllerState.buttonState);
        if (controllerState.buttonState & CONTROLLER_BTN_START) {
            if (!mStartBtnPressed) {
                mFoveationLevel = mFoveationLevel + 1;
                if (mFoveationLevel == FOV_LEVEL_MAX) {
                    mFoveationLevel = FOV_LEVEL_NONE;
                }
                LOGI("change foveationLevel to %d", mFoveationLevel);
                mStartBtnPressed = true;
            }
        } else {
            mStartBtnPressed = false;
        }
    }

}

void FoveatedVR::ProcessEvents()
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
void FoveatedVR::RenderEye(Svr::SvrEyeId eyeId)
{

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);


    if (IsFoveatedSupported())
    {

        //you can calculate eye position here

        if (mFoveationLevel == FOV_LEVEL_NONE)
        {
            GL(glTextureFoveationParametersQCOM(
                    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].GetColorAttachment(),
                    0, 0, mFocalPointX, mFocalPointY, 0, 0,
                    0));
        }
        else if (mFoveationLevel == FOV_LEVEL_HIGH)
        {

            GL(glTextureFoveationParametersQCOM(
                    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].GetColorAttachment(),
                    0, 0, mFocalPointX, mFocalPointY, FOV_HIGH_GAIN_X, FOV_HIGH_GAIN_Y,
                    FOV_HIGH_AREA));

        }
        else if (mFoveationLevel == FOV_LEVEL_LOW)
        {
            GL(glTextureFoveationParametersQCOM(
                    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].GetColorAttachment(),
                    0, 0, mFocalPointX, mFocalPointY, FOV_LOW_GAIN_X, FOV_LOW_GAIN_Y,
                    FOV_LOW_AREA));
        }
    }

    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].Bind();
    glViewport(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
    glScissor(0, 0, mAppContext.targetEyeWidth, mAppContext.targetEyeHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mShader.Bind();
    mShader.SetUniformMat4("projectionMatrix", mProjectionMatrix[eyeId]);
    mShader.SetUniformMat4("viewMatrix", mViewMatrix[eyeId]);
    mShader.SetUniformSampler("srcTex", mModelTexture, GL_TEXTURE_2D, 0);
    glm::vec3 eyePos = glm::vec3(-mViewMatrix[eyeId][3][0], -mViewMatrix[eyeId][3][1],
                                 -mViewMatrix[eyeId][3][2]);
    mShader.SetUniformVec3("eyePos", eyePos);

    for (int j = 0; j < MODEL_SIZE; j++) {
        mShader.SetUniformMat4("modelMatrix", mModelMatrix[j]);
        mModel->Submit();
    }
    mShader.Unbind();
    mAppContext.eyeBuffers[mAppContext.eyeBufferIndex].eyeTarget[eyeId].Unbind();

}

//Callback function of SvrApplication, after the VR mode start,it will be called in a loop
// once each frame after Update(), render for two eyes and submit the frame.
void FoveatedVR::Render()
{

    RenderEye(kLeft);
    RenderEye(kRight);
    SubmitFrame();
}

//Callback function of SvrApplication, called when VR mode stop
//Clean up the model texture
void FoveatedVR::Shutdown()
{
    if (mModelTexture != 0)
    {
        GL(glDeleteTextures(1, &mModelTexture));
        mModelTexture = 0;
    }
    mModel->Destroy();
    mShader.Destroy();
}

void FoveatedVR::StartController()
{

    mControllerHandler = svrControllerStartTracking("foveated-test");
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
        return new FoveatedVR();
    }
}