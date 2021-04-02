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

class AdvancedLayerApp : public Svr::SvrApplication {

public:
    AdvancedLayerApp();

    void Initialize();

    void LoadConfiguration() {};

    void Update();

    void Render();

    void Shutdown();

    void InitializeModel();

    void RenderEye(Svr::SvrEyeId);

    void SubmitFrame();

    float GetRotationAmount(float);


private:
    //This app will render six objects at each direction(up,down,left,right,forward,backward).
    enum
    {
        MODEL_SIZE = 6
    };

    //Use SvrGeometry to load model from an .obj file
    Svr::SvrGeometry *mModel;

    //Use SvrShader to load vertex and fragment shaders from file.
    Svr::SvrShader mShader;

    //Use a texture for coloring
    GLuint mModelTexture;
    //overlay texture 
    GLuint mOverlayTexture;

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
    //radian for overlay layer position 
    float mRadian;
    //overlay layer layout radius
    float gOverlayRadius;
    //overlay layer offset
    float gOverlayOffset;

    static const float RADIAN_ONE = M_PI/180.0f;

};