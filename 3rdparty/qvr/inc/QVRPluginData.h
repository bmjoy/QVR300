/******************************************************************************/
/*! \file  QVRPluginData.h */
/*
* Copyright (c) 2018 Qualcomm Technologies, Inc.
* All Rights Reserved
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/
#ifndef QVRPLUGIN_DATA_H
#define QVRPLUGIN_DATA_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include "QVRTypes.h"


/**************************************************************************//**
* APIs to communicate with QVRPluginData. Typical call flow is as follows:
*   1. Create QVRServiceClient object
*   2. Call QVRServiceClient_GetPluginDataHandle(pluginName) to get a
*      QVRPluginData handle
*   3. Call QVRPluginData_GetPluginInfo() to get information on the
*      plugin vendor/version and its data version, which can be used to
*      determine data format, protocol, etc., of the plugin data.
*   4. Call QVRPluginData_GetData()/QVRPluginData_SetData()
*   5. Call QVRServiceClient_ReleasePluginDataHandle() to release the
*      QVRPluginData handle.
******************************************************************************/

/**************************************************************************//**
* QVRPLUGINDATA_API_VERSION
* -----------------------------------------------------------------------------
* Defines the API versions of this interface. The api_version member of the
* qvrplugin_data_t structure must be set to accurately reflect the version
* of the API that the implementation supports.
******************************************************************************/
typedef enum {
    QVRPLUGINDATA_API_VERSION_1 = 1,
} QVRPLUGINDATA_API_VERSION;

typedef void* qvrplugin_data_handle_t;


typedef struct qvrplugin_data_ops {

    /**********************************************************************//**
    * GetPluginDataInfo()
    * -------------------------------------------------------------------------
    * \param
    *    handle:      [in] qvrplugin_data_handle_t returned by
    *                      QVRServiceClient_GetPluginData().
    *    pInfo:      [out] Pointer to a qvr_plugin_info_t that will be filled
    *                      in with information about the plugin data. Must not
    *                      be NULL.
    * \return
    *    Returns QVR_SUCCESS upon success,
    *            QVR_API_NOT_SUPPORTED if not supported
    *            QVR_INVALID_PARAM if payload parameters are
    *                              not following the non-NULL requirement
    *            QVR_ERROR for other error
    * API level
    *    1 or higher
    * Timing requirements
    *    None
    * Notes
    *    None
    **************************************************************************/
    int32_t (*GetPluginDataInfo)(qvrplugin_data_handle_t handle,
        qvr_plugin_info_t* pInfo);

    /**********************************************************************//**
    * GetData()
    * -------------------------------------------------------------------------
    * This function can be used to query the plugin for some data, e.g. reading
    * a user profile.
    *
    * \param
    *    handle:         [in] qvrplugin_data_handle_t returned by
    *                         QVRServiceClient_GetPluginData().
    *    pControl:       [in] Byte array (of length controlLen)
    *                         allowing the client to convey control information
    *                         associated with the data payload. This is an
    *                         optional array and thus pControl may be NULL.
    *    controlLen:     [in] controlLen is an integer that represents the
    *                         length of the byte array pointed to by pControl.
    *                         controlLen may be zero when the control array is
    *                         not necessary.
    *    pPayload:      [out] Byte array (of length payloadLen) for the data to
    *                         be received. May be NULL on the first call when
    *                         querying the length.
    *    pPayloadLen: [inout] If pPayload is NULL, pPayloadLen will be filled
    *                         in with the number of bytes required to hold the
    *                         value of the parameter specified by pPayload. If
    *                         pPayload is non-NULL, pPayloadLen must point to an
    *                         integer that represents the length of the buffer
    *                         pointed to by pPayload. pPayloadLen must not be
    *                         NULL.
    * \return
    *    Returns QVR_SUCCESS upon success,
    *            QVR_API_NOT_SUPPORTED if not supported
    *            QVR_INVALID_PARAM if payload parameters are
    *                              not following the non-NULL requirement
    *            QVR_BUSY if the call cannot be completed at this time, e.g.
    *                     due to concurrency issues.
    *            QVR_ERROR for other error
    * API level
    *    1 or higher
    * Timing requirements
    *    None
    * Notes
    *    The pPayload array will be filled in up to *pPayloadLen bytes
    *    so this may result in truncation of the payload if the required length
    *    is larger than the size passed in pPayloadLen.
    **************************************************************************/
    int32_t (*GetData)(qvrplugin_data_handle_t handle,
        const char* pControl, uint32_t controlLen,
        char* pPayload, uint32_t* pPayloadLen);

    /**********************************************************************//**
    * SetData()
    * -------------------------------------------------------------------------
    * This function can be used to write data to the plugin, e.g. setting a
    * user profile.
    *
    * \param
    *    handle:     [in] qvrplugin_data_handle_t returned by
    *                     QVRServiceClient_GetPluginData().
    *    pControl:   [in] Byte array (of length controlLen) allowing the client
    *                     to convey control information associated with the data
    *                     payload. This is an optional array and thus pControl
    *                     may be NULL
    *    controlLen: [in] controlLen is an integer that represents the length of
    *                     the byte array pointed to by pControl. controlLen is
    *                     ignored if pControl is NULL.
    *    pPayload:   [in] Byte array (of length payloadLen) representing the
    *                     data to be configured. Must not be NULL.
    *    payloadLen: [in] payloadLen is an integer that represents the length of
    *                     the byte array pointed to by pPayload. Must not be
    *                     zero.
    * \return
    *    Returns QVR_SUCCESS upon success,
    *            QVR_API_NOT_SUPPORTED if not supported
    *            QVR_INVALID_PARAM if payload parameters are
    *                              not following the non-NULL requirement
    *            QVR_BUSY if the call cannot be completed at this time, e.g.
    *                     due to concurrency issues.
    *            QVR_ERROR for other error
    * API level
    *    1 or higher
    * Timing requirements
    *    None
    * Notes
    *    None
    **************************************************************************/
    int32_t (*SetData)(qvrplugin_data_handle_t handle,
        const char* pControl, uint32_t controlLen,
        const char* pPayload, uint32_t payloadLen);

    //Reserved for future use
    void* reserved[64 - 0];

} qvrplugin_data_ops_t;

typedef struct qvrplugin_data {
    int api_version;
    qvrplugin_data_handle_t handle;
    qvrplugin_data_ops_t* ops;
} qvrplugin_data_t;

static inline int32_t QVRPluginData_GetPluginDataInfo(qvrplugin_data_t* me,
    qvr_plugin_info_t* pluginInfo)
{
    if(!me) return QVR_INVALID_PARAM;
    if(!me->ops->GetPluginDataInfo) return QVR_API_NOT_SUPPORTED;
    return me->ops->GetPluginDataInfo(me->handle, pluginInfo);
}

static inline int32_t QVRPluginData_GetData(qvrplugin_data_t* me,
    const char* pControl, uint32_t controlLen,
    char* pPayload, uint32_t* pPayloadLen)
{
    if(!me) return QVR_INVALID_PARAM;
    if(!me->ops->GetData) return QVR_API_NOT_SUPPORTED;
    return me->ops->GetData(
        me->handle, pControl, controlLen, pPayload, pPayloadLen);
}

static inline int32_t QVRPluginData_SetData(qvrplugin_data_t* me,
    const char* pControl, uint32_t controlLen,
    const char* pPayload, uint32_t payloadLen)
{
    if(!me) return QVR_INVALID_PARAM;
    if(!me->ops->SetData) return QVR_API_NOT_SUPPORTED;
    return me->ops->SetData(
        me->handle, pControl, controlLen, pPayload, payloadLen);
}

#ifdef __cplusplus
}
#endif

#endif /* QVRPLUGIN_DATA_H */
