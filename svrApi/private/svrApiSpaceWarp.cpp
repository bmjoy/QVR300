//=============================================================================
// FILE: svrApiSpaceWarp.cpp
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <android/native_window.h>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

#include "svrCpuTimer.h"
#include "svrGpuTimer.h"
#include "svrGeometry.h"
#include "svrProfile.h"
#include "svrShader.h"
#include "svrUtil.h"
#include "svrConfig.h"
#include "svrRenderExt.h"

#include "private/svrApiCore.h"
#include "private/svrApiEvent.h"
#include "private/svrApiHelper.h"
#include "private/svrApiTimeWarp.h"

using namespace Svr;

#ifdef ENABLE_MOTION_VECTORS
VAR(bool, gEnableMotionVectors, false, kVariableNonpersistent);         //Enables motion vector support
VAR(bool, gForceAppEnableMotionVectors, false, kVariableNonpersistent); //Force motion vectors for all applications. Otherwise up to application
VAR(bool, gEnableYuvDecode, true, kVariableNonpersistent);              //For Power: Whether or not to actually send data to motion engine
VAR(bool, gUseMotionVectors, true, kVariableNonpersistent);             //For Power: Whether or not to use motion data
VAR(bool, gWarpMotionVectors, true, kVariableNonpersistent);            //Warp current motion engine input to remove head motion
VAR(float, gWarpMotionScale, 1.0f, kVariableNonpersistent);             //Scale of motion vector warping
VAR(bool, gLogMotionVectors, false, kVariableNonpersistent);            //Enables logging of motion vector activities
VAR(bool, gLogThreadState, false, kVariableNonpersistent);              //Subject to "gLogMotionVectors": Logs thread activities
VAR(float, gMotionVectorScale, 0.5f, kVariableNonpersistent);           //Scale of eye buffer size to motion vector input
VAR(float, gMotionInterpolationFactor, 0.5f, kVariableNonpersistent);   //Scale of motion vector data applied to interpolated frames (Assume half framerate)
VAR(bool, gSmoothMotionVectors, true, kVariableNonpersistent);          //Motion data is average of surrounding samples
VAR(bool, gSmoothMotionVectorsWithGPU, true, kVariableNonpersistent);   //Motion data smoothing is done on the GPU
VAR(bool, gRenderMotionVectors, false, kVariableNonpersistent);         //Enables display of motion vectors as one of the eye buffers
VAR(bool, gRenderMotionInput, false, kVariableNonpersistent);           //Enables display of motion input. Current/Previous are the Left/Right eyes (for checking warp). Subject to gRenderMotionVectors
VAR(bool, gGenerateBothEyes, false, kVariableNonpersistent);            //Generate separate motion data for each eye. Otherwise, same data is used for both

#endif // ENABLE_MOTION_VECTORS

EXTERN_VAR(bool, gUseQvrPerfModule);

// Warp data used by the TimeWarpThread
extern SvrAsyncWarpResources gWarpData;

// These shaders live in svrApiTimeWarpShaders.cpp
extern const char svrBlitQuadVs[];
extern const char svrBlitQuadWarpVs[];
extern const char svrBlitQuadFs[];
extern const char svrBlitQuadYuvFs[];
extern const char svrBlitQuadFs_Image[];
extern const char svrBlitQuadYuvFs_Array[];
extern const char svrBlitQuadYuvFs_Image[];

#ifdef ENABLE_MOTION_VECTORS
SvrMotionData *gpMotionData = NULL;

uint32_t gMostRecentMotionLeft = 0;
uint32_t gMostRecentMotionRight = 0;

// If we are using spacewarp, but not on a frame, we need a stub result texture
GLuint gStubResultTexture = 0;

#endif // ENABLE_MOTION_VECTORS


// Common local function that lives in svrApiCore.cpp
void L_SetThreadPriority(const char *pName, int policy, int priority);

#ifdef ENABLE_MOTION_VECTORS
// Need to be able to calculate warp matrix. Function is in svrApiTimeWarp.cpp
glm::mat4 CalculateWarpMatrix(glm::fquat& origPose, glm::fquat& latestPose);
glm::mat4 CalculateProjectionMatrix();
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
enum eWHICH_SEM
{
    eSEM_AVAILABLE,
    eSEM_DO_WORK,
    eSEM_DONE,
};
bool gfLogSem = false;
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
bool L_ReleaseSem(uint32_t whichMotionData, eWHICH_SEM eWhich)
//-----------------------------------------------------------------------------
{
    int iReturn = 0;

    switch (eWhich)
    {
    case eSEM_AVAILABLE:
        if (gfLogSem) LOGI("Releasing semaphore: Available (%d)", whichMotionData);
        iReturn = sem_post(&gpMotionData[whichMotionData].semAvailable);
        break;

    case eSEM_DO_WORK:
        if (gfLogSem) LOGI("Releasing semaphore: DoWork (%d)", whichMotionData);
        iReturn = sem_post(&gpMotionData[whichMotionData].semDoWork);
        break;

    case eSEM_DONE:
        if (gfLogSem) LOGI("Releasing semaphore: Done (%d)", whichMotionData);
        iReturn = sem_post(&gpMotionData[whichMotionData].semDone);
        break;
    }

    if (iReturn < 0)
    {
        int iError = errno;
        switch (iError)
        {
        case EINVAL:    // The process or thread would have blocked, and the abs_timeout parameter specified a nanoseconds field value less than zero or greater than or equal to 1000 million. 
            LOGE("ERROR releasing semaphore: EINVAL");
            break;

        case ETIMEDOUT: // The semaphore could not be locked before the specified timeout expired. 
            LOGE("ERROR releasing semaphore: ETIMEDOUT");
            break;

        case EDEADLK:   // A deadlock condition was detected. 
            LOGE("ERROR releasing semaphore: EDEADLK");
            break;

        case EINTR:     // A signal interrupted this function. 
            LOGE("ERROR releasing semaphore: EINTR");
            break;

            //case EINVAL:    // The sem argument does not refer to a valid semaphore. 
            //    LOGE("ERROR releasing semaphore: EINVAL");
            //    break;

        default:
            LOGE("ERROR releasing semaphore: %d", iError);
            break;
        }

        return false;
    }

    return true;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
bool L_GetSem(uint32_t whichMotionData, eWHICH_SEM eWhich, uint64_t nanoSeconds)
//-----------------------------------------------------------------------------
{
    int iReturn = -1;
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
    {
        LOGE("ERROR reading system time.");
        return false;
    }

    // Normalize the wait time so it doesn't wrap
    ts.tv_nsec += nanoSeconds;
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    switch (eWhich)
    {
    case eSEM_AVAILABLE:
        if (gfLogSem) LOGI("Requesting semaphore: Available (%d)", whichMotionData);
        iReturn = sem_timedwait(&gpMotionData[whichMotionData].semAvailable, &ts);
        // iReturn = sem_wait(&gpMotionData[whichMotionData].semAvailable);
        break;

    case eSEM_DO_WORK:
        if (gfLogSem) LOGI("Requesting semaphore: DoWork (%d)", whichMotionData);
        iReturn = sem_timedwait(&gpMotionData[whichMotionData].semDoWork, &ts);
        // iReturn = sem_wait(&gpMotionData[whichMotionData].semDoWork);
        break;

    case eSEM_DONE:
        if (gfLogSem) LOGI("Requesting semaphore: Done (%d)", whichMotionData);
        iReturn = sem_timedwait(&gpMotionData[whichMotionData].semDone, &ts);
        // iReturn = sem_wait(&gpMotionData[whichMotionData].semDone);
        break;
    }

    if (iReturn < 0)
    {
        int iError = errno;
        switch (iError)
        {
        case EINVAL:    // The process or thread would have blocked, and the abs_timeout parameter specified a nanoseconds field value less than zero or greater than or equal to 1000 million. 
            LOGE("ERROR waiting for semaphore: EINVAL");
            break;

        case ETIMEDOUT: // The semaphore could not be locked before the specified timeout expired. 
                        // Only an error if actually waiting, otherwise we are just checking and move to next.
            // if (nanoSeconds > 0)
            //     LOGE("ERROR waiting for semaphore: ETIMEDOUT");
            break;

        case EDEADLK:   // A deadlock condition was detected. 
            LOGE("ERROR waiting for semaphore: EDEADLK");
            break;

        case EINTR:     // A signal interrupted this function. 
            LOGE("ERROR waiting for semaphore: EINTR");
            break;

            //case EINVAL:    // The sem argument does not refer to a valid semaphore. 
            //    LOGE("ERROR waiting for semaphore: EINVAL");
            //    break;

        default:
            LOGE("ERROR waiting for semaphore: %d", iError);
            break;
        }

        return false;
    }

    return true;
}
#endif //ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
void L_SetMotionVectorInput(SvrMotionData *pMotionData, svrFrameParamsInternal* pWarpFrame, svrMotionInput *pCurrentInput, svrMotionInputType inputType, glm::mat3 warpMatrix, glm::vec2 offset)
//-----------------------------------------------------------------------------
{
    if (gLogMotionVectors)
        LOGI("Setting motion vector input...");

    // Blending must be off for YUV output targets
    GL(glDisable(GL_BLEND));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, pCurrentInput->imageFrameBuffer));   // GL_DRAW_FRAMEBUFFER
                                                                            // GLenum nDrawBuffer[] = { GL_COLOR_ATTACHMENT0 };
                                                                            // GL(glDrawBuffers(1, nDrawBuffer));

    GL(glViewport(0, 0, pMotionData->imageWidth, pMotionData->imageHeight));
    GL(glScissor(0, 0, pMotionData->imageWidth, pMotionData->imageHeight));

    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // Doesn't currently have a depth buffer

                                                            // 4 points.  Position needs 4 floats per point.  UV needs 2 floats per point
    float quadPos[16];
    float quadUVs[8];

    unsigned int quadIndices[6] = { 0, 2, 1, 1, 2, 3 };
    int indexCount = sizeof(quadIndices) / sizeof(unsigned int);

    glm::vec4 posScaleoffset = glm::vec4(1.0f, 1.0f, offset.x, offset.y);
    // LOGI("DEBUG!    L_SetMotionVectorInput: (%0.4f, %0.4f)", posScaleoffset.z, posScaleoffset.w);

    // Lower Left
    quadPos[0] = -1.0f;
    quadPos[1] = -1.0f;
    quadPos[2] = 0.0f;
    quadPos[3] = 1.0f;
    quadUVs[0] = 0.0f;
    quadUVs[1] = 0.0f;

    // Upper Left
    quadPos[4] = -1.0f;
    quadPos[5] = 1.0f;
    quadPos[6] = 0.0f;
    quadPos[7] = 1.0f;
    quadUVs[2] = 0.0f;
    quadUVs[3] = 1.0f;

    // Lower Right
    quadPos[8] = 1.0f;
    quadPos[9] = -1.0f;
    quadPos[10] = 0.0f;
    quadPos[11] = 1.0f;
    quadUVs[4] = 1.0f;
    quadUVs[5] = 0.0f;

    // Upper Right
    quadPos[12] = 1.0f;
    quadPos[13] = 1.0f;
    quadPos[14] = 0.0f;
    quadPos[15] = 1.0f;
    quadUVs[6] = 1.0f;
    quadUVs[7] = 1.0f;

    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_DEPTH_TEST));

    GL(glBindVertexArray(0));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    // Based on the the eye mask, determine which eye buffer is being used as motion input
    uint32_t whichLayer = 0;
    if (pMotionData->eyeMask == kEyeMaskBoth || pMotionData->eyeMask == kEyeMaskLeft)
    {
        // TODO: Assume first layer is left and second is right
        whichLayer = 0;
    }
    else if(pMotionData->eyeMask == kEyeMaskRight)
    {
        // TODO: Assume first layer is left and second is right
        whichLayer = 1;
    }

    // The UVs passed in to blit need to match the UVs passed in for this layer
    {
        // Lower Left
        quadUVs[0] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[0];
        quadUVs[1] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[1];

        // Upper Left
        quadUVs[2] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[0];
        quadUVs[3] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[1];

        // Lower Right
        quadUVs[4] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[2];
        quadUVs[5] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.LowerUVs[3];

        // Upper Right
        quadUVs[6] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[2];
        quadUVs[7] = pWarpFrame->frameParams.renderLayers[whichLayer].imageCoords.UpperUVs[3];
    }

    if (gLogMotionVectors)
    {
        LOGI("    UVs: LL = [%0.2f, %0.2f]; UL = [%0.2f, %0.2f]; LR = [%0.2f, %0.2f]; UR = [%0.2f, %0.2f]", quadUVs[0], quadUVs[1],
                                                                                                            quadUVs[2], quadUVs[3],
                                                                                                            quadUVs[4], quadUVs[5],
                                                                                                            quadUVs[6], quadUVs[7]);
    }


    // What shader are we using?
    Svr::SvrShader *pCurrentShader = NULL;
    if (inputType == kMotionTypeWarped && gWarpMotionVectors)
    {
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
        {
            if (gLogMotionVectors)
            {
                LOGI("    Blit using YUV Warp (Image) shader");
            }
            pCurrentShader = &pMotionData->blitYuvWarpImageShader;
        }
        else
        {
            if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeTextureArray)
            {
                if (gLogMotionVectors)
                {
                    LOGI("    Blit using YUV Warp Array shader");
                }
                pCurrentShader = &pMotionData->blitYuvWarpArrayShader;
            }
            else
            {
                if (gLogMotionVectors)
                {
                    LOGI("    Blit using YUV Warp shader");
                }
                pCurrentShader = &pMotionData->blitYuvWarpShader;
            }
        }
    }
    else
    {
        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
        {
            if (gLogMotionVectors)
            {
                LOGI("    Blit using YUV (Image) shader");
            }
            pCurrentShader = &pMotionData->blitYuvImageShader;
        }
        else
        {
            if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeTextureArray)
            {
                if (gLogMotionVectors)
                {
                    LOGI("    Blit using YUV Array shader");
                }
                pCurrentShader = &pMotionData->blitYuvArrayShader;
            }
            else
            {
                if (gLogMotionVectors)
                {
                    LOGI("    Blit using YUV shader");
                }
                pCurrentShader = &pMotionData->blitYuvShader;
            }

        }
    }

    // Is it possible that the current shader is ever NULL?
    if (pCurrentShader == NULL)
    {
        LOGE("Unable to find a shader to generate input to Motion Engine!");
        return;
    }

    // Bind the blit shader...
    // LOGI("Thread 0x%08x: Binding blitYuvXXXXShader (Handle = %d)", pMotionData->threadId, pCurrentShader->GetShaderId());
    pCurrentShader->Bind();

    GL(glDisableVertexAttribArray(kPosition));
    GL(glDisableVertexAttribArray(kTexcoord0));

    // ... set attributes ...
    GL(glEnableVertexAttribArray(kPosition));
    GL(glVertexAttribPointer(kPosition, 4, GL_FLOAT, 0, 0, quadPos));

    GL(glEnableVertexAttribArray(kTexcoord0));
    GL(glVertexAttribPointer(kTexcoord0, 2, GL_FLOAT, 0, 0, quadUVs));

    // ... set uniforms ...
    if (gLogMotionVectors)
    {
        LOGI("    posScaleoffset = [%0.2f, %0.2f, %0.2f, %0.2f]", posScaleoffset.x, posScaleoffset.y, posScaleoffset.z, posScaleoffset.w);
    }
    pCurrentShader->SetUniformVec4(1, posScaleoffset);

    if (inputType == kMotionTypeWarped && gWarpMotionVectors)
    {
        if (gLogMotionVectors)
        {
            LOGI("    Setting warp matrix...");
        }
        pCurrentShader->SetUniformMat3(2, warpMatrix);
    }

    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeTextureArray)
    {
        glm::vec4 arrayLayer = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        if (pMotionData->eyeMask & kEyeMaskLeft)
            arrayLayer = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        else if (pMotionData->eyeMask & kEyeMaskRight)
            arrayLayer = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        if (gLogMotionVectors)
        {
            LOGI("    Setting array texture index = %0.2f...", arrayLayer.x);
        }
        pCurrentShader->SetUniformVec4(9, arrayLayer);
    }

    // ... set textures ...
    if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeImage)
    {
        if (gLogMotionVectors)
        {
            LOGI("    Setting uniform texture (Image) handle: srcTex = %d", pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle);
        }
        pCurrentShader->SetUniformSampler("srcTex", pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle, GL_TEXTURE_EXTERNAL_OES, 0);
    }
    else
    {
        int imageHandle = 0;

        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeVulkan)
        {
            imageHandle = GetVulkanInteropHandle(&pWarpFrame->frameParams.renderLayers[whichLayer]);
        }
        else
        {
            imageHandle = pWarpFrame->frameParams.renderLayers[whichLayer].imageHandle;
        }

        if (pWarpFrame->frameParams.renderLayers[whichLayer].imageType == kTypeTextureArray)
        {
            if (gLogMotionVectors)
            {
                LOGI("    Setting uniform texture array handle: srcTex = %d", imageHandle);
            }
            pCurrentShader->SetUniformSampler("srcTex", imageHandle, GL_TEXTURE_2D_ARRAY, 0);
        }
        else
        {
            if (gLogMotionVectors)
            {
                LOGI("    Setting uniform texture handle: srcTex = %d", imageHandle);
            }
            pCurrentShader->SetUniformSampler("srcTex", imageHandle, GL_TEXTURE_2D, 0);
        }
    }

    // ... render the quad ...
    if (gLogMotionVectors)
    {
        LOGI("    Rendering motion input quad...");
    }
    GL(glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, quadIndices));

    // Need a glFinish() to make sure the blit is done before sending to motion engine
    GL(glFinish());

    // ... and disable attributes
    GL(glDisableVertexAttribArray(kPosition));
    GL(glDisableVertexAttribArray(kTexcoord0));

    // Unbind the blit shader...
    // LOGI("Thread 0x%08x: Unbinding blitYuvXXXXShader (Handle = %d)", pMotionData->threadId, pCurrentShader->GetShaderId());
    pCurrentShader->Unbind();

    // Unbind the frame buffer
    GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));   // GL_DRAW_FRAMEBUFFER
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
void ProcessMotionData(uint32_t whichMotionData)
//-----------------------------------------------------------------------------
{
    if (gpMotionData == NULL)
    {
        LOGE("Unable to proccess motion data: Functions have not been initialized!");
        return;
    }

    if (!gpMotionData[whichMotionData].dataIsNew)
    {
        if(gEnableYuvDecode)
            LOGE("Unable to proccess motion data: Data is NOT new!");
        return;
    }

    SvrCpuTimer timer;

    if (gLogMotionVectors)
        LOGI("Processing motion vector buffer %d", whichMotionData);

    PROFILE_ENTER(GROUP_TIMEWARP, 0, "Processing Motion Vector Buffer %d", whichMotionData);

    ME_MotionVector *pWhichData = gpMotionData[whichMotionData].motionVectors;
    if (gSmoothMotionVectors && !gSmoothMotionVectorsWithGPU)
    {
        // Need to smooth the motion vector data then use this value in the texture instead.
        // Should we take half the value since when we interpolate it is halfway between 
        // two frames?
        ME_MotionVector tempSum;
        float sumCount = 5.0f;
        float smoothScale = gMotionInterpolationFactor / sumCount;

        timer.Start();

        // Not doing edges for optimization
        for (uint32_t row = 1; row < gpMotionData[whichMotionData].meshHeight - 1; row++)
        {
            for (uint32_t col = 1; col < gpMotionData[whichMotionData].meshWidth - 1; col++)
            {
                // Where are the neighbors
                // uint32_t left = col > 0 ? col - 1 : 0;
                // uint32_t right = col < gpMotionData[whichMotionData].meshWidth - 2 ? col + 1 : gpMotionData[whichMotionData].meshWidth - 1;
                // 
                // uint32_t top = row > 0 ? row - 1 : 0;
                // uint32_t bottom = row < gpMotionData[whichMotionData].meshHeight - 2 ? row + 1 : gpMotionData[whichMotionData].meshHeight - 1;

                // Now sum up the values for center and all neighbors
                // uint32 whichEntry = 0;
                // tempSum.xMagnitude = 0.0f;
                // tempSum.yMagnitude = 0.0f;
                // sumCount = 0;

                // #ifdef TEST_METHOD
                // Sum
                uint32 c = row * gpMotionData[whichMotionData].meshWidth + col;
                uint32 l = c - 1;
                uint32 r = c + 1;
                uint32 t = c - gpMotionData[whichMotionData].meshWidth;
                uint32 b = c + gpMotionData[whichMotionData].meshWidth;
                tempSum.xMagnitude =    gpMotionData[whichMotionData].motionVectors[c].xMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[l].xMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[r].xMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[t].xMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[b].xMagnitude;
                tempSum.yMagnitude =    gpMotionData[whichMotionData].motionVectors[c].yMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[l].yMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[r].yMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[t].yMagnitude +
                                        gpMotionData[whichMotionData].motionVectors[b].yMagnitude;

                // Scale
                tempSum.xMagnitude *= smoothScale;
                tempSum.yMagnitude *= smoothScale;

                // Average (sumCount now in smoothScale)
                // tempSum.xMagnitude /= sumCount;
                // tempSum.yMagnitude /= sumCount;

                // Write out final value
                gpMotionData[whichMotionData].smoothedVectors[c].xMagnitude = tempSum.xMagnitude;
                gpMotionData[whichMotionData].smoothedVectors[c].yMagnitude = tempSum.yMagnitude;
                // #endif // TEST_METHOD


                // This is no longer a portrait/landscape problem with the latest library.
                // But the values that come back are inverted for some reason (Input order wrong?).
                // Tested both landscape and reverse landscape need the same correction
                // (gAppContext->deviceInfo.displayOrientation == 90)   <= Landscape Left
                // (gAppContext->deviceInfo.displayOrientation == 270)  <= Landscape Right
                // These values are so the correct value is on screen in debug mode.  Correct adjustment
                // is made in the shader.
                gpMotionData[whichMotionData].smoothedVectors[c].xMagnitude = -tempSum.xMagnitude;
                gpMotionData[whichMotionData].smoothedVectors[c].yMagnitude = -tempSum.yMagnitude;

#ifdef WORKING_METHOD
                // TODO: Is motion vector data row major or column major?
                // Center
                whichEntry = row * gpMotionData[whichMotionData].meshHeight + col;
                tempSum.xMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].xMagnitude;
                tempSum.yMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].yMagnitude;
                sumCount++;

                // Left
                whichEntry = row * gpMotionData[whichMotionData].meshHeight + left;
                tempSum.xMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].xMagnitude;
                tempSum.yMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].yMagnitude;
                sumCount++;

                // Right
                whichEntry = row * gpMotionData[whichMotionData].meshHeight + right;
                tempSum.xMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].xMagnitude;
                tempSum.yMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].yMagnitude;
                sumCount++;

                // Top
                whichEntry = top * gpMotionData[whichMotionData].meshHeight + col;
                tempSum.xMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].xMagnitude;
                tempSum.yMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].yMagnitude;
                sumCount++;

                // Bottom
                whichEntry = bottom * gpMotionData[whichMotionData].meshHeight + col;
                tempSum.xMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].xMagnitude;
                tempSum.yMagnitude += gpMotionData[whichMotionData].motionVectors[whichEntry].yMagnitude;
                sumCount++;

                // Scale
                tempSum.xMagnitude *= smoothScale;
                tempSum.yMagnitude *= smoothScale;

                // Average
                tempSum.xMagnitude /= sumCount;
                tempSum.yMagnitude /= sumCount;

                // Write out final value
                gpMotionData[whichMotionData].smoothedVectors[whichEntry].xMagnitude = tempSum.xMagnitude;
                gpMotionData[whichMotionData].smoothedVectors[whichEntry].yMagnitude = tempSum.yMagnitude;
#endif // WORKING_METHOD
            }
        }
        double elapsedTime = timer.Stop();

        if (gLogMotionVectors)
            LOGI("Motion Vector (Set = %d; %dx%d) Smoothing took %0.4f ms", whichMotionData, gpMotionData[whichMotionData].meshWidth, gpMotionData[whichMotionData].meshHeight, elapsedTime);

        // Make sure this new smoothed data is what gets sent to the texture
        pWhichData = gpMotionData[whichMotionData].smoothedVectors;
    }
    else
    {
        timer.Start();

        // Not smoothed, but still need the interpolation factor
        for (uint32_t row = 0; row < gpMotionData[whichMotionData].meshHeight; row++)
        {
            for (uint32_t col = 0; col < gpMotionData[whichMotionData].meshWidth; col++)
            {
                uint32 c = row * gpMotionData[whichMotionData].meshWidth + col;

                gpMotionData[whichMotionData].smoothedVectors[c].xMagnitude = gMotionInterpolationFactor * gpMotionData[whichMotionData].motionVectors[c].xMagnitude;
                gpMotionData[whichMotionData].smoothedVectors[c].yMagnitude = gMotionInterpolationFactor * gpMotionData[whichMotionData].motionVectors[c].yMagnitude;
            }
        }
        double elapsedTime = timer.Stop();

        if (gLogMotionVectors)
            LOGI("Motion Vector (Set = %d; %dx%d) Interpolation factor took %0.4f ms", whichMotionData, gpMotionData[whichMotionData].meshWidth, gpMotionData[whichMotionData].meshHeight, elapsedTime);

        // Make sure this new smoothed data is what gets sent to the texture
        pWhichData = gpMotionData[whichMotionData].smoothedVectors;
    }

    // End the timing here instead of after updating data to graphics
    PROFILE_EXIT(GROUP_TIMEWARP);   // "Processing Motion Vector Buffer %d"

    PROFILE_ENTER(GROUP_TIMEWARP, 0, "Updating texture from Motion Vector Buffer %d", whichMotionData);

    // LOGI("MotionThread %d: Updating result texture %d", whichMotionData + 1, gpMotionData[whichMotionData].resultTexture);
    GL(glBindTexture(GL_TEXTURE_2D, gpMotionData[whichMotionData].resultTexture));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, gpMotionData[whichMotionData].meshWidth, gpMotionData[whichMotionData].meshHeight, 0, GL_RG, GL_FLOAT, pWhichData));
    GL(glBindTexture(GL_TEXTURE_2D, 0));

    // Must do a glFinish or there is no guarantee the GPU work to update the texture has completed.
    // A fence/flush CANNOT be used here as it always comes back as signaled and the texture is not updated.
    GL(glFinish());

    PROFILE_EXIT(GROUP_TIMEWARP);   // "Updating texture from Motion Vector Buffer %d"


    // This data has been processed
    gpMotionData[whichMotionData].dataIsNew = false;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
void VectorCallback(bool vectorsPresent, ME_MotionVector *outputMemory)
//-----------------------------------------------------------------------------
{
    // This should NOT even be possible!
    if (gpMotionData == NULL)
    {
        LOGE("Received Motion Vector callback but Motion Data has not been initialized!");
        return;
    }

    // Which data set is this
    int dataIndx = -1;
    for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
    {
        if (gpMotionData[whichMotionData].motionVectors == outputMemory)
            dataIndx = (int)whichMotionData;
    }

    if (dataIndx == -1)
    {
        LOGE("Received Motion Vector callback but Motion Data is unknown!");
        return;
    }

    if (!gpMotionData[dataIndx].waitingForCallback)
    {
        LOGE("Received Motion Vector callback but Motion Data was not waiting to be filled!");
        return;
    }

    PROFILE_ENTER(GROUP_TIMEWARP, 0, "Motion Vector Callback for Buffer %d", dataIndx);
    PROFILE_EXIT(GROUP_TIMEWARP);   // "Motion Vector Callback for Buffer %d"

    if (!vectorsPresent)
    {
        LOGE("Received Motion Vector callback but Motion Data is absent!");
        memset(gpMotionData[dataIndx].motionVectors, 0, gpMotionData[dataIndx].motionVectorSize);
    }

    // The buffer is not longer waiting to be filled
    gpMotionData[dataIndx].waitingForCallback = false;
    gpMotionData[dataIndx].dataIsNew = true;
    double elapsedTime = gpMotionData[dataIndx].timer.Stop();

    if (gLogMotionVectors)
        LOGI("Recieved motion vector callback for buffer %d (%0.4f ms)", dataIndx, elapsedTime);

    // Let them know we have the data back
    L_ReleaseSem(dataIndx, eSEM_DONE);

    return;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
bool L_CreateMotionVectorImage(SvrMotionData *pMotionData, svrMotionInput *pCurrentInput)
{
    // LOGI("**************************************************");
    // LOGI("Creating %dx%d image buffer...", pMotionData->imageWidth, pMotionData->imageHeight);
    // LOGI("**************************************************");

    // Create the image object
    EGLint eglAttribs[] =
    {
        EGL_WIDTH, pMotionData->imageWidth,
        EGL_HEIGHT, pMotionData->imageHeight,
        EGL_IMAGE_FORMAT_QCOM, EGL_FORMAT_NV12_QCOM,
        EGL_NONE
    };
    //EGL_IMAGE_FORMAT_QCOM, EGL_FORMAT_NV12_QCOM,
    //EGL_IMAGE_FORMAT_QCOM, EGL_FORMAT_RGBA_8888_QCOM,

    // Can't get access to gralloc so have to let the driver create a new one for us.
    // The flag true/false says to wait for a buffer to be ready. Internally this function
    // only supports a maximum of 4 buffers at one time.  So if NUM_MOTION_VECTOR_BUFFERS is
    // greater than 4 this will hang!  For now, leave this as "true" so the app will hang and 
    // last thing in the log will be trying to create this buffer.
    // Latest: The draw call to this buffer fails if the format is YUV. YUV is specified by either EGL_NATIVE_BUFFER_ANDROID or EGL_IMAGE_FORMAT_QCOM = EGL_FORMAT_NV12_QCOM
    if (gLogMotionVectors && gLogThreadState) LOGI("    Calling ME_GetInputBuffer()...");
    pCurrentInput->inputBuffer = ME_GetInputBuffer(pMotionData->hInstance);
    if (gLogMotionVectors && gLogThreadState) LOGI("    ME_GetInputBuffer(): Allocated InputBuffer = %p", pCurrentInput->inputBuffer);

    pCurrentInput->eglImage = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, pCurrentInput->inputBuffer, eglAttribs);
    if (pCurrentInput->eglImage == 0)
    {
        LOGE("Motion Engine: Unable to create image buffer!");
        return false;
    }

    // Create the GL texture object that goes with the image
    GL(glGenTextures(1, &pCurrentInput->imageTexture));

    GL(glBindTexture(GL_TEXTURE_2D, pCurrentInput->imageTexture));    // GL_TEXTURE_EXTERNAL_OES
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // Have the EGL image provide backing for the GL texture
    GL(glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, pCurrentInput->eglImage));   // GL_TEXTURE_EXTERNAL_OES
    // GL(glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0));

    // Create the GL framebuffer object and attach the texture to it
    GL(glGenFramebuffers(1, &pCurrentInput->imageFrameBuffer));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, pCurrentInput->imageFrameBuffer));
    GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pCurrentInput->imageTexture, 0));    // GL_DRAW_FRAMEBUFFER,  GL_TEXTURE_EXTERNAL_OES
    GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    // LOGI("    Image buffer created");
    return true;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
void L_DestroyMotionVectorImage(SvrMotionData *pMotionData, svrMotionInput *pCurrentInput)
{
    // LOGI("Destroying Image buffer. Handle = %d", pMotionData->imageTexture);
    if (pCurrentInput->imageTexture != 0)
    {
        GL(glDeleteTextures(1, &pCurrentInput->imageTexture));
    }
    pCurrentInput->imageTexture = 0;

    if (pCurrentInput->imageFrameBuffer != 0)
    {
        GL(glDeleteFramebuffers(1, &pCurrentInput->imageFrameBuffer));
    }
    pCurrentInput->imageFrameBuffer = 0;

    if (pCurrentInput->eglImage != 0)
        eglDestroyImageKHR(eglGetCurrentDisplay(), pCurrentInput->eglImage);
    pCurrentInput->eglImage = 0;

    // Not sure what we need here, but just to be safe.
    if (pCurrentInput->inputBuffer != 0)
    {
        if (gLogMotionVectors && gLogThreadState) LOGI("    ME_GetInputBuffer(): Releasing InputBuffer = %p", pCurrentInput->inputBuffer);
        pCurrentInput->inputBuffer = 0;
    }
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
bool L_CreateMotionVectorResult(SvrMotionData *pMotionData)
{

    LOGI("**************************************************");
    LOGI("Creating %dx%d motion result texture...", pMotionData->meshWidth, pMotionData->meshHeight);
    LOGI("**************************************************");

    GL(glGenTextures(1, &pMotionData->resultTexture));
    GL(glBindTexture(GL_TEXTURE_2D, pMotionData->resultTexture));

    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // This is RG only float data
    int numChannels = 2;    // RG will be XY motion vectors
    int numItems = pMotionData->meshWidth * pMotionData->meshHeight * numChannels;
    float *pData = (float *)new float[pMotionData->meshWidth * pMotionData->meshHeight * numChannels];
    if (pData == NULL)
    {
        LOGE("Unable to allocate %d bytes for texture memory!", numItems * (int)sizeof(float));
        return false;
    }
    memset(pData, 0, numItems * sizeof(float));

    for (GLuint uiRowIndx = 0; uiRowIndx < pMotionData->meshWidth; uiRowIndx++)
    {
        for (GLuint uiColIndx = 0; uiColIndx < pMotionData->meshHeight; uiColIndx++)
        {
            int iIndx = (uiColIndx * pMotionData->meshWidth + uiRowIndx) * numChannels;

            // Default value that can show up if rendering.  Should be updated by actual motion vectors if everything is working.
            pData[iIndx + 0] = 0.0f;
            pData[iIndx + 1] = 0.0f;
        }
    }

    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, pMotionData->meshWidth, pMotionData->meshHeight, 0, GL_RG, GL_FLOAT, pData));

    // No longer need the data since passed off to OpenGL
    delete pData;

    GL(glBindTexture(GL_TEXTURE_2D, 0));

    LOGI("    Result texture (%d) created", pMotionData->resultTexture);

    return true;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
bool L_CreateStubMotionVectorResult()
{
    uint32_t stubWidth = 4;
    uint32_t stubHeight = 4;

    LOGI("**************************************************");
    LOGI("Creating %dx%d stub motion result texture...", stubWidth, stubHeight);
    LOGI("**************************************************");

    GL(glGenTextures(1, &gStubResultTexture));
    GL(glBindTexture(GL_TEXTURE_2D, gStubResultTexture));

    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // This is RG only float data
    int numChannels = 2;    // RG will be XY motion vectors
    int numItems = stubWidth * stubHeight * numChannels;
    float *pData = (float *)new float[stubWidth * stubHeight * numChannels];
    if (pData == NULL)
    {
        LOGE("Unable to allocate %d bytes for texture memory!", numItems * (int)sizeof(float));
        return false;
    }
    memset(pData, 0, numItems * sizeof(float));

    for (GLuint uiRowIndx = 0; uiRowIndx < stubWidth; uiRowIndx++)
    {
        for (GLuint uiColIndx = 0; uiColIndx < stubHeight; uiColIndx++)
        {
            int iIndx = (uiColIndx * stubWidth + uiRowIndx) * numChannels;

            // For testing, can set this to some offset to prove it is using the stub
            pData[iIndx + 0] = 0.0f;
            pData[iIndx + 1] = 0.0f;
        }
    }

    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, stubWidth, stubHeight, 0, GL_RG, GL_FLOAT, pData));

    // No longer need the data since passed off to OpenGL
    delete pData;

    GL(glBindTexture(GL_TEXTURE_2D, 0));

    LOGI("    Result texture created");

    return true;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
void L_DestroyMotionVectorResult(SvrMotionData *pMotionData)
{
    LOGI("Destroying Motion Vector Result. Handle = %d", pMotionData->resultTexture);
    if (pMotionData->resultTexture != 0)
    {
        GL(glDeleteTextures(1, &pMotionData->resultTexture));
    }
    pMotionData->resultTexture = 0;

}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
void L_DestroyStubMotionVectorResult()
{
    LOGI("Destroying Stub Motion Vector Result. Handle = %d", gStubResultTexture);
    if (gStubResultTexture != 0)
    {
        GL(glDeleteTextures(1, &gStubResultTexture));
    }
    gStubResultTexture = 0;
}
#endif // ENABLE_MOTION_VECTORS


#ifdef ENABLE_MOTION_VECTORS
void TerminateMotionVectors()
{
    // Start/Stop logic tells us we may or may not be in an initialized state
    if (gpMotionData == NULL)
        return;

    LOGI("****************************************");
    LOGI("Terminating Motion Vectors...");
    LOGI("****************************************");
    // Need to wait for any outstanding callbacks
    bool outstandingCallbacks = true;
    uint64_t startTimeNano = Svr::GetTimeNano();
    uint64_t timeNowNano = Svr::GetTimeNano();
    while (outstandingCallbacks)
    {
        outstandingCallbacks = false;
        for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
        {
            if (gpMotionData[whichMotionData].waitingForCallback)
                outstandingCallbacks = true;
        }

        // If one is outstanding, wait for it
        if (outstandingCallbacks)
        {
            LOGI("Waiting for outstanding motion vector callbacks...");
            NanoSleep(MILLISECONDS_TO_NANOSECONDS * 100);
        }

        // If too much time has elapsed then just give up
        timeNowNano = Svr::GetTimeNano();
        if (timeNowNano - startTimeNano > MILLISECONDS_TO_NANOSECONDS * 1000)
        {
            LOGE("Motion vector callbacks did not come in. No longer waiting for them!");
            outstandingCallbacks = false;
        }
    }

    // Stop the motion vector threads
    for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
    {
        if (gpMotionData[whichMotionData].hThread != 0)
        {
            LOGE("TerminateMotionVectors: Stopping thread (%d of %d)...", whichMotionData + 1, NUM_MOTION_VECTOR_BUFFERS);
            gpMotionData[whichMotionData].threadShouldExit = true;
        }
    }

    // Wait for threads to actually finish. If too much time has elapsed then just give up
    uint64_t maxTimeNano = MILLISECONDS_TO_NANOSECONDS * 1000;
    startTimeNano = Svr::GetTimeNano();
    timeNowNano = Svr::GetTimeNano();
    bool runningMotionThreads = true;
    while (runningMotionThreads && timeNowNano - startTimeNano < maxTimeNano)
    {
        runningMotionThreads = false;
        for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
        {
            if(!gpMotionData[whichMotionData].threadClosed)
                runningMotionThreads = true;
        }

        if (runningMotionThreads)
        {
            LOGI("Waiting for motion vector threads to stop...");
            NanoSleep(MILLISECONDS_TO_NANOSECONDS * 250);
        }

        timeNowNano = Svr::GetTimeNano();
    }

    if (timeNowNano - startTimeNano > maxTimeNano)
    {
        LOGE("Motion vector threads did not exit!");
    }

    for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
    {
        if (gpMotionData[whichMotionData].waitingForCallback)
            LOGE("WARNING! Releasing motion vector structure while callback outstanding!");

        gpMotionData[whichMotionData].meshWidth = 0;
        gpMotionData[whichMotionData].meshHeight = 0;

        gpMotionData[whichMotionData].motionVectorSize = 0;
        if (gpMotionData[whichMotionData].motionVectors != NULL)
        {
            free(gpMotionData[whichMotionData].motionVectors);
            gpMotionData[whichMotionData].motionVectors = NULL;
        }

        if (gpMotionData[whichMotionData].smoothedVectors != NULL)
        {
            free(gpMotionData[whichMotionData].smoothedVectors);
            gpMotionData[whichMotionData].smoothedVectors = NULL;
        }


        gpMotionData[whichMotionData].waitingForCallback = false;

        // Close the semaphores
        sem_destroy(&gpMotionData[whichMotionData].semAvailable);
        sem_destroy(&gpMotionData[whichMotionData].semDoWork);
        sem_destroy(&gpMotionData[whichMotionData].semDone);

        // Move this to the actual thread
        // L_DestroyMotionVectorImage(&gpMotionData[whichMotionData]);

        // L_DestroyMotionVectorResult(&gpMotionData[whichMotionData]);
    }

    L_DestroyStubMotionVectorResult();

    // Release the list of structures
    LOGI("Releasing motion vector memory...");
    delete[] gpMotionData;
    gpMotionData = NULL;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
bool L_CreateMotionContext(uint32_t whichMotionData)
//-----------------------------------------------------------------------------
{
    LOGI("Creating motion vector graphics context for MotionThread %d", whichMotionData + 1);

    bool fLogEgl = false;

    EGLDisplay  display;
    EGLSurface  surface;
    EGLContext  context;
    EGLConfig   config = 0;

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY)
    {
        LOGE("MotionVector: Unable to get current display!");
        return false;
    }

    // Initialize EGL 
    EGLint major, minor;
    eglInitialize(display, &major, &minor);
    LOGI("MotionVector: MotionThread %d has EGL version %d.%d", whichMotionData + 1, major, minor);

    // Need to create a small PBuffer surface to attach the context to
    EGLConfig configs[512];
    EGLint numConfigs = 0;

    int currentConfigId = -1;
    EGLConfig pbufferConfig = 0;

    // Get the current configuration id...
    if (EGL_FALSE == eglQueryContext(display, gAppContext->modeContext->warpRenderContext, EGL_CONFIG_ID, &currentConfigId))
    {
        EGLint error = eglGetError();

        char *pError = NULL;
        switch (error)
        {
        case EGL_SUCCESS:				pError = (char *)"EGL_SUCCESS"; break;
        case EGL_NOT_INITIALIZED:		pError = (char *)"EGL_NOT_INITIALIZED"; break;
        case EGL_BAD_ACCESS:			pError = (char *)"EGL_BAD_ACCESS"; break;
        case EGL_BAD_ALLOC:				pError = (char *)"EGL_BAD_ALLOC"; break;
        case EGL_BAD_ATTRIBUTE:			pError = (char *)"EGL_BAD_ATTRIBUTE"; break;
        case EGL_BAD_CONTEXT:			pError = (char *)"EGL_BAD_CONTEXT"; break;
        case EGL_BAD_CONFIG:			pError = (char *)"EGL_BAD_CONFIG"; break;
        case EGL_BAD_CURRENT_SURFACE:	pError = (char *)"EGL_BAD_CURRENT_SURFACE"; break;
        case EGL_BAD_DISPLAY:			pError = (char *)"EGL_BAD_DISPLAY"; break;
        case EGL_BAD_SURFACE:			pError = (char *)"EGL_BAD_SURFACE"; break;
        case EGL_BAD_MATCH:				pError = (char *)"EGL_BAD_MATCH"; break;
        case EGL_BAD_PARAMETER:			pError = (char *)"EGL_BAD_PARAMETER"; break;
        case EGL_BAD_NATIVE_PIXMAP:		pError = (char *)"EGL_BAD_NATIVE_PIXMAP"; break;
        case EGL_BAD_NATIVE_WINDOW:		pError = (char *)"EGL_BAD_NATIVE_WINDOW"; break;
        case EGL_CONTEXT_LOST:			pError = (char *)"EGL_CONTEXT_LOST"; break;
        default:
            pError = (char *)"Unknown EGL Error!";
            LOGE("Unknown! (0x%x)", error);
            break;
        }

        LOGE("MotionVector: eglQueryContext (context = %p) failed (Egl Error = %s)", gAppContext->modeContext->warpRenderContext, pError);

        return false;
    }
    if (fLogEgl) LOGI("MotionVector: Current Config ID for context = %p: %d", gAppContext->modeContext->warpRenderContext, currentConfigId);

    // ... loop through all configs to find which one this is ...
    eglGetConfigs(display, configs, 1024, &numConfigs);
    if (fLogEgl) LOGI("MotionVector: Found %d EGL configurations", numConfigs);
    int i = 0;
    for (; i < numConfigs; i++)
    {
        EGLint tmpConfigId = -1;
        eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID, &tmpConfigId);
        if (fLogEgl) LOGI("    Configuration %d has ID = %d", i, tmpConfigId);
        if (tmpConfigId == currentConfigId)
        {
            if (fLogEgl) LOGI("    Configuration %d matches desired configuration", i);
            pbufferConfig = configs[i];
            break;
        }
    }
    if (i >= numConfigs)
    {
        LOGE("MotionVector: Unable to find configuration that matches current context!");
        return false;
    }

    // ... and create the Pbuffer surface (16x16) with this config
    const EGLint surfaceAttribs[] =
    {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };

    surface = eglCreatePbufferSurface(display, pbufferConfig, surfaceAttribs);
    if (surface == EGL_NO_SURFACE)
    {

        EGLint error = eglGetError();

        char *pError = NULL;
        switch (error)
        {
        case EGL_SUCCESS:				pError = (char *)"EGL_SUCCESS"; break;
        case EGL_NOT_INITIALIZED:		pError = (char *)"EGL_NOT_INITIALIZED"; break;
        case EGL_BAD_ACCESS:			pError = (char *)"EGL_BAD_ACCESS"; break;
        case EGL_BAD_ALLOC:				pError = (char *)"EGL_BAD_ALLOC"; break;
        case EGL_BAD_ATTRIBUTE:			pError = (char *)"EGL_BAD_ATTRIBUTE"; break;
        case EGL_BAD_CONTEXT:			pError = (char *)"EGL_BAD_CONTEXT"; break;
        case EGL_BAD_CONFIG:			pError = (char *)"EGL_BAD_CONFIG"; break;
        case EGL_BAD_CURRENT_SURFACE:	pError = (char *)"EGL_BAD_CURRENT_SURFACE"; break;
        case EGL_BAD_DISPLAY:			pError = (char *)"EGL_BAD_DISPLAY"; break;
        case EGL_BAD_SURFACE:			pError = (char *)"EGL_BAD_SURFACE"; break;
        case EGL_BAD_MATCH:				pError = (char *)"EGL_BAD_MATCH"; break;
        case EGL_BAD_PARAMETER:			pError = (char *)"EGL_BAD_PARAMETER"; break;
        case EGL_BAD_NATIVE_PIXMAP:		pError = (char *)"EGL_BAD_NATIVE_PIXMAP"; break;
        case EGL_BAD_NATIVE_WINDOW:		pError = (char *)"EGL_BAD_NATIVE_WINDOW"; break;
        case EGL_CONTEXT_LOST:			pError = (char *)"EGL_CONTEXT_LOST"; break;
        default:
            pError = (char *)"Unknown EGL Error!";
            LOGE("Unknown! (0x%x)", error);
            break;
        }

        LOGE("MotionVector: eglCreatePbufferSurface failed (Egl Error = %s)", pError);

        return false;
    }

    // Create the SpaceWarp context. The third parameter specifies the context with which to share data.
    // Share with the eye render context, which is in turn also shared with TimeWarp.
    EGLint contextAttribs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context = eglCreateContext(display, config, gAppContext->modeContext->warpRenderContext, contextAttribs);
    if (context == EGL_NO_CONTEXT)
    {
        LOGE("MotionVector: Failed to create EGL context");
        return false;
    }

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
    {
        LOGE("MotionVector: eglMakeCurrent failed");
        return false;
    }

    // Save the characteristics of the surface
    EGLint surfaceWidth = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &surfaceWidth);

    EGLint surfaceHeight = 0;
    eglQuerySurface(display, surface, EGL_HEIGHT, &surfaceHeight);

    LOGI("MotionVector: Surface Dimensions ( %d x %d )", surfaceWidth, surfaceHeight);

    gpMotionData[whichMotionData].surfaceWidth = surfaceWidth;
    gpMotionData[whichMotionData].surfaceHeight = surfaceHeight;
    gpMotionData[whichMotionData].eglDisplay = display;
    gpMotionData[whichMotionData].eglSurface = surface;
    gpMotionData[whichMotionData].eglContext = context;

    LOGI("Motion vector graphics context for MotionThread %d successfully created", whichMotionData + 1);

    return true;
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
void L_DestroyMotionContext(uint32_t whichMotionData)
{
    if (gpMotionData == NULL)
    {
        LOGI("MotionVector: Unable to Destroy surface/context for MotionThread %d. Motion data has not been initialized", whichMotionData + 1);
        return;
    }

    LOGI("Destroying %dx%d motion vector graphics context for MotionThread %d", gpMotionData[whichMotionData].surfaceWidth, gpMotionData[whichMotionData].surfaceHeight, whichMotionData + 1);

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY)
    {
        LOGE("MotionVector: Cannot destroy motion context. Unable to get current display!");
        return;
    }

    if (gpMotionData[whichMotionData].eglSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(display, gpMotionData[whichMotionData].eglSurface);
        gpMotionData[whichMotionData].eglSurface = EGL_NO_SURFACE;
    }

    if (gpMotionData[whichMotionData].eglContext != EGL_NO_CONTEXT)
    {
        eglDestroyContext(display, gpMotionData[whichMotionData].eglContext);
        gpMotionData[whichMotionData].eglContext = EGL_NO_CONTEXT;
    }

    gpMotionData[whichMotionData].surfaceWidth = 0;
    gpMotionData[whichMotionData].surfaceHeight = 0;
    gpMotionData[whichMotionData].eglDisplay = EGL_NO_DISPLAY;

    LOGI("Motion vector graphics context for MotionThread %d successfully destroyed", whichMotionData + 1);
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
void L_WaitForRender(SvrMotionData *pThisData, Svr::svrFrameParamsInternal* pWarpFrame, GLuint64 timeoutNano, char *threadName)
{
    // TODO: This is similar to code in TimeWarp to wait for a frame but that code is in a loop with zero wait

    // Check to see if the frame has already finished on the GPU
    // Determine it is Vulkan by looking at the first eyebuffer.
    if (pWarpFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore != 0)
    {
        // This is a Vulkan frame
        // Need a GL handle to the semaphore, which comes to us as a fence
        // We don't have the handle, get it before we can wait on it
        EGLint vSyncAttribs[] =
        {
            EGL_SYNC_NATIVE_FENCE_FD_ANDROID, (EGLint)pWarpFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore,
            EGL_NONE,
        };

        // LOGI("Getting sync handle from external fd (%d)...", (int)pWarpFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore);

        EGLSyncKHR externalSync = eglCreateSyncKHR(pThisData->eglDisplay, EGL_SYNC_NATIVE_FENCE_ANDROID, vSyncAttribs);
        if (externalSync == EGL_NO_SYNC_KHR)
        {
            LOGE("Unable to create sync fence from external fd!");
            return;
        }
        else
        {
            // LOGI("    Sync handle from external fd %d => 0x%x", (int)pWarpFrame->frameParams.renderLayers[0].vulkanInfo.renderSemaphore, (int)externalSync);
        }

        // Now loop until it is done or we time out
        uint64_t timeStart = Svr::GetTimeNano();
        uint64_t timeNow = Svr::GetTimeNano();
        bool isSignaled = false;
        bool isError = false;
        while (timeNow - timeStart < timeoutNano && !isSignaled && !isError)
        {
            // We can only call eglClientWaitSync on the sync object imported from the Vk semaphore once. That seems
            // to be the case even with the special timeout value of 0, which should only check the status. So instead
            // let's use eglGetSyncAttribKHR to get the status.
            EGLint syncStatus = EGL_UNSIGNALED_KHR;
            EGLBoolean eglResult = EGL(eglGetSyncAttribKHR(pThisData->eglDisplay, externalSync, EGL_SYNC_STATUS_KHR, &syncStatus));

            if (eglResult != EGL_TRUE)
            {
                LOGE("Error checking status of external sync: %d", eglResult);
                isError = true;
            }
            else
            {
                switch (syncStatus)
                {
                case EGL_SIGNALED_KHR:      // 0x30F2
                    // Finished rendering
                    isSignaled = true;
                    break;
                case EGL_UNSIGNALED_KHR:    // 0x30F3
                    // Not finished rendering
                    isSignaled = false;
                    break;

                }
                if (syncStatus == EGL_UNSIGNALED_KHR)
                {
                    // This frame has not finished
                    //LOGI("TIMEWARP: GPU not finished with frame %d", checkFrameCount);
                }
            }

            // Update the time we have been trying
            timeNow = Svr::GetTimeNano();
        }

        // If done waiting due to timeout, log it
        if (timeNow - timeStart >= timeoutNano)
        {
            LOGE("%s: Timeout waiting for eye buffer fence", threadName);
        }
    }   // Vulkan Fence
    else
    {
        // This is a GL Frame
        GLenum syncResult = GL(glClientWaitSync(pWarpFrame->frameSync, GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNano));
        switch (syncResult)
        {
        case GL_ALREADY_SIGNALED:
            // LOGI("%s: Eye buffer fence signaled", threadName);
            break;

        case GL_TIMEOUT_EXPIRED:
            LOGE("%s: Timeout waiting for eye buffer fence", threadName);
            break;

        case GL_CONDITION_SATISFIED:
            // LOGI("%s: Eye buffer fence satisfied", threadName);
            break;

        case GL_WAIT_FAILED:
            LOGE("%s: Wait for eye buffer fence returned GL_WAIT_FAILED", threadName);
            break;
        }
    }   // It is a GL frame

}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
//-----------------------------------------------------------------------------
void* MotionThreadMain(void* arg)
//-----------------------------------------------------------------------------
{

    // The argument is which set of motion data in gpMotionData
    uint32_t whichMotionData = *((uint32_t *)arg);

    SvrMotionData *pThisData = &gpMotionData[whichMotionData];

    if (whichMotionData >= NUM_MOTION_VECTOR_BUFFERS)
    {
        LOGE("MotionThreadMain: Data index out of range (%d of %d)!  Thread exiting...", whichMotionData + 1, NUM_MOTION_VECTOR_BUFFERS);
        return 0;
    }

    char threadName[256];
    memset(threadName, 0, sizeof(threadName));
    if (whichMotionData == 0)
    {
        sprintf(threadName, "(MotionThread Left)");
    }
    else if (whichMotionData == 1)
    {
        sprintf(threadName, "(MotionThread Right)");
    }
    else
    {
        sprintf(threadName, "(MotionThread %d)", whichMotionData + 1);
    }

    pThisData->threadId = gettid();
    // LOGI("MotionThreadMain (%d of %d) Entered (Thread id = %d)", whichMotionData + 1, NUM_MOTION_VECTOR_BUFFERS, (int)pThisData->threadId);
    LOGI("MotionThreadMain (%s) Entered (Thread id = %d)", threadName, (int)pThisData->threadId);


    // Set the thread priority
    if (gUseQvrPerfModule && gAppContext->qvrHelper)
    {
        // LOGI("Calling SetThreadAttributesByType for Spacewarp thread: %s", threadName);
        int32_t qRes = QVRServiceClient_SetThreadAttributesByType(gAppContext->qvrHelper, pThisData->threadId, NORMAL);
        if (qRes != QVR_SUCCESS)
        {
            LOGE("QVRServiceClient_SetThreadAttributesByType failed for Spacewarp thread: %s", threadName);
        }
    }
    else
    {
        int normalPriority = 0;
        L_SetThreadPriority(threadName, SCHED_NORMAL, normalPriority);
    }

    // Need one ME_Init() for each thread.
    // Initialize motion vector library
    uint32_t inputWidth = (uint32_t)(gMotionVectorScale * (float)gAppContext->deviceInfo.targetEyeWidthPixels);     // TODO: No guarantee this is the correct size
    uint32_t inputHeight = (uint32_t)(gMotionVectorScale * (float)gAppContext->deviceInfo.targetEyeHeightPixels);
    ME_InputFormat vectorFormat = ME_InputFormat_YUV420_SemiPlanar;

    unsigned int flags = 0;
    unsigned int meshWidth;
    unsigned int meshHeight;
    unsigned int versionNumber = 0;
    LOGI("****************************************");
    LOGI("MotionThreadMain: %s Initializing Motion Vectors (Input = %dx%d)...", threadName, inputWidth, inputHeight);
    LOGI("****************************************");
    ME_Init(inputWidth,                                 // [IN]  Width of images being provided as input for motion calculation
            inputHeight,                                // [IN]  Height of images that will be provided as input for motion calculation
            vectorFormat,                               // [IN]  Format of the data in the input buffers
            flags,                                      // [IN]  Init flags
            &meshWidth,                                 // [OUT] Width of the vector array that will be produced
            &meshHeight,                                // [OUT] Height of the vector array that will be produced
            &versionNumber,                             // [OUT] Version number of the library
            &pThisData->hInstance);                     // [OUT] Handle for the created motion engine instance

    if (pThisData->hInstance == 0)
    {
        LOGE("MotionThreadMain: %s Unable to initialize Motion Vectors!", threadName);
        return 0;
    }

    LOGI("MotionThreadMain: %s Motion Vectors Initialized!", threadName);
    LOGI("    MotionThreadMain: %s Version %d", threadName, versionNumber);
    LOGI("    MotionThreadMain: %s Output Width %d", threadName, meshWidth);
    LOGI("    MotionThreadMain: %s Output Height %d", threadName, meshHeight);
    LOGI("    MotionThreadMain: %s Handle = %p", threadName, pThisData->hInstance);

    // ********************************
    // Start - Init structure members
    // ********************************

    // State Variables
    pThisData->waitingForCallback = false;
    pThisData->dataIsNew = false;

    // Motion Vectors
    pThisData->meshWidth = meshWidth;
    pThisData->meshHeight = meshHeight;

    pThisData->motionVectorSize = sizeof(ME_MotionVector) * pThisData->meshWidth * pThisData->meshHeight;
    pThisData->motionVectors = (ME_MotionVector *)malloc(pThisData->motionVectorSize);
    if (pThisData->motionVectors == NULL)
    {
        LOGE("MotionThreadMain: %s Unable to allocate %d bytes for Motion Vector buffer!", threadName, pThisData->motionVectorSize);
    }
    else
    {
        memset(pThisData->motionVectors, 0, pThisData->motionVectorSize);
    }

    pThisData->smoothedVectors = (ME_MotionVector *)malloc(pThisData->motionVectorSize);
    if (pThisData->smoothedVectors == NULL)
    {
        LOGE("MotionThreadMain: %s Unable to allocate %d bytes for Motion Vector (smoothed) buffer!", threadName, pThisData->motionVectorSize);
    }
    else
    {
        memset(pThisData->smoothedVectors, 0, pThisData->motionVectorSize);
    }

    // Image Buffer Input
    pThisData->imageWidth = inputWidth;
    pThisData->imageHeight = inputHeight;

    pThisData->newBuffer = 0;
    pThisData->oldBuffer = 1;

    // Start our image buffers in a known state
    for (int whichSet = 0; whichSet < NUM_DUAL_BUFFERS; whichSet++)
    {
        for (int whichType = 0; whichType < kNumMotionTypes; whichType++)
        {
            // The actual buffers...
            pThisData->dualBuffers[whichSet][whichType].inputBuffer = (EGLClientBuffer)0;
            pThisData->dualBuffers[whichSet][whichType].imageFrameBuffer = 0;
            pThisData->dualBuffers[whichSet][whichType].imageTexture = 0;
            pThisData->dualBuffers[whichSet][whichType].eglImage = (EGLImageKHR)0;

            memset(&pThisData->dualBuffers[whichSet][whichType].headPoseState, 0, sizeof(svrHeadPoseState));

            //... and the debug ones for gRenderMotionInput
            pThisData->debugBuffers[whichSet][whichType].inputBuffer = (EGLClientBuffer)0;
            pThisData->debugBuffers[whichSet][whichType].imageFrameBuffer = 0;
            pThisData->debugBuffers[whichSet][whichType].imageTexture = 0;
            pThisData->debugBuffers[whichSet][whichType].eglImage = (EGLImageKHR)0;

            memset(&pThisData->debugBuffers[whichSet][whichType].headPoseState, 0, sizeof(svrHeadPoseState));
        }
    }

    // ********************************
    // End - Init structure members
    // ********************************


    // Create the graphics context for this thread
    L_CreateMotionContext(whichMotionData);

    // Create the textures used by the main loop
    // L_CreateMotionVectorImage(pThisData);   <= Called every frame now
    L_CreateMotionVectorResult(pThisData);

    // Create the shaders used by this thread
    int numVertStrings = 0;
    int numFragStrings = 0;
    const char *vertShaderStrings[16];
    const char *fragShaderStrings[16];

    // Blit YUV Shader
    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs;

    LOGI("MotionThreadMain: %s Initializing blitYuvShader...", threadName);
    if (!pThisData->blitYuvShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, "svrBlitQuadVs", "svrBlitQuadYuvFs"))
    {
        LOGE("MotionThreadMain: %s Failed to initialize blitYuvShader!", threadName);
    }

    // Blit YUV Array Shader
    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs_Array;

    LOGI("MotionThreadMain: %s Initializing blitYuvShader_Array...", threadName);
    if (!pThisData->blitYuvArrayShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, "svrBlitQuadVs", "svrBlitQuadYuvFs_Array"))
    {
        LOGE("MotionThreadMain: %s Failed to initialize blitYuvArrayShader!", threadName);
    }

    // Blit YUV Image Shader
    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBlitQuadVs;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs_Image;

    LOGI("MotionThreadMain: %s Initializing blitYuvShader_Image...", threadName);
    if (!pThisData->blitYuvImageShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, "svrBlitQuadVs_Image", "svrBlitQuadYuvFs_Image"))
    {
        LOGE("MotionThreadMain: %s Failed to initialize blitYuvShader_Image!", threadName);
    }

    // Blit YUV Shader with Warp
    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBlitQuadWarpVs;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs;

    LOGI("MotionThreadMain: %s Initializing blitYuvWarpShader...", threadName);
    if (!pThisData->blitYuvWarpShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, "svrBlitQuadVs", "svrBlitQuadYuvFs"))
    {
        LOGE("MotionThreadMain: %s Failed to initialize blitYuvWarpShader!", threadName);
    }

    // Blit YUV Array Shader with Warp
    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBlitQuadWarpVs;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs_Array;

    LOGI("MotionThreadMain: %s Initializing blitYuvWarpArrayShader...", threadName);
    if (!pThisData->blitYuvWarpArrayShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, "svrBlitQuadWarpVs", "svrBlitQuadYuvArrayFs"))
    {
        LOGE("MotionThreadMain: %s Failed to initialize blitYuvWarpArrayShader!", threadName);
    }

    // Blit YUV Image Shader
    numVertStrings = 0;
    vertShaderStrings[numVertStrings++] = svrBlitQuadWarpVs;

    numFragStrings = 0;
    fragShaderStrings[numFragStrings++] = svrBlitQuadYuvFs_Image;

    LOGI("MotionThreadMain: %s Initializing blitYuvWarpShader_Image...", threadName);
    if (!pThisData->blitYuvWarpImageShader.Initialize(numVertStrings, vertShaderStrings, numFragStrings, fragShaderStrings, "svrBlitQuadVs_Image", "svrBlitQuadYuvFs_Image"))
    {
        LOGE("MotionThreadMain: %s Failed to initialize blitYuvWarpShader_Image!", threadName);
    }

    if (gRenderMotionInput)
    {
        // Need to actually create the debug buffers
        for (int whichSet = 0; whichSet < NUM_DUAL_BUFFERS; whichSet++)
        {
            for (int whichType = 0; whichType < kNumMotionTypes; whichType++)
            {
                if (!L_CreateMotionVectorImage(pThisData, &pThisData->debugBuffers[whichSet][whichType]))
                {
                    // Not sure what to do here except log it
                    LOGE("Failed to create new motion vector input buffer or surface (for rendering motion input)");
                }
            }
        }
    }

    // Loop forever, or until told to stop
    uint64_t spinTimeout = 250000000; // 250 ms
    LOGI("MotionThreadMain: %s looping...", threadName);
    while (gpMotionData && !pThisData->threadShouldExit)
    {
        // Let the world know we are ready for work
        if (gLogMotionVectors && gLogThreadState) LOGI("MotionThreadMain: %s waiting for work...", threadName);
        L_ReleaseSem(whichMotionData, eSEM_AVAILABLE);
        if (gLogMotionVectors && gLogThreadState) LOGI("MotionThreadMain: %s waiting for actual data to work on...", threadName);

        // Wait for something to do...
        while (!pThisData->threadShouldExit && !L_GetSem(whichMotionData, eSEM_DO_WORK, spinTimeout))
        {
            // Spin here until either not running or told to do work
        }

        // We are out, are we working or leaving
        if (pThisData->threadShouldExit)
        {
            // Just continue so the main loop will leave
            if (gLogMotionVectors && gLogThreadState) LOGI("MotionThreadMain: %s thread exit flag (%d) has been set...", threadName, (int)pThisData->threadShouldExit);
            continue;
        }

        // If we don't have anything to do, go back and wait
        if (pThisData->pWarpFrame == NULL)
        {
            if (gLogMotionVectors && gLogThreadState) LOGI("MotionThreadMain: %s woke up with nothing to do...", threadName);
            continue;
        }

        // Process this new input
        if (gLogMotionVectors && gLogThreadState) LOGI("MotionThreadMain: %s working...", threadName);

        // Need the head pose for this frame so warp works later.
        // Not sure which one we are using later so set them both
        if (gWarpMotionVectors)
        {
            pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeStatic].headPoseState = pThisData->pWarpFrame->frameParams.headPoseState;
            pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeWarped].headPoseState = pThisData->pWarpFrame->frameParams.headPoseState;
        }

        // Can't do anything with this eye buffer until it's render is finished
        GLuint64 timeout = MILLISECONDS_TO_NANOSECONDS * 50;

        // This is either a GL or a Vulkan frame.  The wait is handled different
        L_WaitForRender(pThisData, pThisData->pWarpFrame, timeout, threadName);

        // The image gets consumed when generating motion vectors so we have to create them each frame.
        // When creating, do both the warped and the normal.  Then after creation destroy the warped
        // and the normal from LAST frame.
        bool staticResult = L_CreateMotionVectorImage(pThisData, &pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeStatic]);
        bool warpResult = true;
        if (gWarpMotionVectors)
        {
            warpResult = L_CreateMotionVectorImage(pThisData, &pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeWarped]);
        }

        if (!staticResult || !warpResult)
        {
            // Not sure what to do here except log it
            LOGE("Failed to create new motion vector input buffer or surface");
            continue;
        }

        // Use the current left eye to generate motion vectors
        if (gLogMotionVectors)
            LOGI("SetMotionVectorInput() for buffer %d", whichMotionData);

        // Need both the warped and static version created
        glm::mat3 warpMatrix = glm::mat3(1.0f);
        L_SetMotionVectorInput(pThisData, pThisData->pWarpFrame, &pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeStatic], kMotionTypeStatic, warpMatrix, glm::vec2(0.0f, 0.0f));

        if (gRenderMotionInput)
        {
            // LOGI("DEBUG! L_SetMotionVectorInput (Left indx = %d) = %d", pThisData->newBuffer, pThisData->debugBuffers[pThisData->newBuffer][kMotionTypeStatic].imageTexture);
            L_SetMotionVectorInput(pThisData, pThisData->pWarpFrame, &pThisData->debugBuffers[pThisData->newBuffer][kMotionTypeStatic], kMotionTypeStatic, warpMatrix, glm::vec2(0.0f, 0.0f));
        }

        if (gWarpMotionVectors)
        {
            // Need warp matrix to remove head motion from motion data
            glm::fquat newQuat = glmQuatFromSvrQuat(pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeStatic].headPoseState.pose.rotation);
            glm::fquat oldQuat = glmQuatFromSvrQuat(pThisData->dualBuffers[pThisData->oldBuffer][kMotionTypeStatic].headPoseState.pose.rotation);

            // Need to get the difference between new and old
            glm::fquat recenterRot = inverse(oldQuat);
            glm::fquat diffQuat = newQuat * recenterRot;

            // Rotation Matrix around Z:
            //      Cos -Sin  0
            //      Sin  Cos  0
            //        0    0  1
            glm::vec3 diffEuler = glm::eulerAngles(diffQuat);
            glm::mat3 transMat = glm::mat3(1.0f);

            transMat[0].x = cos(diffEuler.z);
            transMat[0].y = sin(diffEuler.z);
            transMat[1].x = -transMat[0].y;
            transMat[1].y = transMat[0].x;
            // LOGI("DEBUG! Diff Quat (Euler): (Pitch = %0.4f; Yaw = %0.4f; Roll = %0.4f)", diffEuler.x, diffEuler.y, diffEuler.z);

            glm::vec3 Forward = glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 LookDir = diffQuat * Forward;
            // LOGI("DEBUG! Look Dir: (%0.4f, %0.4f, %0.4f)", -LookDir.x, -LookDir.y, LookDir.z);

            // We now have a vector that we need to set Z to be the distance to near plane
            if(fabs(LookDir.z) > 0.000001f)   // Epsilon check so not dividing by zero
                LookDir *= (gAppContext->deviceInfo.leftEyeFrustum.near / LookDir.z);
            // LOGI("DEBUG!    Adjust Near: (%0.4f, %0.4f, %0.4f)", -LookDir.x, -LookDir.y, LookDir.z);

            float xTotal = (gAppContext->deviceInfo.leftEyeFrustum.right - gAppContext->deviceInfo.leftEyeFrustum.left);
            float xAdjust = 0.0f;
            if (xTotal != 0.0f)
                xAdjust = LookDir.x / xTotal;

            float yTotal = (gAppContext->deviceInfo.leftEyeFrustum.top - gAppContext->deviceInfo.leftEyeFrustum.bottom);
            float yAdjust = 0.0f;
            if (yTotal != 0.0f)
                yAdjust = LookDir.y / yTotal;

            // Screen is [-1, 1] and adjustment is [0,1]
            xAdjust *= 2.0f;
            yAdjust *= 2.0f;
            // LOGI("DEBUG!    Screen Adjust: (%0.4f, %0.4f)", xAdjust, yAdjust);

            // Needed a tuning parameter since motion is not quite correct
            xAdjust *= gWarpMotionScale;
            yAdjust *= gWarpMotionScale;
            // LOGI("DEBUG!    Screen  Scale Adjust: (%0.4f, %0.4f)", xAdjust, yAdjust);

            // Need the translation in the transform matrix
            transMat[2].x = xAdjust;
            transMat[2].y = yAdjust;

            L_SetMotionVectorInput(pThisData, pThisData->pWarpFrame, &pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeWarped], kMotionTypeWarped, transMat, glm::vec2(0.0f, 0.0f));

            if (gRenderMotionInput)
            {
                // LOGI("DEBUG! L_SetMotionVectorInput (Right indx = %d) = %d", pThisData->newBuffer, pThisData->debugBuffers[pThisData->newBuffer][kMotionTypeWarped].imageTexture);
                L_SetMotionVectorInput(pThisData, pThisData->pWarpFrame, &pThisData->debugBuffers[pThisData->newBuffer][kMotionTypeWarped], kMotionTypeWarped, transMat, glm::vec2(0.0f, 0.0f));
            }
        }

        // Generate the motion vectors
        if (gEnableYuvDecode)
        {
            if (gLogMotionVectors)
                LOGI("%s: ME_GenerateVectors() for buffer %d", threadName, whichMotionData);

            PROFILE_ENTER(GROUP_TIMEWARP, 0, "Generating Motion Vectors for Buffer %d", whichMotionData);
            pThisData->waitingForCallback = true;
            pThisData->timer.Start();

            // Generate vector with old static and current warped
            EGLClientBuffer inputBufferA = pThisData->dualBuffers[pThisData->oldBuffer][kMotionTypeStatic].inputBuffer;

            EGLClientBuffer inputBufferB = pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeStatic].inputBuffer;
            if(gWarpMotionVectors)
                inputBufferB = pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeWarped].inputBuffer;


            if (inputBufferA == 0 || inputBufferB == 0)
            {
                LOGE("Unable to generate motion vectors: An input buffer has not been created (Can ignore if first time through)");
            }
            else
            {
                bool fReturn = false;
                if (false)
                {
                    fReturn = ME_GenerateVectors_Sequential(pThisData->hInstance, inputBufferB, pThisData->motionVectors);
                }
                else
                {
                    if (gLogMotionVectors)
                        LOGI("%s:    ME_GenerateVectors_DualBuffer: %p -> %p", threadName, inputBufferA, inputBufferB);

                    fReturn = ME_GenerateVectors_DualBuffer(pThisData->hInstance, inputBufferA, inputBufferB, pThisData->motionVectors);
                }

                if (!fReturn)
                {
                    // What do we do with this error?
                    LOGE("%s: ME_GenerateVectors returned False!", threadName);
                }
            }

            pThisData->waitingForCallback = false;
            pThisData->dataIsNew = true;
            double elapsedTime = pThisData->timer.Stop();

            if (gLogMotionVectors)
                LOGI("    ME_GenerateVectors() for buffer %d returned (%0.4f ms)", whichMotionData, elapsedTime);

            PROFILE_EXIT(GROUP_TIMEWARP);   // "Generating Motion Vectors for Buffer %d"
        }   // gEnableYuvDecode

        // ME_GenerateVectors() now consumes the inputBuffer and the generated surface
        // Destroy the current warped version and the old static version
        L_DestroyMotionVectorImage(pThisData, &pThisData->dualBuffers[pThisData->oldBuffer][kMotionTypeStatic]);

        if(gWarpMotionVectors)
            L_DestroyMotionVectorImage(pThisData, &pThisData->dualBuffers[pThisData->newBuffer][kMotionTypeWarped]);

        // Change the old and new variables
        pThisData->newBuffer = 1 - pThisData->newBuffer;
        pThisData->oldBuffer = 1 - pThisData->oldBuffer;

        // Process the data (Smooth and put into a texture)...
        if(gUseMotionVectors)
            ProcessMotionData(whichMotionData);

        if (gLogMotionVectors && gLogThreadState) LOGI("MotionThreadMain: %s done working...", threadName);

        // TODO: Need a thread safe way to update this variable!

        // Need to update which one is most recent
        if (pThisData->eyeMask & kEyeMaskLeft)
        {
            gMostRecentMotionLeft = whichMotionData;
            // if (gLogMotionVectors) LOGI("Most recent (left) motion data is %d (id = %d)", gMostRecentMotionLeft, pThisData->resultTexture);

            // If NOT doing both eyes, then set the right eye most recent also
            if (!gGenerateBothEyes)
            {
                gMostRecentMotionRight = whichMotionData;
                // if (gLogMotionVectors) LOGI("Most recent (right) motion data is %d (id = %d)", gMostRecentMotionRight, pThisData->resultTexture);
            }
        }
        else if (pThisData->eyeMask & kEyeMaskRight)
        {
            gMostRecentMotionRight = whichMotionData;
            // if (gLogMotionVectors) LOGI("Most recent (right) motion data is %d (id = %d)", gMostRecentMotionRight, pThisData->resultTexture);
        }
        else
        {
            LOGI("Error! No idea if this is Left or Right! Recent Left/Right = %d/%d", gMostRecentMotionLeft, gMostRecentMotionRight);
        }

    }

    if (pThisData->threadShouldExit)
    {
        LOGI("MotionThreadMain: %s exit looping due to flag...", threadName);
    }

    if (gpMotionData == NULL)
    {
        LOGE("**************************************************");
        LOGE("MotionThreadMain: Motion Data memory freed before thread %s could exit!", threadName);
        LOGE("**************************************************");
    }

    // Clean up shaders
    pThisData->blitYuvShader.Destroy();
    pThisData->blitYuvArrayShader.Destroy();
    pThisData->blitYuvImageShader.Destroy();
    pThisData->blitYuvWarpShader.Destroy();
    pThisData->blitYuvWarpArrayShader.Destroy();
    pThisData->blitYuvWarpImageShader.Destroy();

    // Clean up input value that may still be around from last set
    for (int whichSet = 0; whichSet < NUM_DUAL_BUFFERS; whichSet++)
    {
        // Know there are only two of these...
        // L_DestroyMotionVectorImage(pThisData, whichSet, kMotionTypeStatic);
        // L_DestroyMotionVectorImage(pThisData, whichSet, kMotionTypeWarped);

        //... but looping to be cleaner
        for (int whichType = 0; whichType < kNumMotionTypes; whichType++)
        {
            L_DestroyMotionVectorImage(pThisData, &pThisData->dualBuffers[whichSet][whichType]);

            if (gRenderMotionInput)
            {
                L_DestroyMotionVectorImage(pThisData, &pThisData->debugBuffers[whichSet][whichType]);
            }
        }
    }

    // Clean up textures
    // L_DestroyMotionVectorImage(pThisData); <= Now done per frame
    L_DestroyMotionVectorResult(pThisData);

    // Destroy the motion context
    L_DestroyMotionContext(whichMotionData);

    LOGI("MotionThreadMain: %s Releasing Motion Vectors library...", threadName);
    ME_Destroy(pThisData->hInstance);

    LOGI("MotionThreadMain: %s exiting...", threadName);

    pThisData->threadClosed = true;

    return 0;
}
#endif // ENABLE_MOTION_VECTORS

#if defined(ENABLE_MOTION_VECTORS)
void InitializeMotionVectors()
{
    if (gLogMotionVectors)
        LOGI("Initializing Motion Vectors...");

    // Start/Stop logic tells us we may or may not be in an initialized state
    if (gpMotionData != NULL)
        TerminateMotionVectors();

    // Allocate our list of structures
    LOGI("**************************************************");
    LOGI("**************************************************");
    LOGI("Allocating motion data structures!!!");
    LOGI("**************************************************");
    LOGI("**************************************************");
    gpMotionData = new SvrMotionData[NUM_MOTION_VECTOR_BUFFERS];
    if (gpMotionData == NULL)
    {
        LOGE("Unable to allocate %d bytes for motion data structures!", NUM_MOTION_VECTOR_BUFFERS * (int)sizeof(SvrMotionData));
        return;
    }

    // Do NOT clear this!  It breaks the constructor!
    // memset(gpMotionData, 0, sizeof(SvrMotionData) * NUM_MOTION_VECTOR_BUFFERS);

    int status = 0;

    // Need some thread attributes in case we want to change anything
    pthread_attr_t  threadAttribs;
    status = pthread_attr_init(&threadAttribs);
    if (status != 0)
    {
        LOGE("InitializeMotionVectors: Failed to initialize thread attributes");
    }

    // Set some parameters and spawn the motion vector threads
    for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
    {
        gpMotionData[whichMotionData].listIndex = whichMotionData;

        // EGL Variables
        gpMotionData[whichMotionData].eglDisplay = EGL_NO_DISPLAY;
        gpMotionData[whichMotionData].eglSurface = EGL_NO_SURFACE;
        gpMotionData[whichMotionData].eglContext = EGL_NO_CONTEXT;

        // Thread control
        gpMotionData[whichMotionData].threadShouldExit = false;
        gpMotionData[whichMotionData].threadClosed = false;

        // Handle to MotionEngine
        gpMotionData[whichMotionData].hInstance = NULL;

        // Thread Semaphores: Initialize all to 0 count
        if (0 != sem_init(&gpMotionData[whichMotionData].semAvailable, 0, 0))
            LOGE("InitializeMotionVectors: Unable to initialize semaphore!");
        if (0 != sem_init(&gpMotionData[whichMotionData].semDoWork, 0, 0))
            LOGE("InitializeMotionVectors: Unable to initialize semaphore!");
        if (0 != sem_init(&gpMotionData[whichMotionData].semDone, 0, 0))
            LOGE("InitializeMotionVectors: Unable to initialize semaphore!");

// #ifdef WORKER_THREAD_VERSION
        // Thread creation
        char threadName[256];
        memset(threadName, 0, sizeof(threadName));
        sprintf(threadName, "MotionVector %d", whichMotionData + 1);

        void *pParam = static_cast<void*>(&gpMotionData[whichMotionData].listIndex);
        status = pthread_create(&gpMotionData[whichMotionData].hThread, &threadAttribs, &MotionThreadMain, pParam);
        if (status != 0)
        {
            LOGE("InitializeMotionVectors: Failed to create thread (%d of %d)", whichMotionData + 1, NUM_MOTION_VECTOR_BUFFERS);
        }
        pthread_setname_np(gAppContext->modeContext->warpThread, threadName);

        // NanoSleep(MILLISECONDS_TO_NANOSECONDS * 1000);
// #endif // WORKER_THREAD_VERSION
    }

    // No longer need the thread attributes
    status = pthread_attr_destroy(&threadAttribs);
    if (status != 0)
    {
        LOGE("InitializeMotionVectors: Failed to destroy thread attributes");
    }



    // Initialize each one in the list
    for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
    {
        // Move these to the actual thread.
        // L_CreateMotionVectorImage(&gpMotionData[whichMotionData]);
        // L_CreateMotionVectorResult(&gpMotionData[whichMotionData]);
    }

    L_CreateStubMotionVectorResult();

    LOGI("Motion Vectors initialized");
}
#endif // ENABLE_MOTION_VECTORS

#ifdef ENABLE_MOTION_VECTORS
void GenerateMotionData(svrFrameParamsInternal* pWarpFrame)
{
    if (gpMotionData == NULL)
    {
        LOGE("Unable to generate motion data: Functions have not been initialized!");
        return;
    }

    // How many eyes do we have to do?
    uint32_t numEyes = 1;
    svrEyeMask eyeMask = kEyeMaskBoth;

    if (gGenerateBothEyes)
    {
        // This logic can get very complicated (figuring out first layer for each eye) so only looking at
        // first layer for each eye.
        numEyes = 2;
        if (pWarpFrame->frameParams.renderLayers[0].eyeMask == kEyeMaskBoth)
        {
            // Back to the single eye case
            numEyes = 1;
            eyeMask = kEyeMaskBoth;
        }
    }

    for (uint32_t whichEye = 0; whichEye < numEyes; whichEye++)
    {
        // Find a thread that can process this data
        bool fFoundWorker = false;
        uint64_t startTimeNano = Svr::GetTimeNano();
        uint64_t timeNowNano = Svr::GetTimeNano();
        uint64_t waitTimeNano = MILLISECONDS_TO_NANOSECONDS * 100;

        uint32_t availableData = 0;
        while (!fFoundWorker && (timeNowNano - startTimeNano < waitTimeNano))
        {
            for (uint32_t whichMotionData = 0; whichMotionData < NUM_MOTION_VECTOR_BUFFERS; whichMotionData++)
            {
                // LOGI("GenerateMotionData trying to get eSEM_AVAILABLE for thread %d", whichMotionData + 1);
                if (!L_GetSem(whichMotionData, eSEM_AVAILABLE, 0))
                    continue;

                // Got a worker!
                // LOGI("GenerateMotionData got eSEM_AVAILABLE for thread %d", whichMotionData + 1);
                fFoundWorker = true;
                availableData = whichMotionData;
                break;
            }

            timeNowNano = Svr::GetTimeNano();
        }

        if (!fFoundWorker)
        {
            // If we are here it means we couldn't find a worker
            LOGE("GenerateMotionData: Error! No worker thread available to do work");
            return;
        }

        // Later: Left Eye is ALWAYS first thread and Right Eye is ALWAYS second thread
        if (whichEye == 0 && availableData != 0)
        {
            LOGE("GenerateMotionData: Error! Left Eye worker thread not available to do work!");
            gpMotionData[availableData].pWarpFrame = NULL;
            L_ReleaseSem(availableData, eSEM_DO_WORK);
            return;
        }
        if (whichEye == 1 && availableData != 1)
        {
            LOGE("GenerateMotionData: Error! Right Eye worker thread not available to do work!");
            gpMotionData[availableData].pWarpFrame = NULL;
            L_ReleaseSem(availableData, eSEM_DO_WORK);
            return;
        }

        // We have our worker.  Fire it off.

        // Set parameters so thread can actually work!
        gpMotionData[availableData].pWarpFrame = pWarpFrame;

        if(numEyes == 1)
            gpMotionData[availableData].eyeMask = kEyeMaskBoth;
        else if(whichEye == 0)
            gpMotionData[availableData].eyeMask = kEyeMaskLeft;
        else
            gpMotionData[availableData].eyeMask = kEyeMaskRight;

        // LOGI("GenerateMotionData releasing eSEM_DO_WORK for thread %d", availableData + 1);
        L_ReleaseSem(availableData, eSEM_DO_WORK);
    }   // Each eye to be processed

}
#endif // ENABLE_MOTION_VECTORS
