//=============================================================================
// FILE: app.h
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
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

#include "../../../vera/render/AbstractRender.h"
#include "../../../vera/render/PlaneRender.h"
#include "../../../vera/render/CubeRender.h"

using namespace InVision;

class SimpleApp : public Svr::SvrApplication {

public:
    SimpleApp();

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

    CubeRender *mCubeRender;
    PlaneRender *mPlaneRender;
    bool mFindPlane = false;

    /**
     * 0 - ori torus
     * 1 - cube
     */

    float A;
    float B;
    float C;
    float X;
    float Y;
    float Z;

    float anchorX;
    float anchorY;
    float anchorZ;

    int mRenderType = 1;
    void* libHandle;
    int (*ivslam_getplan)(int x, int y, float &A, float &B, float &C, float& pointX, float& pointY, float& pointZ);
    int (*ivslam_getanchor)(float& pointX, float& pointY, float& pointZ);

};