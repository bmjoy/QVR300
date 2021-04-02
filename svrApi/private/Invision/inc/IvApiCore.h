//
// Created by zengchuiguo on 5/21/2020.
//

#ifndef SVRAPI_IVAPICORE_H
#define SVRAPI_IVAPICORE_H

#include "svrApi.h"

//TODO: implement API to read slam version
#define IVSLAM_VER  "1.0"

#if 0
SvrResult invisionBeginVr(const svrBeginParams *pBeginParams);
SvrResult invisionEndVr();
SVRP_EXPORT SvrResult invisionSetTrackingMode(uint32_t trackingModes);
#endif
int dump_buffer_to_txt(const char* file_name,const char* data, const int len);
int dump_float_buffer_to_txt(const char* file_name, const uint64_t index, const float* data, const int total_len);
int invisionGetParam(char* pBuffer, uint32_t* pLen);

#endif //SVRAPI_IVAPICORE_H
