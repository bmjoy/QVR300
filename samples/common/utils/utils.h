//=============================================================================
// FILE: utils.h
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//============================================================================
#pragma once

#include "svrShader.h"
#include "svrUtil.h"
#include "svrKtxLoader.h"


namespace Svr {
    void
    InitializeShader(Svr::SvrShader &whichShader, const char *vertexPath, const char *fragmentPath,
                     const char *vertexName, const char *fragmentName);

    void *GetFileBuffer(const char *pFileName, int *bufferSize);

    bool
    LoadTextureCommon(GLuint *pTexture, const char *pFileName, bool isProtectedTexture = false);

    unsigned int GetTimeMS();

    void L_CreateLayout(float centerX, float centerY, float radiusX, float radiusY, svrLayoutCoords *pLayout);

}