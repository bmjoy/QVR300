//=============================================================================
// FILE: app.h
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
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

class ControllerApp : public Svr::SvrApplication {

public:
    ControllerApp();

    void Initialize();

    void LoadConfiguration() {};

    void Update();

    void Render();

    void Shutdown();

    void InitializeModel();

    void RenderEye(Svr::SvrEyeId);

    void SubmitFrame();

    void StartController();

    void StopController();

    void ProcessEvents();

    void LogControllerState(svrControllerState &tempState);


private:
    //This app will render six objects at each direction(up,down,left,right,forward,backward).
    enum
    {
        MODEL_SIZE = 1
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
    //controller handler 
    int mControllerHandler;
    //controller orientation
    glm::mat4  mControllerOrientation;
    //controller connection state
    bool mControllerConnected;
};