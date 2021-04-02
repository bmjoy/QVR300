//=============================================================================
// FILE: svrApiPredictiveSensor.cpp
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#include <android/looper.h>
#include <android/sensor.h>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

#include "svrApi.h"
#include "svrProfile.h"
#include "svrUtil.h"
#include "svrConfig.h"
#include "svrCpuTimer.h"


#include "private/svrApiCore.h"
#include "private/svrApiPredictiveSensor.h"
#include "private/svrApiTimeWarp.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <dlfcn.h>

#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <rapidxml.hpp>

#include <dirent.h>
using namespace rapidxml;

using namespace Svr;


typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

// Sensor Parameters
VAR(float, gSensorOrientationCorrectX,

0.0f, kVariableNonpersistent);   //Adjustment if sensors are physically rotated (degrees)
VAR(float, gSensorOrientationCorrectY,

0.0f, kVariableNonpersistent);   //Adjustment if sensors are physically rotated (degrees)
VAR(float, gSensorOrientationCorrectZ,

0.0f, kVariableNonpersistent);   //Adjustment if sensors are physically rotated (degrees)
VAR(int, gSensorHomePosition,

0, kVariableNonpersistent);   // Base device configuration. 0 = Landscape Left; 1 = Landscape Right

VAR(float, gSensorHeadOffsetX,

0.0f, kVariableNonpersistent);       //Adjustment for device physical distance from head (meters)
VAR(float, gSensorHeadOffsetY,

0.0f, kVariableNonpersistent);       //Adjustment for device physical distance from head (meters)
VAR(float, gSensorHeadOffsetZ,

0.0762, kVariableNonpersistent);     //Adjustment for device physical distance from head (meters)

VAR(float, gMinPoseQuality,

0.7, kVariableNonpersistent);           // Minimum pose quality (In range [0.0, 1.0]) allowed

VAR(float, gIMUSensorPoseDelay,

0.0, kVariableNonpersistent);       // Base IMU delay (milliseconds).  How old is pose before it is recorded?

VAR(float, gMaxPredictedTime,

75.0, kVariableNonpersistent);        // Max allowable predicted pose time (milliseconds)
VAR(bool, gLogMaxPredictedTime,

true, kVariableNonpersistent);     //Enable/disable logging when max predicted time is enforced


// Logging Options
VAR(bool, gLogRawSensorData,

false, kVariableNonpersistent);            //Enable/disable logging of sensor values directly from the service

// Debug Fixed Rotation
VAR(bool, gUseFixedRotation,

false, kVariableNonpersistent);            //Enable/disable setting of sensor rotation value directly
VAR(float, gFixedRotationQuatX,

0.0f, kVariableNonpersistent);

VAR(float, gFixedRotationQuatY,

0.0f, kVariableNonpersistent);

VAR(float, gFixedRotationQuatZ,

0.0f, kVariableNonpersistent);

VAR(float, gFixedRotationQuatW,

1.0f, kVariableNonpersistent);

// Fixed rotation speed (Euler Angles) subject to gUseFixedRotation
VAR(float, gFixedRotationSpeedX,

0.0f, kVariableNonpersistent);

VAR(float, gFixedRotationSpeedY,

0.0f, kVariableNonpersistent);

VAR(float, gFixedRotationSpeedZ,

0.0f, kVariableNonpersistent);

// Debug Fixed Position
VAR(bool, gUseFixedPosition,

false, kVariableNonpersistent);            //Enable/disable setting of sensor position value directly
VAR(float, gFixedPositionX,

0.0f, kVariableNonpersistent);

VAR(float, gFixedPositionY,

0.0f, kVariableNonpersistent);

VAR(float, gFixedPositionZ,

0.0f, kVariableNonpersistent);

VAR(float, gFixedPositionEndX,

0.0f, kVariableNonpersistent);

VAR(float, gFixedPositionEndY,

0.0f, kVariableNonpersistent);

VAR(float, gFixedPositionEndZ,

0.0f, kVariableNonpersistent);

VAR(float, gFixedPositionSpeed,

0.0f, kVariableNonpersistent);

VAR(float, gHeadRotateX, 0.0f, kVariableNonpersistent);
VAR(float, gHeadRotateLeftY, 0.0f, kVariableNonpersistent);
VAR(float, gHeadRotateRightY, 0.0f, kVariableNonpersistent);

EXTERN_VAR(int, gPredictVersion);

float gTotalFixedRotationX = 0.0f;
float gTotalFixedRotationY = 0.0f;
float gTotalFixedRotationZ = 0.0f;
uint64_t gLastFixedRotationTime = 0;

float gCurrentFixedPositionX = 0.0f;
float gCurrentFixedPositionY = 0.0f;
float gCurrentFixedPositionZ = 0.0f;
uint64_t gLastFixedPositionTime = 0;

// Offset from the time Android has to what QVR has
extern int64_t gQTimeToAndroidBoot;

// QVR has some flags that are only described in QVRSharedMem.h but are not defined.
// These flags determine the 6DOF tracking state.
// Also, since we don't want to be spammed, only display them every second or so.
uint64_t gLastBadPoseTime = 0;
uint16_t gLastTrackingState = 0;
uint16_t gLastTrackingFlags = 0;
uint64_t gLastHeadTrackingFailTime = 0;

// Tracking State (16-Bit Flag)
// Earlier there was discussion for other states:
//  INITIALIZING | TRACKING | RELOCATING | HALTED (given up)
#define QVR_RELOCATION_IN_PROGRESS              0x0001

// Tracking Warning/Errors (16-Bit Flag)
#define QVR_LOW_FEATURE_COUNT_ERROR             0x0001
#define QVR_LOW_LIGHT_ERROR                     0x0002
#define QVR_BRIGHT_LIGHT_ERROR                  0x0004
#define QVR_STEREO_CAMERA_CALIBRATION_ERROR     0x0008

bool gHasLoadCalibration = false;
glm::mat4 gCalibrationM = glm::mat4(1);


void qIntegrate(float *_q0, float *_q1, float *_q2, float *_q3, float gx, float gy, float gz) {

    float recipNorm;
    float q0 = *_q0;
    float q1 = *_q1;
    float q2 = *_q2;
    float q3 = *_q3;

    float qa = q0;
    float qb = q1;
    float qc = q2;

    q0 += (-qb * gx - qc * gy - q3 * gz);
    q1 += (qa * gx + qc * gz - q3 * gy);
    q2 += (qa * gy - qb * gz + q3 * gx);
    q3 += (qa * gz + qb * gy - qc * gx);

    // Normalise quaternion
    recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;

    *_q0 = q0;
    *_q1 = q1;
    *_q2 = q2;
    *_q3 = q3;
}

bool _predict_motion(
        float *gyro_data, const float *quatIn, float *quatOut, float delay,
        const float *gyro_data_s, const float *gyro_data_b, const float *gyro_data_bdt,
        const float *gyro_data_bdt2) {
    //LOGI("prediction time: %f\n\n", delay);
    delay /= 1000.f;

    float gx, gy, gz;

    float pred_gyro_data[3];
    float f1 = 1.1f;
    float f2 = 1.1f;
    float f3 = 5.0f;
    for (int i = 0; i < 3; ++i) {
        pred_gyro_data[i] = (gyro_data_s[i] * delay
                             + (gyro_data_b[i] * delay * delay) / (f1 * 2.0f)
                             + (gyro_data_bdt[i] * (delay * delay * delay)) / (f1 * f2 * 3.0f)
                             + (gyro_data_bdt2[i] * (delay * delay * delay * delay)) /
                               (f1 * f2 * f3 * 4.0f));
    }

    //LOGI("holt_winters: delay = %f gyro_data_s = (%f %f %f) gyro_data_b = (%f %f %f) gyro_data = (%f %f %f) pred_gyro_data = (%f %f %f)", delay, gyro_data_s[0],gyro_data_s[1],gyro_data_s[2], gyro_data_b[0],gyro_data_b[1],gyro_data_b[2], gyro_data[0],gyro_data[1],gyro_data[2], pred_gyro_data[0], pred_gyro_data[1], pred_gyro_data[2]);

    gx = pred_gyro_data[0] / 2 / 4;
    gy = pred_gyro_data[1] / 2 / 4;
    gz = pred_gyro_data[2] / 2 / 4;

    float q0 = quatIn[3]; //w
    float q1 = quatIn[0]; //x
    float q2 = quatIn[1]; //y
    float q3 = quatIn[2]; //z

    qIntegrate(&q0, &q1, &q2, &q3, gx, gy, gz);
    qIntegrate(&q0, &q1, &q2, &q3, gx, gy, gz);
    qIntegrate(&q0, &q1, &q2, &q3, gx, gy, gz);
    qIntegrate(&q0, &q1, &q2, &q3, gx, gy, gz);


    quatOut[3] = q0; //w
    quatOut[0] = q1; //x
    quatOut[1] = q2; //y
    quatOut[2] = q3; //z

    return true;
}

void
L_TrackingToReturnExt(svrHeadPoseState &poseState, const float *gyro_data_s, const float *gyro_data_b,
                   const float *gyro_data_bdt, const float *gyro_data_bdt2,
                   float fw_prediction_delay, const float *rotation, const float *translation, bool virHandGesture) {
    if (gLogRawSensorData) {
        glm::fquat tempQuat;
        tempQuat.x = rotation[0];
        tempQuat.y = rotation[1];
        tempQuat.z = rotation[2];
        tempQuat.w = rotation[3];

        glm::vec3 euler = eulerAngles(tempQuat);

        LOGI("Raw Sensor Position: (%0.2f, %0.2f, %0.2f)", translation[0], translation[1],
             translation[2]);
        LOGI("Raw Sensor Rotation: (%0.2f, %0.2f, %0.2f, %0.2f) => Euler(%0.2f, %0.2f, %0.2f)",
             rotation[0], rotation[1], rotation[2], rotation[3], glm::degrees(euler.x),
             glm::degrees(euler.y), glm::degrees(euler.z));
    }

    // We have decided that if the prediction is 0, we just return the pose data.
    // If they want a predicted time then need to take into account how old the pose
    // is as well as the base IMU pose delay
    if (3 == gPredictVersion && fw_prediction_delay != 0.0f) {
        // How long ago did this pose get written?
        uint64_t timeNow = Svr::GetTimeNano();
        uint64_t poseTime = poseState.poseTimeStampNs - gQTimeToAndroidBoot;
        uint64_t diffTime = timeNow - poseTime;
        float poseAge = ((float) diffTime * NANOSECONDS_TO_MILLISECONDS);
        // LOGE("Current Pose: %0.4f ms ago", poseAge);

        if (gLogRawSensorData) {
            LOGI("    Prediction time adjusted by pose age (%0.4f) and IMU Delay (%0.4f): %0.4f -> %0.4f",
                 poseAge, gIMUSensorPoseDelay, fw_prediction_delay,
                 fw_prediction_delay + poseAge + gIMUSensorPoseDelay);
        }

        fw_prediction_delay += (poseAge + gIMUSensorPoseDelay);
    }

    /*
    * For both 6dof and 3dof, service returns pose in Android Potrait orientation (x up, y left, z towards user)
    */
    if (fw_prediction_delay != 0.0f && gyro_data_s != NULL && gyro_data_b != NULL &&
        gyro_data_bdt != NULL && gyro_data_bdt2 != NULL) {
        bool bPredictionDataError = false;
        for (int i = 0; i < 3; ++i) {
            if (std::isnan(gyro_data_s[i]) ||
                std::isnan(gyro_data_b[i])
                || std::isnan(gyro_data_bdt[i]) ||
                std::isnan(gyro_data_bdt2[i])) {
                bPredictionDataError = true;
                break;
            }
        }
        if (bPredictionDataError) {
            LOGE("TMP_NAN Error start");
            for (int i = 0; i < 3; ++i) {
                if (std::isnan(gyro_data_s[i])) {
                    LOGE("TMP_NAN prediction_coff_s[%d] is NAN", i);
                }
                if (std::isnan(gyro_data_b[i])) {
                    LOGE("TMP_NAN prediction_coff_b[%d] is NAN", i);
                }
                if (std::isnan(gyro_data_bdt[i])) {
                    LOGE("TMP_NAN prediction_coff_bdt[%d] is NAN", i);
                }
                if (std::isnan(gyro_data_bdt2[i])) {
                    LOGE("TMP_NAN prediction_coff_bdt2[%d] is NAN", i);
                }
            }
            LOGE("TMP_NAN Error done");
        }
        if (3 == gPredictVersion && fw_prediction_delay > gMaxPredictedTime) {
            if (gLogMaxPredictedTime) {
                LOGI("    Maximum Prediction time allowed is %0.4f ms. Requested time of %0.4f ms will be clamped!",
                     gMaxPredictedTime, fw_prediction_delay);
            }
            fw_prediction_delay = gMaxPredictedTime;
        }
        if (2 == gPredictVersion) {
            fw_prediction_delay /= 1.25;
        }

        float quatOut[4] = {0.f};
        float gyro_data[3] = {0.f};

        if (gLogRawSensorData) {
            LOGI("    Prediction of %0.4f ms from pose time stamp with Coefficients = (%0.2f, %0.2f, %0.2f, %0.2f)",
                 fw_prediction_delay, *gyro_data_s, *gyro_data_b, *gyro_data_bdt, *gyro_data_bdt2);
        }

        _predict_motion(gyro_data, rotation, quatOut, fw_prediction_delay, gyro_data_s, gyro_data_b,
                        gyro_data_bdt, gyro_data_bdt2);
        poseState.pose.rotation.x = quatOut[0];
        poseState.pose.rotation.y = quatOut[1];
        poseState.pose.rotation.z = quatOut[2];
        poseState.pose.rotation.w = quatOut[3];
    } else {
        poseState.pose.rotation.x = rotation[0];
        poseState.pose.rotation.y = rotation[1];
        poseState.pose.rotation.z = rotation[2];
        poseState.pose.rotation.w = rotation[3];
    }
    if (gLogRawSensorData) {
        glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                            poseState.pose.rotation.y, poseState.pose.rotation.z);
        glm::vec3 euler = eulerAngles(tempQuat);
        LOGI("    Predicted Sensor Quat: (%0.2f, %0.2f, %0.2f, %0.2f) => Euler(%0.2f, %0.2f, %0.2f)",
             poseState.pose.rotation.x, poseState.pose.rotation.y, poseState.pose.rotation.z,
             poseState.pose.rotation.w, glm::degrees(euler.x), glm::degrees(euler.y),
             glm::degrees(euler.z));
    }

    if (!gAppContext->useTransformMatrix && !virHandGesture) {
        if (gSensorOrientationCorrectX != 0.0) {
            glm::quat adjustRotation = glm::angleAxis(glm::radians(gSensorOrientationCorrectX),
                                                      glm::vec3(1.0f, 0.0f, 0.0f));
            // LOGI("Adjusting sensor orientation by (%0.4f, %0.4f, %0.4f, %0.4f", adjustRotation.x, adjustRotation.y, adjustRotation.z, adjustRotation.w);

            glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                poseState.pose.rotation.y, poseState.pose.rotation.z);
            tempQuat *= adjustRotation;
            poseState.pose.rotation.x = tempQuat.x;
            poseState.pose.rotation.y = tempQuat.y;
            poseState.pose.rotation.z = tempQuat.z;
            poseState.pose.rotation.w = tempQuat.w;
        }

        if (gSensorOrientationCorrectY != 0.0) {
            glm::quat adjustRotation = glm::angleAxis(glm::radians(gSensorOrientationCorrectY),
                                                      glm::vec3(0.0f, 1.0f, 0.0f));
            // LOGI("Adjusting sensor orientation by (%0.4f, %0.4f, %0.4f, %0.4f", adjustRotation.x, adjustRotation.y, adjustRotation.z, adjustRotation.w);

            glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                poseState.pose.rotation.y, poseState.pose.rotation.z);
            tempQuat *= adjustRotation;
            poseState.pose.rotation.x = tempQuat.x;
            poseState.pose.rotation.y = tempQuat.y;
            poseState.pose.rotation.z = tempQuat.z;
            poseState.pose.rotation.w = tempQuat.w;
        }

        if (gSensorOrientationCorrectZ != 0.0) {
            glm::quat adjustRotation = glm::angleAxis(glm::radians(gSensorOrientationCorrectZ),
                                                      glm::vec3(0.0f, 0.0f, 1.0f));
            // LOGI("Adjusting sensor orientation by (%0.4f, %0.4f, %0.4f, %0.4f", adjustRotation.x, adjustRotation.y, adjustRotation.z, adjustRotation.w);

            glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                poseState.pose.rotation.y, poseState.pose.rotation.z);
            tempQuat *= adjustRotation;
            poseState.pose.rotation.x = tempQuat.x;
            poseState.pose.rotation.y = tempQuat.y;
            poseState.pose.rotation.z = tempQuat.z;
            poseState.pose.rotation.w = tempQuat.w;
        }
    }

    if (gLogRawSensorData &&
        (gSensorOrientationCorrectX != 0.0 || gSensorOrientationCorrectY != 0.0 ||
         gSensorOrientationCorrectZ != 0.0)) {
        glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                            poseState.pose.rotation.y, poseState.pose.rotation.z);
        glm::vec3 euler = eulerAngles(tempQuat);
        LOGI("    Adjusted for Sensor Orientation: (%0.2f, %0.2f, %0.2f, %0.2f) => Euler(%0.2f, %0.2f, %0.2f)",
             poseState.pose.rotation.x, poseState.pose.rotation.y, poseState.pose.rotation.z,
             poseState.pose.rotation.w, glm::degrees(euler.x), glm::degrees(euler.y),
             glm::degrees(euler.z));
    }


    //convert quat to rotation matrix
    glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                        poseState.pose.rotation.y, poseState.pose.rotation.z);
    glm::mat3 rotation_mat = mat3_cast(tempQuat);
    rotation_mat = glm::transpose(rotation_mat); //converting from col major to row major format
    const float *r = (const float *) glm::value_ptr(rotation_mat);

    //now convert from Android Potrait to Android Landscape orientation

    float input[16] = {0.f};
    input[0] = r[0];
    input[1] = r[1];
    input[2] = r[2];
    input[3] = -translation[0];

    input[4] = r[3];
    input[5] = r[4];
    input[6] = r[5];
    input[7] = -translation[1];

    input[8] = r[6];
    input[9] = r[7];
    input[10] = r[8];
    input[11] = -translation[2];

    input[12] = 0.f;
    input[13] = 0.f;
    input[14] = 0.f;
    input[15] = 1.f;

    // This is confusing!  The matrix represents the CORRECTION to a rotation.
    // So the "Left" matrix is really a "Right" rotation matrix.
    // R(Theta) = | Cos(Theta)  -Sin(Theta) |
    //            | Sin(Theta)   Cos(Theta) |

    float LandRotLeft[16] = {0.0f, 1.0f, 0.0f, 0.0f,
                             -1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 1.0f};

    float LandRotLeftInv[16] = {0.0f, -1.0f, 0.0f, 0.0f,
                                1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f};

    // Z 90 degrees
    float LandRotRight[16] = {0.0f, -1.0f, 0.0f, 0.0f,
                              1.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f};

    float LandRotRightInv[16] = {0.0f, 1.0f, 0.0f, 0.0f,
                                 -1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f};

    glm::mat4 input_glm = glm::make_mat4(input);
    glm::mat4 m = input_glm;

    if (gAppContext->deviceInfo.displayOrientation == 90) {
        // Landscape Left
        //LOGE("Correcting for Landscape Left!");
        glm::mat4 LandRotLeft_glm = glm::make_mat4(LandRotLeft);
        glm::mat4 LandRotLeftInv_glm = glm::make_mat4(LandRotLeftInv);
        m = LandRotLeft_glm * input_glm * LandRotLeftInv_glm;
    } else if (gAppContext->deviceInfo.displayOrientation == 270) {
        // Landscape Right
        //LOGE("Correcting for Landscape Right!");
        glm::mat4 LandRotRight_glm = glm::make_mat4(LandRotRight);
        glm::mat4 LandRotRightInv_glm = glm::make_mat4(LandRotRightInv);
        m = LandRotRight_glm * input_glm * LandRotRightInv_glm;


        // If we are running on Android-M (23) then the sensor rotation correction is different.
        // Android-N (24) is the new rotation.
        if (gAppContext->deviceInfo.deviceOSVersion < 24) {
            // For "M":
            // Need an extra 180 degrees around X
            glm::mat4 t = glm::rotate(glm::mat4(1.0f), (float) M_PI, glm::vec3(1.0f, 0.0f, 0.0f));
            m = m * t;
        } else {
            // For "N":
            // X: Rotated 180 around Final Y
            // Y: Rotated 180 around final Z (Look Blue, right is red, green down, purple up)
            // Z: Correct starting rotation of looking down -Z
            glm::mat4 t = glm::rotate(glm::mat4(1.0f), (float) M_PI, glm::vec3(0.0f, 0.0f, 1.0f));
            m = m * t;
        }
    } else if (gAppContext->deviceInfo.displayOrientation == 0) {
        // Landscape left: sensor input Landscape
        //LOGE("Correcting for Landscape : orientation 0 degree!");
        glm::mat4 LandRotLeft_glm = glm::make_mat4(LandRotLeft);
        glm::mat4 LandRotLeftInv_glm = glm::make_mat4(LandRotLeftInv);
        m = LandRotLeft_glm * input_glm * LandRotLeftInv_glm;
    } else if (gAppContext->deviceInfo.displayOrientation == 180) {
        // Landscape right: sensor input Landscape
        //LOGE("Correcting for Landscape : orientation 180 degree!");
        glm::mat4 LandRotRight_glm = glm::make_mat4(LandRotRight);
        glm::mat4 LandRotRightInv_glm = glm::make_mat4(LandRotRightInv);
        m = LandRotRight_glm * input_glm * LandRotRightInv_glm;

        // Need an extra 180 degrees around Z
        glm::mat4 t = glm::rotate(glm::mat4(1.0f), ((float) M_PI), glm::vec3(0.0f, 0.0f, 1.0f));
        m = m * t;
    }


    const float *p = (const float *) glm::value_ptr(m);

    poseState.pose.position.x = p[3];
    poseState.pose.position.y = p[7];
    poseState.pose.position.z = p[11];

    glm::fquat fromMat = glm::quat_cast(m);
    glm::fquat newQ =
            glm::rotate(glm::quat(1, 0, 0, 0), glm::radians(gHeadRotateX), glm::vec3(1, 0, 0)) *
            fromMat;
    poseState.pose.rotation.x = newQ.x;
    poseState.pose.rotation.y = newQ.y;
    poseState.pose.rotation.z = newQ.z;
    poseState.pose.rotation.w = newQ.w;
    // poseState.pose.rotation.x = fromMat.x;
    // poseState.pose.rotation.y = fromMat.y;
    // poseState.pose.rotation.z = fromMat.z;
    // poseState.pose.rotation.w = fromMat.w;

    poseState.headRotateLeftY = gHeadRotateLeftY;
    poseState.headRotateRightY = gHeadRotateRightY;

    // Need to adjust the position by the offset caused from device rotating around head
    // Only if in 6DOF mode.
    if (!virHandGesture && !gAppContext->useTransformMatrix &&
        (gAppContext->currentTrackingMode & kTrackingPosition)) {
        glm::fquat TempQuat = fromMat * gAppContext->modeContext->recenterRot;
        glm::vec3 baseOffset = glm::vec3(gSensorHeadOffsetX, gSensorHeadOffsetY,
                                         gSensorHeadOffsetZ);
        glm::vec3 rotOffset = baseOffset * TempQuat;
        glm::vec3 diffOffset = baseOffset - rotOffset;

        if (gLogRawSensorData &&
            (gSensorHeadOffsetX != 0.0 || gSensorHeadOffsetY != 0.0 || gSensorHeadOffsetZ != 0.0)) {
            LOGI("    Adjustment to position (%0.3f, %0.3f, %0.3f) based on head offset: (%0.3f, %0.3f, %0.3f)",
                 poseState.pose.position.x, poseState.pose.position.y, poseState.pose.position.z,
                 diffOffset.x, diffOffset.y, diffOffset.z);
        }
        poseState.pose.position.x = poseState.pose.position.x + diffOffset.x;
        poseState.pose.position.y = poseState.pose.position.y + diffOffset.y;
        poseState.pose.position.z = poseState.pose.position.z + diffOffset.z;
    }

    if (gLogRawSensorData) {
        glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                            poseState.pose.rotation.y, poseState.pose.rotation.z);
        glm::vec3 euler = eulerAngles(tempQuat);
        LOGI("    Final Returned Orientation: (%0.2f, %0.2f, %0.2f, %0.2f) => Euler(%0.2f, %0.2f, %0.2f)",
             poseState.pose.rotation.x, poseState.pose.rotation.y, poseState.pose.rotation.z,
             poseState.pose.rotation.w, glm::degrees(euler.x), glm::degrees(euler.y),
             glm::degrees(euler.z));
    }
}

void
L_TrackingToReturn(svrHeadPoseState &poseState, const float *gyro_data_s, const float *gyro_data_b,
                   const float *gyro_data_bdt, const float *gyro_data_bdt2,
                   float fw_prediction_delay, const float *rotation, const float *translation){
    L_TrackingToReturnExt(poseState, gyro_data_s, gyro_data_b, gyro_data_bdt, gyro_data_bdt2,
                          fw_prediction_delay, rotation, translation, false);
}

void
L_TrackingToReturn_for_handshank(svrHeadPoseState &poseState, const float *gyro_data_s, const float *gyro_data_b,
                                 const float *gyro_data_bdt, const float *gyro_data_bdt2,
                                 float fw_prediction_delay, const float *rotation, const float *translation) {

    poseState.pose.rotation.x = rotation[0];
    poseState.pose.rotation.y = rotation[1];
    poseState.pose.rotation.z = rotation[2];
    poseState.pose.rotation.w = rotation[3];

    //convert quat to rotation matrix
    glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                        poseState.pose.rotation.y, poseState.pose.rotation.z);
    glm::mat3 rotation_mat = mat3_cast(tempQuat);
    rotation_mat = glm::transpose(rotation_mat); //converting from col major to row major format
    const float *r = (const float *) glm::value_ptr(rotation_mat);

    //now convert from Android Potrait to Android Landscape orientation

    float input[16] = {0.f};
    input[0] = r[0];
    input[1] = r[1];
    input[2] = r[2];
    input[3] = -translation[0];

    input[4] = r[3];
    input[5] = r[4];
    input[6] = r[5];
    input[7] = -translation[1];

    input[8] = r[6];
    input[9] = r[7];
    input[10] = r[8];
    input[11] = -translation[2];

    input[12] = 0.f;
    input[13] = 0.f;
    input[14] = 0.f;
    input[15] = 1.f;

    float LandRotLeft[16] = {0.0f, 1.0f, 0.0f, 0.0f,
                             -1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 1.0f};

    float LandRotLeftInv[16] = {0.0f, -1.0f, 0.0f, 0.0f,
                                1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f};

    // Z 90 degrees
    float LandRotRight[16] = {0.0f, -1.0f, 0.0f, 0.0f,
                              1.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f};

    float LandRotRightInv[16] = {0.0f, 1.0f, 0.0f, 0.0f,
                                 -1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f};

    glm::mat4 input_glm = glm::make_mat4(input);
    glm::mat4 m = input_glm;

    if (gAppContext->deviceInfo.displayOrientation == 90) {
        // Landscape Left
        //LOGE("Correcting for Landscape Left!");
        glm::mat4 LandRotLeft_glm = glm::make_mat4(LandRotLeft);
        glm::mat4 LandRotLeftInv_glm = glm::make_mat4(LandRotLeftInv);
        m = LandRotLeft_glm * input_glm * LandRotLeftInv_glm;
    } else if (gAppContext->deviceInfo.displayOrientation == 270) {
        // Landscape Right
        //LOGE("Correcting for Landscape Right!");
        glm::mat4 LandRotRight_glm = glm::make_mat4(LandRotRight);
        glm::mat4 LandRotRightInv_glm = glm::make_mat4(LandRotRightInv);
        m = LandRotRight_glm * input_glm * LandRotRightInv_glm;


        // If we are running on Android-M (23) then the sensor rotation correction is different.
        // Android-N (24) is the new rotation.
        if (gAppContext->deviceInfo.deviceOSVersion < 24) {
            // For "M":
            // Need an extra 180 degrees around X
            glm::mat4 t = glm::rotate(glm::mat4(1.0f), (float) M_PI, glm::vec3(1.0f, 0.0f, 0.0f));
            m = m * t;
        } else {
            // For "N":
            // X: Rotated 180 around Final Y
            // Y: Rotated 180 around final Z (Look Blue, right is red, green down, purple up)
            // Z: Correct starting rotation of looking down -Z
            glm::mat4 t = glm::rotate(glm::mat4(1.0f), (float) M_PI, glm::vec3(0.0f, 0.0f, 1.0f));
            m = m * t;
        }
    } else if (gAppContext->deviceInfo.displayOrientation == 0) {
        // Landscape left: sensor input Landscape
        //LOGE("Correcting for Landscape : orientation 0 degree!");
        glm::mat4 LandRotLeft_glm = glm::make_mat4(LandRotLeft);
        glm::mat4 LandRotLeftInv_glm = glm::make_mat4(LandRotLeftInv);
        m = LandRotLeft_glm * input_glm * LandRotLeftInv_glm;
    } else if (gAppContext->deviceInfo.displayOrientation == 180) {
        // Landscape right: sensor input Landscape
        //LOGE("Correcting for Landscape : orientation 180 degree!");
        glm::mat4 LandRotRight_glm = glm::make_mat4(LandRotRight);
        glm::mat4 LandRotRightInv_glm = glm::make_mat4(LandRotRightInv);
        m = LandRotRight_glm * input_glm * LandRotRightInv_glm;

        // Need an extra 180 degrees around Z
        glm::mat4 t = glm::rotate(glm::mat4(1.0f), ((float) M_PI), glm::vec3(0.0f, 0.0f, 1.0f));
        m = m * t;
    }

    const float *p = (const float *) glm::value_ptr(m);

    poseState.pose.position.x = p[3];
    poseState.pose.position.y = p[7];
    poseState.pose.position.z = p[11];

    glm::fquat fromMat = glm::quat_cast(m);
    glm::fquat newQ =
            glm::rotate(glm::quat(1, 0, 0, 0), glm::radians(gHeadRotateX), glm::vec3(1, 0, 0)) *
            fromMat;
    poseState.pose.rotation.x = newQ.x;
    poseState.pose.rotation.y = newQ.y;
    poseState.pose.rotation.z = newQ.z;
    poseState.pose.rotation.w = newQ.w;

    poseState.headRotateLeftY = gHeadRotateLeftY;
    poseState.headRotateRightY = gHeadRotateRightY;
}

void L_CheckPoseQuality(svrHeadPoseState &poseState, qvrservice_head_tracking_data_t *t,
                        bool shouldPostEvents) {
    if (gAppContext == NULL) {
        LOGE("Unable to check pose quality: VR has not been initialized (AppContext)!");
        return;
    }

    if (gAppContext->modeContext == NULL) {
        LOGE("Unable to check pose quality: VR has not been initialized (ModeContext)!");
        return;
    }

    if (gAppContext->modeContext->eventManager == NULL) {
        LOGE("Unable to check pose quality: VR has not been initialized (EventManager)!");
        return;
    }

    svrEventQueue *pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
    if (pQueue == NULL) {
        LOGE("Unable to check pose quality: VR has not been initialized (EventQueue)!");
        return;
    }

    // Will need event data in several cases
    svrEventData eventData;

    if (t == NULL) {
        LOGE("Unable to check pose quality: Head tracking data is NULL!");
        return;
    }

    // First check the "pose_quality"...
    if ((gAppContext->currentTrackingMode & kTrackingPosition) &&
        t->pose_quality < gMinPoseQuality) {
        if (gLogRawSensorData) {
            LOGE("Pose quality is below allowed value: %0.2f < %0.2f", t->pose_quality,
                 gMinPoseQuality);
        }

        poseState.poseStatus &= ~kTrackingPosition;
    }

    // ... then check any of the state flags for 6DOF.
    // Don't need to worry about pulling out tracking from poseStatus because
    // all the flags are supposed to tell us why the status is low
    bool sentPoseStatusEvent = false;
    float spamTime = 1000.0f;  // Time (ms) between posting events about lost pose

    // How much time has elapsed since last time
    uint64_t timeNowNano = Svr::GetTimeNano();

    float diffTimeMs = 0.0f;
    if (gLastBadPoseTime == 0) {
        gLastBadPoseTime = timeNowNano;
    } else {
        uint64_t diffTimeNano = timeNowNano - gLastBadPoseTime;
        diffTimeMs = diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
    }

    // Quick way to force a test of these events
    if (false && diffTimeMs > 5000.0f) {
        t->tracking_state = 0;
        t->tracking_state += QVR_RELOCATION_IN_PROGRESS;

        t->tracking_warning_flags = 0;
        t->tracking_warning_flags += QVR_LOW_FEATURE_COUNT_ERROR;
        t->tracking_warning_flags += QVR_LOW_LIGHT_ERROR;
        t->tracking_warning_flags += QVR_BRIGHT_LIGHT_ERROR;
        t->tracking_warning_flags += QVR_STEREO_CAMERA_CALIBRATION_ERROR;

    }

    // First check tracking state
    if (shouldPostEvents && t->tracking_state != 0) {
        // Tracking state is set, did we already send notification for this state?
        if (gLastTrackingState != t->tracking_state || diffTimeMs > spamTime) {
            // Need to post a message
            if (t->tracking_state & QVR_RELOCATION_IN_PROGRESS) {
                // Don't have any event specific data
                memset(&eventData, 0, sizeof(eventData));
                pQueue->SubmitEvent(kEvent6dofRelocation, eventData);
            }

            gLastTrackingState = t->tracking_state;
            sentPoseStatusEvent = true;
        }
    }

    // Then check tracking flags
    if (shouldPostEvents && t->tracking_warning_flags != 0) {
        // Tracking flags are set, did we already send notification for these flags?
        if (gLastTrackingFlags != t->tracking_warning_flags || diffTimeMs > spamTime) {
            // Need to post a message for each possible warning
            if (t->tracking_warning_flags & QVR_LOW_FEATURE_COUNT_ERROR) {
                // Don't have any event specific data
                memset(&eventData, 0, sizeof(eventData));
                pQueue->SubmitEvent(kEvent6dofWarningFeatureCount, eventData);

                sentPoseStatusEvent = true;
                gLastTrackingFlags = t->tracking_warning_flags;
            }

            if (t->tracking_warning_flags & QVR_LOW_LIGHT_ERROR) {
                // Don't have any event specific data
                memset(&eventData, 0, sizeof(eventData));
                pQueue->SubmitEvent(kEvent6dofWarningLowLight, eventData);

                sentPoseStatusEvent = true;
                gLastTrackingFlags = t->tracking_warning_flags;
            }

            if (t->tracking_warning_flags & QVR_BRIGHT_LIGHT_ERROR) {
                // Don't have any event specific data
                memset(&eventData, 0, sizeof(eventData));
                pQueue->SubmitEvent(kEvent6dofWarningBrightLight, eventData);

                sentPoseStatusEvent = true;
                gLastTrackingFlags = t->tracking_warning_flags;
            }

            if (t->tracking_warning_flags & QVR_STEREO_CAMERA_CALIBRATION_ERROR) {
                // Don't have any event specific data
                memset(&eventData, 0, sizeof(eventData));
                pQueue->SubmitEvent(kEvent6dofWarningCameraCalibration, eventData);

                sentPoseStatusEvent = true;
                gLastTrackingFlags = t->tracking_warning_flags;
            }

        }
    }


    if (sentPoseStatusEvent) {
        // Since we sent an event we need to reset the time (spam prevention)
        gLastBadPoseTime = timeNowNano;
    }

}

//--------------------------------------------------------------------------------------------------------
SvrResult GetTrackingFromPredictiveSensor(float fw_prediction_delay, uint64_t *pSampleTimeStamp,
                                          svrHeadPoseState &poseState){
    return GetTrackingFromPredictiveSensorExt(fw_prediction_delay, pSampleTimeStamp, poseState, false);
}

SvrResult GetTrackingFromPredictiveSensorExt(float fw_prediction_delay, uint64_t *pSampleTimeStamp,
                                          svrHeadPoseState &poseState, bool virHandGesture)
//--------------------------------------------------------------------------------------------------------
{
    if (gAppContext == NULL || ((!gAppContext->mUseIvSlam) && (NULL == gAppContext->qvrHelper))) {
        LOGI("QVRService: Unable to read tracking data.  Service has not been initialized");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    int32_t qvrReturn;
    qvrservice_head_tracking_data_t *t = NULL;

    // Tired of spam that QVR is not working. 
    uint64_t timeNowNano = Svr::GetTimeNano();
    float diffTimeMs = 0.0f;
    float spamTime = 1000.0f;
    if (gLastHeadTrackingFailTime == 0) {
        gLastHeadTrackingFailTime = timeNowNano;
    } else {
        uint64_t diffTimeNano = timeNowNano - gLastHeadTrackingFailTime;
        diffTimeMs = diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
    }
    qvrservice_head_tracking_data_t post_track_t;

    if(gAppContext->mUseIvSlam){
        float ivpose[16] = {0};
        double pose_ts;
        //TODO: Fix the qvrReturn
        //LOGE("GetTrackingFromPredictiveSensor %f",ivpose,fw_prediction_delay);
        IvSlamClient_GetPose(gAppContext->ivslamHelper,ivpose,fw_prediction_delay*1e6 ,pose_ts);
        qvrReturn = QVR_SUCCESS;

        glm::mat4 poseMatrix = glm::make_mat4(ivpose);
        post_track_t.translation[0] = poseMatrix[3][0];
        post_track_t.translation[1] = poseMatrix[3][1];
        post_track_t.translation[2] = poseMatrix[3][2];

        poseMatrix[3][0] = 0;
        poseMatrix[3][1] = 0;
        poseMatrix[3][2] = 0;

        glm::quat outData_rotationQuaternion = glm::quat_cast(poseMatrix);
        post_track_t.rotation[0] = outData_rotationQuaternion.x;
        post_track_t.rotation[1] = outData_rotationQuaternion.y;
        post_track_t.rotation[2] = outData_rotationQuaternion.z;
        post_track_t.rotation[3] = outData_rotationQuaternion.w;

        //TODO:Remove the log
        LOGI("IvSlam rotation: %f %f %f, %f %f %f %f",post_track_t.translation[0],
             post_track_t.translation[1],
             post_track_t.translation[2],
             post_track_t.rotation[0],
             post_track_t.rotation[1],
             post_track_t.rotation[2],
             post_track_t.rotation[3]);

        post_track_t.ts = pose_ts*1e9;
        post_track_t.pose_quality = 0.99;
        post_track_t.sensor_quality = 0.99;
        post_track_t.camera_quality = 0.99;
        t = &post_track_t;
    } else {
        qvrReturn = QVRServiceClient_GetHeadTrackingData(gAppContext->qvrHelper, &t);
    }

    if (qvrReturn != QVR_SUCCESS) {
        // Can't trust the quality at this point
        poseState.poseStatus = 0;
        switch (qvrReturn) {
            case QVR_CALLBACK_NOT_SUPPORTED:
                if (diffTimeMs > spamTime) {
                    LOGE("Error from QVRServiceClient_GetHeadTrackingData: QVR_CALLBACK_NOT_SUPPORTED");
                    gLastHeadTrackingFailTime = timeNowNano;
                }
                return SVR_ERROR_UNSUPPORTED;

            case QVR_API_NOT_SUPPORTED:
                if (diffTimeMs > spamTime) {
                    LOGE("Error from QVRServiceClient_GetHeadTrackingData: QVR_API_NOT_SUPPORTED");
                    gLastHeadTrackingFailTime = timeNowNano;
                }
                return SVR_ERROR_UNSUPPORTED;

            case QVR_INVALID_PARAM:
                if (diffTimeMs > spamTime) {
                    LOGE("Error from QVRServiceClient_GetHeadTrackingData: QVR_INVALID_PARAM");
                    gLastHeadTrackingFailTime = timeNowNano;
                }
                return SVR_ERROR_UNKNOWN;

            default:
                if (diffTimeMs > spamTime) {
                    LOGE("Error from QVRServiceClient_GetHeadTrackingData: Unknown = %d",
                         qvrReturn);
                    gLastHeadTrackingFailTime = timeNowNano;
                }
                return SVR_ERROR_UNKNOWN;
        }
    }

    // Crashed here once.  Safety check that we acutally get something back
    if (t == NULL) {
        LOGE("QVRServiceClient_GetHeadTrackingData() returned NULL!");
        return SVR_ERROR_UNKNOWN;
    }

    // Check the quality of the data that came back.
    // Later: It looks like pose quality only comes in if using 6DOF.
    L_CheckPoseQuality(poseState, t, true);

    // Currently GetSensorTrackingData() doesn't always return -1 on an error.  It does return
    // and invalid rotation of [0,0,0,0].  Therefore we are being defensive and checking the return;
    if (t->rotation[0] == 0.0f && t->rotation[1] == 0.0f && t->rotation[2] == 0.0f &&
        t->rotation[3] == 0.0f) {
        LOGI("QVRService: Invalid tracking (rotation) data");
        return SVR_ERROR_UNKNOWN;
    }

    memcpy(poseState.pose.rawQuaternion, t->rotation, sizeof(float) * 4);
    memcpy(poseState.pose.rawPosition, t->translation, sizeof(float) * 3);
    poseState.pose.rawTS = t->ts;

    *pSampleTimeStamp = t->ts;

    // Save the time stamp from what actually comes back.
    poseState.poseTimeStampNs = t->ts;

    const float *rotation = t->rotation;
    const float *translation = t->translation;
    if (gAppContext->mUseIvSlam) {
        L_TrackingToReturnExt(poseState, NULL, NULL, NULL, NULL, 0.0f, rotation,
                           translation,virHandGesture); // Note NULL and 0.0f since not doing any prediction on historic data :)
    } else{
        L_TrackingToReturnExt(poseState, t->prediction_coff_s, t->prediction_coff_b,
                           t->prediction_coff_bdt, t->prediction_coff_bdt2, fw_prediction_delay,
                           rotation, translation,virHandGesture);
    }


    // Debug Fixed Rotation
    if (gUseFixedRotation) {
        poseState.pose.rotation.x = gFixedRotationQuatX;
        poseState.pose.rotation.y = gFixedRotationQuatY;
        poseState.pose.rotation.z = gFixedRotationQuatZ;
        poseState.pose.rotation.w = gFixedRotationQuatW;

        if (gFixedRotationSpeedX != 0.0f || gFixedRotationSpeedY != 0.0f ||
            gFixedRotationSpeedZ != 0.0f) {
            // How much time has elapsed since last time
            uint64_t timeNowNano = Svr::GetTimeNano();

            float diffTimeMs = 0.0f;
            if (gLastFixedRotationTime != 0) {
                uint64_t diffTimeNano = timeNowNano - gLastFixedRotationTime;
                diffTimeMs = diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
            }
            gLastFixedRotationTime = timeNowNano;

            glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                poseState.pose.rotation.y, poseState.pose.rotation.z);

            // Rotation is radians per second
            gTotalFixedRotationX += diffTimeMs * gFixedRotationSpeedX / 1000.0f;
            gTotalFixedRotationY += diffTimeMs * gFixedRotationSpeedY / 1000.0f;
            gTotalFixedRotationZ += diffTimeMs * gFixedRotationSpeedZ / 1000.0f;

            // LOGE("DEBUG! Total Fixed Rotations (Diff Time = %0.4f ms): (%0.2f, %0.2f, %0.2f)", diffTimeMs, gTotalFixedRotationX, gTotalFixedRotationY, gTotalFixedRotationZ);

            if (gTotalFixedRotationX > 2.0f * M_PI)
                gTotalFixedRotationX -= 2.0f * M_PI;
            if (gTotalFixedRotationY > 2.0f * M_PI)
                gTotalFixedRotationY -= 2.0f * M_PI;
            if (gTotalFixedRotationZ > 2.0f * M_PI)
                gTotalFixedRotationZ -= 2.0f * M_PI;

            if (gFixedRotationSpeedX != 0.0f)
                tempQuat = glm::rotate(tempQuat, gTotalFixedRotationX, glm::vec3(1.0f, 0.0f, 0.0f));

            if (gFixedRotationSpeedY != 0.0f)
                tempQuat = glm::rotate(tempQuat, gTotalFixedRotationY, glm::vec3(0.0f, 1.0f, 0.0f));

            if (gFixedRotationSpeedZ != 0.0f)
                tempQuat = glm::rotate(tempQuat, gTotalFixedRotationZ, glm::vec3(0.0f, 0.0f, 1.0f));

            poseState.pose.rotation.x = tempQuat.x;
            poseState.pose.rotation.y = tempQuat.y;
            poseState.pose.rotation.z = tempQuat.z;
            poseState.pose.rotation.w = tempQuat.w;
        }

        if (gLogRawSensorData) {
            glm::fquat tempQuat(poseState.pose.rotation.w, poseState.pose.rotation.x,
                                poseState.pose.rotation.y, poseState.pose.rotation.z);
            glm::vec3 euler = eulerAngles(tempQuat);
            LOGI("    Config File Fixed Orientation: (%0.2f, %0.2f, %0.2f, %0.2f) => Euler(%0.2f, %0.2f, %0.2f)",
                 poseState.pose.rotation.x, poseState.pose.rotation.y, poseState.pose.rotation.z,
                 poseState.pose.rotation.w, glm::degrees(euler.x), glm::degrees(euler.y),
                 glm::degrees(euler.z));
        }

        // Set status since this is "valid" data
        poseState.poseStatus |= kTrackingRotation;
    }

    // Debug Fixed Position
    if (gUseFixedPosition) {
        // This is for Landscape right and all the positions are negated
        poseState.pose.position.x = -gFixedPositionX;
        poseState.pose.position.y = -gFixedPositionY;
        poseState.pose.position.z = -gFixedPositionZ;

        if (gFixedPositionSpeed != 0.0) {
            // Need to travel from A to B
            glm::vec3 direction =
                    glm::vec3(gFixedPositionEndX, gFixedPositionEndY, gFixedPositionEndZ) -
                    glm::vec3(gFixedPositionX, gFixedPositionY, gFixedPositionZ);

            // What is the maximum that can be traveled?
            float maxMovement = glm::length(direction);

            // Is glm smart enough to normalize a zero vector?
            if (maxMovement != 0.0f)
                direction = glm::normalize(direction);

            // How much time has elapsed since last time
            uint64_t timeNowNano = Svr::GetTimeNano();

            float diffTimeMs = 0.0f;
            if (gLastFixedPositionTime != 0) {
                uint64_t diffTimeNano = timeNowNano - gLastFixedPositionTime;
                diffTimeMs = diffTimeNano * NANOSECONDS_TO_MILLISECONDS;
            } else {
                // First time through here, so set the current position to be the start
                gCurrentFixedPositionX = gFixedPositionX;
                gCurrentFixedPositionY = gFixedPositionY;
                gCurrentFixedPositionZ = gFixedPositionZ;
            }
            gLastFixedPositionTime = timeNowNano;

            // Move along the direction vector based on speed
            float msToSeconds = 0.001f;
            direction *= (gFixedPositionSpeed * diffTimeMs * msToSeconds);
            gCurrentFixedPositionX += direction.x;
            gCurrentFixedPositionY += direction.y;
            gCurrentFixedPositionZ += direction.z;

            // Have we reached the end?
            glm::vec3 totalMovement = glm::vec3(gCurrentFixedPositionX, gCurrentFixedPositionY,
                                                gCurrentFixedPositionZ) -
                                      glm::vec3(gFixedPositionX, gFixedPositionY, gFixedPositionZ);
            float totalDistance = glm::length(totalMovement);
            if (gFixedPositionSpeed > 0.0f && totalDistance > maxMovement) {
                // Since at the end, reset to beginning.
                // Remember, if going backwards then will never hit the end
                gCurrentFixedPositionX = gFixedPositionX;
                gCurrentFixedPositionY = gFixedPositionY;
                gCurrentFixedPositionZ = gFixedPositionZ;
            }

            // Set the return value.  This is negated for the same reason the non-speed one is set
            poseState.pose.position.x = -gCurrentFixedPositionX;
            poseState.pose.position.y = -gCurrentFixedPositionY;
            poseState.pose.position.z = -gCurrentFixedPositionZ;
        }   // Fixed position has a speed

        if (gLogRawSensorData) {
            LOGI("    Config File Fixed Position: (%0.3f, %0.3f, %0.3f)",
                 -poseState.pose.position.x, -poseState.pose.position.y,
                 -poseState.pose.position.z);
        }

        // Set status since this is "valid" data
        poseState.poseStatus |= kTrackingPosition;
    }

    // If using mag data but it has not been calibrated, send a message
    static uint64_t lastMagEventTime = 0;
    if (t->flags_3dof_mag_used && !t->flags_3dof_mag_calibrated) {
        // Don't want to spam, so only send the message every so often.
        uint64_t timeNow = Svr::GetTimeNano();

        if (((timeNow - lastMagEventTime) * NANOSECONDS_TO_MILLISECONDS) > 5000.0f) {
            // It has been enough time since we sent an event
            LOGI("Sending magnometer uncalibrated event...");
            lastMagEventTime = timeNow;

            // Only send the event if we can get the event manager
            if (gAppContext != NULL && gAppContext->modeContext != NULL &&
                gAppContext->modeContext->eventManager != NULL) {
                svrEventQueue *pQueue = gAppContext->modeContext->eventManager->AcquireEventQueue();
                if (pQueue != NULL) {
                    // Don't have any event specific data
                    svrEventData eventData;
                    memset(&eventData, 0, sizeof(eventData));

                    pQueue->SubmitEvent(kEventMagnometerUncalibrated, eventData);
                }   // Event queue is NOT NULL
                else {
                    LOGE("Unable to send magnometer uncalibrated event: Event queue could not be acquired");
                }

            }   // gAppContext and members are NOT NULL
            else {
                LOGE("Unable to send magnometer uncalibrated event: Application context has not been created");
            }
        }   // Enough time has elapsed
    }   // Mag uncalibrated flag is set


    return SVR_ERROR_NONE;
}

glm::mat3 rodrigues(glm::vec3 &rotation) {
    auto axis = glm::normalize(rotation);
    float theta = glm::length(rotation);
    float cosTheta = cos(theta);
    float sinTheta = sin(theta);
    glm::mat3 axisMat{0, axis.z, -axis.y, -axis.z, 0, axis.x, axis.y, -axis.x, 0};
    glm::mat3 outerMat = glm::outerProduct(axis, axis);
    return glm::mat3(1) * cosTheta + (1 - cosTheta) * outerMat + sinTheta * axisMat;
}

void printGlmMat4(const char *matName, glm::mat4 &mat4) {
    LOGI("%s = [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]", matName,
         mat4[0][0], mat4[1][0], mat4[2][0], mat4[3][0],
         mat4[0][1], mat4[1][1], mat4[2][1], mat4[3][1],
         mat4[0][2], mat4[1][2], mat4[2][2], mat4[3][2],
         mat4[0][3], mat4[1][3], mat4[2][3], mat4[3][3]);
}

void printGlmMat3(const char *matName, glm::mat3 &mat3) {
    LOGI("%s = [%f, %f, %f; %f, %f, %f; %f, %f, %f]", matName,
         mat3[0][0], mat3[1][0], mat3[2][0],
         mat3[0][1], mat3[1][1], mat3[2][1],
         mat3[0][2], mat3[1][2], mat3[2][2]);
}

//--------------------------------------------------------------------------------------------------------
SvrResult GetTrackingFromHistoricSensor(uint64_t timestampNs, svrHeadPoseState &poseState, bool bLoadCalibration)
//--------------------------------------------------------------------------------------------------------
{
    if (gAppContext == NULL) {
        LOGE("svrGetHistoricHeadPose() failed: VR has not been initialized!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    if (gAppContext->qvrHelper == NULL) {
        LOGE("svrGetHistoricHeadPose() not supported on this device!");
        return SVR_ERROR_VRMODE_NOT_INITIALIZED;
    }

    int32_t qvrReturn;
    qvrservice_head_tracking_data_t *t = NULL;
    qvrReturn = QVRServiceClient_GetHistoricalHeadTrackingData(gAppContext->qvrHelper, &t,
                                                               timestampNs);
    if (qvrReturn != QVR_SUCCESS) {
        switch (qvrReturn) {
            case QVR_CALLBACK_NOT_SUPPORTED:
                LOGE("Error from QVRServiceClient_GetHistoricalHeadTrackingData: QVR_CALLBACK_NOT_SUPPORTED");
                return SVR_ERROR_UNSUPPORTED;
                break;

            case QVR_API_NOT_SUPPORTED:
                LOGE("Error from QVRServiceClient_GetHistoricalHeadTrackingData: QVR_API_NOT_SUPPORTED");
                return SVR_ERROR_UNSUPPORTED;
                break;

            case QVR_INVALID_PARAM:
                LOGE("Error from QVRServiceClient_GetHistoricalHeadTrackingDatas: QVR_INVALID_PARAM");
                return SVR_ERROR_UNKNOWN;
                break;

            default:
                LOGE("Error from QVRServiceClient_GetHistoricalHeadTrackingData: Unknown = %d",
                     qvrReturn);
                break;
        }

        return SVR_ERROR_UNKNOWN;
    }

    // LOGE("T = 0x%x", (unsigned int)t);

    // Crashed here once.  Safety check that we acutally get something back
    if (t == NULL) {
        LOGE("QVRServiceClient_GetHistoricalHeadTrackingData() returned NULL!");
        return SVR_ERROR_UNKNOWN;
    }

    // Currently GetSensorTrackingData() doesn't always return -1 on an error.  It does return
    // and invalid rotation of [0,0,0,0].  Therefore we are being defensive and checking the return;
    if (t->rotation[0] == 0.0f && t->rotation[1] == 0.0f && t->rotation[2] == 0.0f &&
        t->rotation[3] == 0.0f) {
        LOGI("QVRService: Invalid tracking (rotation) data");
        return SVR_ERROR_UNKNOWN;
    }

    // Save the time stamp from what actually comes back.
    poseState.poseTimeStampNs = t->ts;

    if (bLoadCalibration) {
        if (!gHasLoadCalibration) {
            LOGI("GetTrackingFromHistoricSensor start load calibration");
            std::string persistPath = "/persist/qvr/";
            std::string calibrationPath;
            std::string xmlSuffix = ".xml";
            DIR *persistDir;
            struct dirent *ent;
            if ((persistDir = opendir (persistPath.c_str())) != NULL) {
                while ((ent = readdir (persistDir)) != NULL) {
                    std::string currFilePath{ent->d_name};
                    if (currFilePath.size() <= xmlSuffix.size()) {
                        continue;
                    } else if (std::equal(xmlSuffix.rbegin(), xmlSuffix.rend(),
                                          currFilePath.rbegin())) {
                        calibrationPath = persistPath + currFilePath;
                        break;
                    }
                }
                closedir (persistDir);
            } else {
                LOGE("Failed open folder %s", persistPath.c_str());
            }
            struct stat fileBuffer;
            if (0 == stat(calibrationPath.c_str(), &fileBuffer)) {
                LOGI("GetTrackingFromHistoricSensor file exist start parse");
                std::ifstream ifs(calibrationPath);
                std::string content((std::istreambuf_iterator<char>(ifs)),
                                    (std::istreambuf_iterator<char>()));
                std::vector<char> dataVector(content.begin(), content.end());
                xml_document<> doc;
                doc.parse<0>(&dataVector[0]);
                xml_node<>* node = doc.first_node("DeviceConfiguration")->first_node("SFConfig")->first_node("Stateinit");
                std::string strOMBC = node->first_attribute("ombc")->value();
                std::string strTBC = node->first_attribute("tbc")->value();
                LOGI("GetTrackingFromHistoricSensor strOMBC=%s, strTBC=%s", strOMBC.c_str(), strTBC.c_str());
                std::stringstream ssOMBC{strOMBC};
                std::vector<float> ombcVector{ std::istream_iterator<float>(ssOMBC) , std::istream_iterator<float>() };
                glm::vec3 ombc{ombcVector[0], ombcVector[1], ombcVector[2]};
                glm::mat3 ombcMat = rodrigues(ombc);
                printGlmMat3("GetTrackingFromHistoricSensor ombcMat", ombcMat);
                std::stringstream ssTBC{strTBC};
                std::vector<float> tbcVector{ std::istream_iterator<float>(ssTBC) , std::istream_iterator<float>() };
                glm::vec3 tbc{tbcVector[0], tbcVector[1], tbcVector[2]};
                glm::mat4 transformM = glm::mat4(ombcMat);
                transformM[3] = glm::vec4{tbc, 1.0f};
                printGlmMat4("GetTrackingFromHistoricSensor transformM", transformM);
                glm::mat4 negativeM = glm::mat4(1);
                negativeM[0][0] = 0;
                negativeM[1][1] = 0;
                negativeM[0][1] = -1;
                negativeM[1][0] = -1;
                negativeM[2][2] = -1;
                gCalibrationM = transformM * negativeM;
                printGlmMat4("GetTrackingFromHistoricSensor done gCalibrationM", gCalibrationM);
                ifs.close();
            } else {
                LOGI("GetTrackingFromHistoricSensor Failed loading %s", calibrationPath.c_str());
            }
            gHasLoadCalibration = true;
        }
        glm::quat oriQ{t->rotation[3], t->rotation[0], t->rotation[1], t->rotation[2]};
        glm::vec3 oriT{t->translation[0], t->translation[1], t->translation[2]};
        glm::mat4 oriM = glm::mat4_cast(oriQ);
        oriM[3] = glm::vec4{oriT, 1.0f};
        glm::mat4 newM = oriM * gCalibrationM;
        glm::quat newQ = glm::quat_cast(newM);
        glm::vec4 newT = newM[3];
        t->rotation[0] = newQ.x;
        t->rotation[1] = newQ.y;
        t->rotation[2] = newQ.z;
        t->rotation[3] = newQ.w;
        t->translation[0] = newT.x;
        t->translation[1] = newT.y;
        t->translation[2] = newT.z;
    }

    const float *rotation = t->rotation;
    const float *translation = t->translation;
    L_TrackingToReturn(poseState, NULL, NULL, NULL, NULL, 0.0f, rotation,
                       translation); // Note NULL and 0.0f since not doing any prediction on historic data :)

    // Check the pose quality
    L_CheckPoseQuality(poseState, t, false);

    return SVR_ERROR_NONE;
}

