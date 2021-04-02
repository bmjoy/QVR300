//******************************************************************************************************************************
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//******************************************************************************************************************************

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define EXPORT __attribute__((visibility("default")))

typedef void* ME_Handle;

enum ME_InputFormat
{
    ME_InputFormat_YUV420_SemiPlanar = 1
};

enum ME_Flags
{
    ME_Flags_PerfModeNominal = 0x01,
    ME_Flags_PerfModeTurbo   = 0x10
};

struct ME_MotionVector {
    float xMagnitude;
    float yMagnitude;
};

extern "C" { 

/*******************************************************************************************************************************
*   ME_Init
*
*   @brief
*       Create and initilize a motion engine session.
*   @return
*       outputHandle is set to 0 on failure.
*
*******************************************************************************************************************************/
EXPORT void ME_Init(unsigned int   inputWidth,      // [IN]  Width of images being provided as input for motion calculation
                    unsigned int   inputHeight,     // [IN]  Height of images that will be provided as input for motion calculation
                    ME_InputFormat format,          // [IN]  Format of the data in the input buffers
                    unsigned int   flags,           // [IN]  Init flags
                    unsigned int   *outputWidth,    // [OUT] Width of the vector array that will be produced
                    unsigned int   *outputHeight,   // [OUT] Height of the vector array that will be produced
                    unsigned int   *outputVersion,  // [OUT] Version number of the library
                    ME_Handle      *outputHandle);  // [OUT] Handle for the created motion engine session


/*******************************************************************************************************************************
*   ME_GetInputBuffer
*
*   @brief
*       Retrieves an input buffer. The input buffer should be used for a single submission and becomes invalid after being
*       passed to ME_GenerateVectors.
*   @return
*       Returns null on error.
*
*******************************************************************************************************************************/
EXPORT EGLClientBuffer ME_GetInputBuffer(ME_Handle handle); // [IN] Handle to the motion engine session


/*******************************************************************************************************************************
*   ME_GenerateVectors_Sequential
*
*   @brief
*       Generate motion vectors. The function blocks until outputMemory is populated with the motion vectors based on the
*       delta between inputBuffer and the inputBuffer from the previous submission. Vectors are in row-major order with the origin
*       at the lower left corner. Vector units are signed and normalized. inputBuffer becomes invalid once passed to
*       ME_GenerateVectors and eglImages created from it should be destroyed.
*   @return
*       True on success, false on error.
*
*******************************************************************************************************************************/
EXPORT bool ME_GenerateVectors_Sequential(ME_Handle       handle,         // [IN]  Handle to the motion engine session
                                          EGLClientBuffer inputBuffer,    // [IN]  Specification of which input buffer to consume
                                          ME_MotionVector *outputMemory); // [OUT] An allocation of (outputWidth * outputHeight) MotionVectors where the results will be stored

/*******************************************************************************************************************************
*   ME_GenerateVectors_DualBuffer
*
*   @brief
*       Generate motion vectors. The function blocks until outputMemory is populated with the motion vectors based on the
*       delta between inputBufferA and inputBufferB. Vectors are in row-major order with the origin at the lower left corner.
*       Vector units are signed and normalized.  inputBuffer becomes invalid once passed to ME_GenerateVectors and eglImages
*       created from it should be destroyed.
*   @return
*       True on success, false on error.
*
*******************************************************************************************************************************/
EXPORT bool ME_GenerateVectors_DualBuffer(ME_Handle       handle,         // [IN]  Handle to the motion engine session
                                          EGLClientBuffer inputBufferA,   // [IN]  First input buffer to consume
                                          EGLClientBuffer inputBufferB,   // [IN]  Second input buffer to consume
                                          ME_MotionVector *outputMemory); // [OUT] An allocation of (outputWidth * outputHeight) MotionVectors where the results will be stored

/*******************************************************************************************************************************
*   ME_Destroy
*
*   @brief
*       Perform clean up of session resources
*   @return
*       none
*
*******************************************************************************************************************************/
EXPORT void ME_Destroy(ME_Handle handle); // [IN] Handle to the motion engine session

} // extern "C"
