//=============================================================================
// FILE: utils.cpp
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//============================================================================
//                       Introduction
//============================================================================
//This file provide some util functions for Snapdragon VRSDK native apps, such
// as load the shader from files , load the texture from files, read file and
// store content in a buffer, get the current timestamp and so on.
//=============================================================================
#include <sys/time.h>

#include "utils.h"

namespace Svr {

    //Load the vertex shader and fragment shader from file path to Svr::SvrShader
    //Svr::SvrShader: the shaders will be loaded into this object
    //vertexPath: the vetex shader file path
    //fragmentPath: the fragment shader file path
    //vertexName: the name of vertex shader
    //fragmentName: the name of fragment shader
    void
    InitializeShader(Svr::SvrShader &whichShader, const char *vertexPath, const char *fragmentPath,
                     const char *vertexName, const char *fragmentName)
    {
        int fileBuffSize;

        char *pVsBuffer = (char *) GetFileBuffer(vertexPath, &fileBuffSize);
        if (pVsBuffer == NULL) {
            return;
        }

        char *pFsBuffer = (char *) GetFileBuffer(fragmentPath, &fileBuffSize);
        if (pFsBuffer == NULL) {
            free(pVsBuffer);
            return;
        }

        int numVertStrings = 0;
        int numFragStrings = 0;
        const char *vertShaderStrings[16];
        const char *fragShaderStrings[16];

        vertShaderStrings[numVertStrings++] = pVsBuffer;
        fragShaderStrings[numFragStrings++] = pFsBuffer;
        whichShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings,
                               vertexName, fragmentName);

        free(pVsBuffer);
        free(pFsBuffer);
    }

    //Read content from file path and return pointer of the buffer
    //pFileName: the pointer of the file path string
    //pBufferSize: will write the the length of the content into it
    //return a pointer of the buffer which have the file content
    void *GetFileBuffer(const char *pFileName, int *pBufferSize)
    {

        LOGI("Opening File: %s", pFileName);
        FILE *fp = fopen(pFileName, "rb");
        if (!fp) {
            LOGE("Unable to open file: %s", pFileName);
            return NULL;
        }

        fseek(fp, 0, SEEK_END);
        *pBufferSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char *pRetBuff = new char[*pBufferSize + 1];
        fread(pRetBuff, 1, *pBufferSize, fp);
        pRetBuff[*pBufferSize] = 0;

        fclose(fp);

        return pRetBuff;
    }

    //Load ktx file and bind it to the texture
    //pTexture: the texture ID which will be binded
    //pFileName: the ktx file path
    //isProtectedTexture: whether the texture is protected
    //return true if success, false if fail.
    bool LoadTextureCommon(GLuint *pTexture, const char *pFileName, bool isProtectedTexture)
    {
        GLenum TexTarget;
        KtxTexture TexHelper;

        LOGI("Loading %s...", pFileName);

        int TexBuffSize;
        char *pTexBuffer = (char *) GetFileBuffer(pFileName, &TexBuffSize);
        if (pTexBuffer == NULL) {
            LOGE("Unable to open file: %s", pFileName);
            return false;
        }

        TKTXHeader *pOutHeader = NULL;
        TKTXErrorCode ResultCode = TexHelper.LoadKtxFromBuffer(pTexBuffer, TexBuffSize, pTexture,
                                                               &TexTarget, pOutHeader,
                                                               isProtectedTexture);
        if (ResultCode != KTX_SUCCESS) {
            LOGE("Unable to load texture: %s", pFileName);
        }

        free(pTexBuffer);

        return true;
    }

    //Get current time in milliseconds
    unsigned int GetTimeMS()
    {
        timeval t;
        t.tv_sec = t.tv_usec = 0;

        if (gettimeofday(&t, NULL) == -1) {
            return 0;
        }
        return (unsigned int) (t.tv_sec * 1000LL + t.tv_usec / 1000LL);
    }

    void L_CreateLayout(float centerX, float centerY, float radiusX, float radiusY, svrLayoutCoords *pLayout)
    {
        // This is always in screen space so we want Z = 0 and W = 1
        float lowerLeftPos[4] = { centerX - radiusX, centerY - radiusY, 0.0f, 1.0f };
        float lowerRightPos[4] = { centerX + radiusX, centerY - radiusY, 0.0f, 1.0f };
        float upperLeftPos[4] = { centerX - radiusX, centerY + radiusY, 0.0f, 1.0f };
        float upperRightPos[4] = { centerX + radiusX, centerY + radiusY, 0.0f, 1.0f };

        float lowerUVs[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
        float upperUVs[4] = { 0.0f, 1.0f, 1.0f, 1.0f };

        memcpy(pLayout->LowerLeftPos, lowerLeftPos, sizeof(lowerLeftPos));
        memcpy(pLayout->LowerRightPos, lowerRightPos, sizeof(lowerRightPos));
        memcpy(pLayout->UpperLeftPos, upperLeftPos, sizeof(upperLeftPos));
        memcpy(pLayout->UpperRightPos, upperRightPos, sizeof(upperRightPos));

        memcpy(pLayout->LowerUVs, lowerUVs, sizeof(lowerUVs));
        memcpy(pLayout->UpperUVs, upperUVs, sizeof(upperUVs));

        float identTransform[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                    0.0f, 1.0f, 0.0f, 0.0f,
                                    0.0f, 0.0f, 1.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, 1.0f };
        memcpy(pLayout->TransformMatrix, identTransform, sizeof(pLayout->TransformMatrix));

    }

}