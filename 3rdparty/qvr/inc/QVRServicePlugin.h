/******************************************************************************/
/*! \file  QVRServicePlugin.h  */
/*
* Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
* All Rights Reserved
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/

// Draft version: 0.2, subject to change
//

#ifndef QVRSERVICE_PLUGIN_H
#define QVRSERVICE_PLUGIN_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include "QVRTypes.h"

/**************************************************************************//**
* QVRSERVICE_PLUGIN_API_VERSION
* -----------------------------------------------------------------------------
* Defines the API versions of this interface. The api_version member of the
* qvr_plugin_t object retrieved from the plugin library must
* be set to the API version that matches its functionality.
******************************************************************************/
typedef enum {
    QVRSERVICE_PLUGIN_API_VERSION_INVALID = 0,
    QVRSERVICE_PLUGIN_API_VERSION_1 = 1,
    QVRSERVICE_PLUGIN_API_VERSION_2 = 2,
} QVRSERVICE_PLUGIN_API_VERSION;

/**************************************************************************//**
* QVRSERVICE_PLUGIN_ID
* -----------------------------------------------------------------------------
* Defines the ID of the plugin.
******************************************************************************/
typedef enum {
    QVRSERVICE_PLUGIN_ID_NONE,
    QVRSERVICE_PLUGIN_ID_EYE_TRACKING = 1,
} QVRSERVICE_PLUGIN_ID;

/**************************************************************************//**
* QVRSERVICE_PLUGIN_CONFIG_PATH
* -----------------------------------------------------------------------------
* Description
*   Can be used with SetParam() to set the configuration path for the device.
*   The path contains read only files pertinent to plugin configuration.
******************************************************************************/
#define QVRSERVICE_PLUGIN_CONFIG_PATH "qvr-plugin-config-path"

/**************************************************************************//**
* QVRSERVICE_PLUGIN_DATA_PATH
* -----------------------------------------------------------------------------
* Description
*   Can be used with SetParam() to set the data path for the device.
*   The path can be used to write any data by the plugin.
******************************************************************************/
#define QVRSERVICE_PLUGIN_DATA_PATH "qvr-plugin-data-path"

/**************************************************************************//**
* QVRSERVICE_PLUGIN_CALIBRATION_PATH
* -----------------------------------------------------------------------------
* Description
*   Can be used with SetParam() to set the calibration path for the device.
*   The path contains read only files pertinent to device-specific calibration
*   (i.e. eye camera calibration).
******************************************************************************/
#define QVRSERVICE_PLUGIN_CALIBRATION_PATH "qvr-plugin-calibration-path"

/**************************************************************************//**
* QVRSERVICE_PLUGIN_VENDOR_STRING
* -----------------------------------------------------------------------------
* Description
*   Can be used with GetParam() to get vendor string from the plugin.
******************************************************************************/
#define QVRSERVICE_PLUGIN_VENDOR_STRING "qvr-plugin-vendor-string"

/**************************************************************************//**
* QVRSERVICE_PLUGIN_VERSION
* -----------------------------------------------------------------------------
* Description
*   Can be used with GetParam() to get the plugin version.
******************************************************************************/
#define QVRSERVICE_PLUGIN_VERSION "qvr-plugin-version"

/**************************************************************************//**
* qvr_plugin_param_t
* -----------------------------------------------------------------------------
* Description
*   Used with Init() to set parameters.
******************************************************************************/
typedef struct _qvr_plugin_param {
    const char* name;
    const char* val;
} qvr_plugin_param_t;

/**************************************************************************//**
* QVRSERVICE_PLUGIN_ERROR_STATE
* -----------------------------------------------------------------------------
*   QVRSERVICE_PLUGIN_ERROR_STATE_RECOVERING
*      Indicates the plugin is trying to recover from an error
*   QVRSERVICE_PLUGIN_ERROR_STATE_RECOVERED
*      Indicates the plugin hass successfully recovered from an error
*   QVRSERVICE_PLUGIN_ERROR_STATE_UNRECOVERABLE
*      Indicates the plugin is unable to recover from the error.
******************************************************************************/
typedef enum {
    QVRSERVICE_PLUGIN_ERROR_STATE_RECOVERING = 0,
    QVRSERVICE_PLUGIN_ERROR_STATE_RECOVERED,
    QVRSERVICE_PLUGIN_ERROR_STATE_UNRECOVERABLE
} QVRSERVICE_PLUGIN_ERROR_STATE;

typedef struct qvr_plugin_callbacks_ops {

    /**************************************************************************//**
    * NotifyError()
    * -----------------------------------------------------------------------------
    * Used by the plugin to notify the VR service that an error has occurred.
    *
    * \param
    *    pCtx:          [in] The context passed to Create().
    *    error_state:   [in] QVRSERVICE_PLUGIN_ERROR_STATE
    *    dwParam:       [in] Error-specific parameter (see Notes)
    * \return
    *    None
    * API level
    *    2 or higher
    * Notes
    *   1. If error_state is QVRSERVICE_PLUGIN_ERROR_STATE_RECOVERING, then
    *      dwParam indicates the expected time to recovery in seconds.
    *   2. If error_state is , QVRSERVICE_PLUGIN_ERROR_STATE_UNRECOVERABLE, then
    *      the plugin may be destroyed and re-created, but not until after
    *      the call to NotifyError() has returned.
    ******************************************************************************/
    void (*NotifyError)(void *pCtx, QVRSERVICE_PLUGIN_ERROR_STATE error_state,
        uint64_t dwParam);

    //Reserved for future use
    void* reserved[64 - 0];

} qvr_plugin_callbacks_ops_t;

typedef struct qvr_plugin_callbacks {
    QVRSERVICE_PLUGIN_API_VERSION api_version;
    void *ctx;
    qvr_plugin_callbacks_ops_t* ops;
} qvr_plugin_callbacks_t;


typedef struct qvr_plugin_ops {

    /**********************************************************************//**
    * Init()
    * -------------------------------------------------------------------------
    * Initializes the plugin for normal operation.
    *
    * \param
    *    pKey         [in] NULL terminated string for privilege checking. This
    *                      key may be used to create privileged clients.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Notes
    *    Memory pointed to by the pKey parameter is only valid for the duration
    *    of the call to Init(), so the plugin should make a copy of pKey if it
    *    needs to retain it.
    * Resource requirements
    *    After this function is called, the plugin must conform to the following
    *    contraints:
    *      - Memory: no contraints on the plugin
    *      - Power: the plugin must remain in a low power state
    **************************************************************************/
    int32_t (*Init)(const char* key);

    /**********************************************************************//**
    * Deinit()
    * -------------------------------------------------------------------------
    * Deinitializes the plugin. The plugin should relinquish any resources
    * obtained in Init().
    *
    * \param
    *    None
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Notes
    *    None
    * Resource requirements
    *    After this function is called, the plugin must conform to the following
    *    contraints:
    *      - Memory: the plugin must minimize its memory usage
    *      - Power: the plugin must remain in a low power state
    **************************************************************************/
    int32_t (*Deinit)(void);

    /**********************************************************************//**
    * Start()
    * -------------------------------------------------------------------------
    * Starts the plugin's normal operation.
    * This function will be called when an application calls StartVRMode() on
    * the VR service.
    *
    * \param
    *    None
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Notes
    *    None
    * Resource requirements
    *    After this function is called, the plugin must conform to the following
    *    contraints:
    *      - Memory: no contraints on the plugin
    *      - Power: no contraints on the plugin
    **************************************************************************/
    int32_t (*Start)(void);

    /**********************************************************************//**
    * Stop()
    * -------------------------------------------------------------------------
    * This function will be called when an application calls StopVRMode() on
    * the VR service.
    *
    * \param
    *    None
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Notes
    *    None
    * Resource requirements
    *    After this function is called, the plugin must conform to the following
    *    contraints:
    *      - Memory: no contraints on the plugin
    *      - Power: the plugin must remain in a low power state
    **************************************************************************/
    int32_t (*Stop)(void);

    /**********************************************************************//**
    * GetParam()
    * -------------------------------------------------------------------------
    * Gets a specific parameter value from the plugin.
    *
    * \param
    *    pName:    [in] NUL-terminated name of the parameter length/value to
    *                   retrieve. Must not be NULL.
    *    pLen:  [inout] If pValue is NULL, pLen will be filled in with the
    *                   number of bytes (including the NUL terminator) required
    *                   to hold the value of the parameter specified by pName.
    *                   If pValue is non-NULL, pLen must point to an integer
    *                   that represents the length of the buffer pointed to by
    *                   pValue. pLen must not be NULL.
    *    pValue:   [in] Buffer to receive value.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Notes
    *    The pValue buffer will be filled in up to *pLen bytes (including NUL),
    *    so this may result in truncation of the value if the required length
    *    is larger than the size passed in pLen.
    **************************************************************************/
    int32_t (*GetParam)(const char* pName, uint32_t* pLen, char* pValue);

    /**********************************************************************//**
    * SetParam()
    * -------------------------------------------------------------------------
    * Sets a specific parameter value on the plugin.
    *
    * \param
    *    pName:    [in] NUL-terminated name of parameter value to set. Must not
    *                   be NULL.
    *    pValue:   [in] NUL-terminated value. Must not be NULL.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Notes
    *    Memory pointed to by the pTransform parameter is only valid for the
    *    duration of the call to SetTransform(), so the plugin should make a
    *    copy of pTransform if it needs to retain it.
    **************************************************************************/
    int32_t (*SetParam)(const char* pName, const char* pValue);

    /**********************************************************************//**
    * Create()
    * -------------------------------------------------------------------------
    * Creates the plugin.
    *
    * \param
    *    pParams      [in] An array of qvr_plugin_param_t elements
    *    nParams      [in] Number of elements in the params array
    *    pTransforms  [in] An array of qvrservice_hw_transform_t elements
    *    nTransforms  [in] Number of elements in the transforms array
    *    callbacks    [in] Pointer to callbacks object to provide notifications
    * API level
    *    2 or higher
    * \return
    *    Returns 0 upon success, -1 otherwise
    * Notes
    *    1. This function may be called at boot time, so the plugin can use this
    *       function to do very basic initialization, but resource usage must be
    *       kept to an absolute minimum until Init() is called.
    *    2. Memory pointed to by the array pointers is only valid for the
    *       duration of the call to Create(), so the plugin should make copies
    *       if it needs to retain the contents.
    * Resource requirements
    *    After this function is called, the plugin must conform to the following
    *    contraints:
    *      - Memory: the plugin must minimize its memory usage
    *      - Power: the plugin must remain in a low power state
    **************************************************************************/
    int32_t (*Create)(qvr_plugin_param_t pParams[], int32_t nParams,
        qvrservice_hw_transform_t pTransforms[], int32_t nTransforms,
        qvr_plugin_callbacks_t *callbacks);

    /**********************************************************************//**
    * Destroy()
    * -------------------------------------------------------------------------
    * Destroys the plugin. The plugin should relinquish any resources it
    * obtained in Create().
    *
    * \param
    *    None
    * \return
    *    None
    * API level
    *    2 or higher
    * Notes
    *    None
    **************************************************************************/
    void (*Destroy)();

    /**********************************************************************//**
    * SetTransform()
    * -------------------------------------------------------------------------
    * Sets or updates a hardware transform.
    *
    * \param
    *    pTransform   [in] The hardware transform to set or update
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Notes
    *    Memory pointed to by the pTransform parameter is only valid for the
    *    duration of the call to SetTransform(), so the plugin should make a
    *    copy of pTransform if it needs to retain it.
    **************************************************************************/
    int32_t (*SetTransform)(qvrservice_hw_transform_t* pTransform);

    /**********************************************************************//**
    * GetData()
    * -------------------------------------------------------------------------
    * This function will be called when a QVRService client needs to query the
    * plugin for some data, e.g. user profile data.
    *
    * \param
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
    *            QVR_ERROR for other error
    * API level
    *    2 or higher
    * Notes
    *    This API is invoked by QVRService on behalf of a remote QVRService
    *    client that uses the QVRPluginData_GetData() API.
    *    The pPayload array will be filled in up to *pPayloadLen bytes
    *    so this may result in truncation of the payload if the required length
    *    is larger than the size passed in pPayloadLen.
    **************************************************************************/
    int32_t (*GetData)(char* pControl, uint32_t controlLen,
        char* pPayload, uint32_t* pPayloadLen);

    /**********************************************************************//**
    * SetData()
    * -------------------------------------------------------------------------
    * This function will be called when a QVRService client needs to configure
    * the plugin with some data, e.g. user profile data.
    *
    * \param
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
    *            QVR_ERROR for other error
    * API level
    *    2 or higher
    * Notes
    *    This API is invoked by QVRService on behalf of a remote QVRService
    *    client which would use the QVRPluginData_SetData API.
    **************************************************************************/
    int32_t (*SetData)(const char* pControl, uint32_t controlLen,
        const char* pPayload, uint32_t payloadLen);

    //Reserved for future use
    void* reserved[64 - 5];

} qvr_plugin_ops_t;

typedef struct qvr_plugin{
    QVRSERVICE_PLUGIN_API_VERSION api_version;
    QVRSERVICE_PLUGIN_ID plugin_id;
    qvr_plugin_ops_t*    ops;
    qvr_plugin_info_t    info;
} qvr_plugin_t;

qvr_plugin_t* getQVRPluginInstance(void);

/**********************************************************************//**
* QVRServicePluginCallbacks Helpers
**************************************************************************/
static __inline void QVRServicePluginCallbacks_NotifyError(
    qvr_plugin_callbacks_t *me, QVRSERVICE_PLUGIN_ERROR_STATE error_state,
    uint64_t param)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_2) return;
    if (NULL == me->ops || NULL == me->ops->NotifyError) return;
    me->ops->NotifyError(me->ctx, error_state, param);
}

/**********************************************************************//**
* QVRServicePlugin Helpers
**************************************************************************/
static __inline int32_t QVRServicePlugin_Init(qvr_plugin_t *me,
    const char* key)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_1) return -1;
    if (NULL == me->ops || NULL == me->ops->Init) return -1;
    return me->ops->Init(key);
}
static __inline int32_t QVRServicePlugin_Deinit(qvr_plugin_t *me)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_1) return -1;
    if (NULL == me->ops || NULL == me->ops->Deinit) return -1;
    return me->ops->Deinit();
}
static __inline int32_t QVRServicePlugin_Start(qvr_plugin_t *me)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_1) return -1;
    if (NULL == me->ops || NULL == me->ops->Start) return -1;
    return me->ops->Start();
}
static __inline int32_t QVRServicePlugin_Stop(qvr_plugin_t *me)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_1) return -1;
    if (NULL == me->ops || NULL == me->ops->Stop) return -1;
    return me->ops->Stop();
}
static __inline int32_t QVRServicePlugin_GetParam(qvr_plugin_t *me,
    const char* pName, uint32_t* pLen, char* pValue)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_1) return -1;
    if (NULL == me->ops || NULL == me->ops->GetParam) return -1;
    return me->ops->GetParam(pName, pLen, pValue);
}
static __inline int32_t QVRServicePlugin_SetParam(qvr_plugin_t *me,
    const char* pName, const char* pValue)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_1) return -1;
    if (NULL == me->ops || NULL == me->ops->SetParam) return -1;
    return me->ops->SetParam(pName, pValue);
}
static __inline int32_t QVRServicePlugin_Create(qvr_plugin_t *me,
    qvr_plugin_param_t* params, int32_t nParams,
    qvrservice_hw_transform_t transforms[], int32_t nTransforms,
    qvr_plugin_callbacks_t *callbacks)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_2) return -1;
    if (NULL == me->ops || NULL == me->ops->Create) return -1;
    return me->ops->Create(params, nParams, transforms, nTransforms, callbacks);
}
static __inline void QVRServicePlugin_Destroy(qvr_plugin_t *me)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_2) return;
    if (NULL == me->ops || NULL == me->ops->Destroy) return;
    me->ops->Destroy();
}
static __inline int32_t QVRServicePlugin_SetTransform(qvr_plugin_t *me,
    qvrservice_hw_transform_t* transform)
{
    if (me->api_version < QVRSERVICE_PLUGIN_API_VERSION_2) return -1;
    if (NULL == me->ops || NULL == me->ops->SetTransform) return -1;
    return me->ops->SetTransform(transform);
}

static __inline int32_t QVRServicePlugin_GetData(qvr_plugin_t *me,
    char* pControl, uint32_t controlLen,
    char* pPayload, uint32_t* pPayloadLen)
{
    if (NULL == me || NULL == me->ops->GetData) return -1;
    return me->ops->GetData(pControl, controlLen, pPayload, pPayloadLen);
}

static __inline int32_t QVRServicePlugin_SetData(qvr_plugin_t *me,
    const char* pControl, uint32_t controlLen,
    const char* pPayload, uint32_t payloadLen)
{
    if (NULL == me || NULL == me->ops->SetData) return -1;
    return me->ops->SetData(pControl, controlLen, pPayload, payloadLen);
}

#ifdef __cplusplus
}
#endif

#endif /* QVRSERVICE_PLUGIN_H */
