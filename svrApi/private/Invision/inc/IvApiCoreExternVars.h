//
// Created by zengchuiguo on 5/21/2020.
//

#ifndef SVRAPI_IVPAI_CORE_EXTERNVARS_H
#define SVRAPI_IVPAI_CORE_EXTERNVARS_H

#include <fstream>
#include <string>
#include "svrConfig.h"

EXTERN_VAR(std::ifstream, gGpuBusyFStream);
//EXTERN_VAR(const std::string, QCOM_GPU_BUSY_FILE_PATH);
EXTERN_VAR(float, gGpuBusyCheckPeriod);
EXTERN_VAR(uint64_t, gVrStartTimeNano);
EXTERN_VAR(bool, gEnableMotionVectors);
EXTERN_VAR(bool, gForceAppEnableMotionVectors);
EXTERN_VAR(bool, gHeuristicPredictedTime);
EXTERN_VAR(bool, gUseLinePtr);
EXTERN_VAR(bool, gEnableTimeWarp);
EXTERN_VAR(bool, gEnableDebugServer);
EXTERN_VAR(bool, gUseQvrPerfModule);
EXTERN_VAR(bool, gEnableRenderThreadFifo);
EXTERN_VAR(bool, gUseMagneticRotationFlag);
EXTERN_VAR(int, gNumHeuristicEntries);
EXTERN_VAR(int, gHeuristicWriteIndx);
EXTERN_VAR(int, gRenderThreadCore);
EXTERN_VAR(int, gNormalPriorityRender);
EXTERN_VAR(int, gForceTrackingMode);
EXTERN_VAR(float*, gpHeuristicPredictData);


#endif //SVRAPI_IVPAI_CORE_EXTERNVARS_H
