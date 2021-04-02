//=============================================================================
// FILE: app.h
//
//                  Copyright (c) 2017-2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================

#pragma once

#include <math.h>
#include "svrApi.h"
#include "svrApplication.h"
#include "svrGeometry.h"
#include "svrRenderTarget.h"
#include "svrShader.h"
#include "svrUtil.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

#include "utils.h"

class FoveatedVR : public Svr::SvrApplication {

public:
    FoveatedVR();

    void Initialize();

    void LoadConfiguration() {};

    void AllocateEyeBuffers();

    void Update();

    void Render();

    void Shutdown();




private:
    //This app will render six objects at each direction(up,down,left,right,forward,backward).
    enum
    {
        MODEL_SIZE = 1
    };

    enum {
        FOV_LEVEL_NONE = 0,
        FOV_LEVEL_LOW = 1,
        FOV_LEVEL_HIGH = 2,
        FOV_LEVEL_MAX = 3,
    };

    //Use SvrGeometry to load model from an .obj file
    Svr::SvrGeometry *mModel;

    //Use SvrShader to load vertex and fragment shaders from file.
    Svr::SvrShader mShader;

    //Use a texture for coloring
    GLuint mModelTexture;

    //pose state
    svrHeadPoseState mPoseState;

    //view matrix
    glm::mat4 mViewMatrix[kNumEyes];

    //projection matrix
    glm::mat4 mProjectionMatrix[kNumEyes];

    //model matrix for each object
    glm::mat4 mModelMatrix[MODEL_SIZE];

    //color for each object
    glm::vec3 mModelColor[MODEL_SIZE];

    //recored the render time to calculate rotation
    unsigned int mLastRenderTime;

    int mFoveationLevel;

    float mFocalPointX;
    float mFocalPointY;
    float mFoveationGainX;
    float mFoveationGainY;
    float mFoveationArea;
    static constexpr float FOV_LOW_GAIN_X = 4;
    static constexpr float FOV_LOW_GAIN_Y = 4;
    static constexpr float FOV_LOW_AREA = 2;

    static constexpr float FOV_HIGH_GAIN_X = 16;
    static constexpr float FOV_HIGH_GAIN_Y = 16;
    static constexpr float FOV_HIGH_AREA = 2;

    void InitializeModel();

    void RenderEye(Svr::SvrEyeId);

    void SubmitFrame();

    bool IsFoveatedSupported();

    void StartController();

    void StopController();

    void ProcessEvents();

    int mControllerHandler;

    bool mStartBtnPressed;
};