//=============================================================================
// FILE: svrApiPredictiveSensor.h
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#ifndef _SVR_API_PREDICTIVE_SENSOR_H_
#define _SVR_API_PREDICTIVE_SENSOR_H_

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "private/svrApiCore.h"

SvrResult GetTrackingFromPredictiveSensor(float fw_prediction_delay, uint64_t *pSampleTimeStamp,
                                          svrHeadPoseState &poseState);
SvrResult GetTrackingFromPredictiveSensorExt(float fw_prediction_delay, uint64_t *pSampleTimeStamp,
                                          svrHeadPoseState &poseState, bool virHandGesture);

SvrResult GetTrackingFromHistoricSensor(uint64_t timestampNs, svrHeadPoseState &poseState, bool bLoadCalibration=false);

// add by chenweihua 20201026 (start)
void L_TrackingToReturn_for_handshank(svrHeadPoseState &poseState, const float *gyro_data_s, const float *gyro_data_b,
                        const float *gyro_data_bdt, const float *gyro_data_bdt2,
                        float fw_prediction_delay, const float *rotation, const float *translation);
// add by chenweihua 20201026 (end)

#endif //_SVR_API_PREDICTIVE_SENSOR_H_
