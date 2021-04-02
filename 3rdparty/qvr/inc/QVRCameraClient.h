/******************************************************************************/
/*! \file  QVRCameraClient.h  */
/*
* Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
* All Rights Reserved
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/

#ifndef QVRCAMERA_CLIENT_H
#define QVRCAMERA_CLIENT_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "QVRTypes.h"
#include "QVRCameraDeviceParam.h"

typedef void* qvrcamera_client_handle_t;
typedef void* qvrcamera_device_handle_t;

/**************************************************************************//**
* Camera Device Names
******************************************************************************/
#define QVRSERVICE_CAMERA_NAME_TRACKING     "tracking"
#define QVRSERVICE_CAMERA_NAME_EYE_TRACKING "eye-tracking"

/**************************************************************************//**
* Client Keys
* -----------------------------------------------------------------------------
* Keys that may be used with QVRCameraClient_CreateWithKey().
*   QVR_KEY_CAM_CLIENT_TRACKING_WITH_SYNC
*     Clients created with this key will be allowed to call
*     QVRCameraDevice_GetFrame() on the "tracking" camera with the blocking
*     mode set to QVRCAMERA_MODE_NON_BLOCKING_SYNC in order to enable the
*     timing-sync feature to reduce frame latency. This key may not be used
*     by more than one client at a time, so attempting to create a client with
*     this key if one has already been created will fail.
******************************************************************************/
#define QVR_KEY_CAM_CLIENT_TRACKING_WITH_SYNC   "qvr.key.cam.tracking.sync"

/**************************************************************************//**
* Error codes
******************************************************************************/
#define QVR_CAM_SUCCESS                     0
#define QVR_CAM_ERROR                      -1
#define QVR_CAM_CALLBACK_NOT_SUPPORTED     -2
#define QVR_CAM_API_NOT_SUPPORTED          -3
#define QVR_CAM_INVALID_PARAM              -4
#define QVR_CAM_EXPIRED_FRAMENUMBER        -5
#define QVR_CAM_FUTURE_FRAMENUMBER         -6
#define QVR_CAM_DROPPED_FRAMENUMBER        -7
#define QVR_CAM_DEVICE_NOT_STARTED         -8
#define QVR_CAM_DEVICE_STOPPING            -9
#define QVR_CAM_DEVICE_DETACHING           -10
#define QVR_CAM_DEVICE_ERROR               -11

/**************************************************************************//**
* QVRCAMERA_API_VERSION
* -----------------------------------------------------------------------------
* Defines the API versions of this interface. The api_version member of the
* qvr_camera_t object retrieved from the external camera library must
* be set to the API version that matches its functionality.
******************************************************************************/
typedef enum {
    QVRCAMERACLIENT_API_VERSION_1 = 1,
    QVRCAMERACLIENT_API_VERSION_2 = 2,
    QVRCAMERACLIENT_API_VERSION_3 = 3,
    QVRCAMERACLIENT_API_VERSION_4 = 4,
} QVRCAMERA_API_VERSION;

/**************************************************************************//**
* qvrcamera_frame_t
* -----------------------------------------------------------------------------
* frame metadata provided to each client as a result of GetFrame() calls
*    fn:                   Frame number
*    start_of_exposure_ts: Frame timestamp in ns (kernel boottime clk)
*    exposure:             Frame exposure time in ns
*    buffer:               Frame data
*    len:                  Frame length = width*height+
*                                         secondary_width*secondary_height
*    width:                Width of frame or crop-image1
*    height:               Height of frame or crop-image1
*    secondary_width:      Width of crop-image2 or 0 (no crop-image2)
*    secondary_height:     Height of crop-image2 or 0 (no crop-image2)
*    gain:                 Frame gain (available as of QVRCAMERACLIENT_API_VERSION_4)
*
* NOTE:
* The frame layout/size may change dynamically between frames due to
* changes in the crop settings made via calls to SetCropRegion().
* There are 3 supported frame cropping modes:
*
* Crop-off/single-crop: frame is a single image
*                       width/height: size of image
*                       secondary_width/height: not used and should be 0
*                       len: width*height
* Dual-crop:            frame is 2 images that are sequential in memory
*                       width/height: size of first image
*                       secondary_width/height: size of second image
*                       len: width*height + secondary_width*secondary_height
******************************************************************************/
typedef struct qvrcamera_frame
{
    uint32_t fn;
    uint64_t start_of_exposure_ts;
    uint32_t exposure;
    volatile uint8_t* buffer;
    uint32_t len;
    uint32_t width;
    uint32_t height;
    uint32_t secondary_width;
    uint32_t secondary_height;
    uint32_t gain;
} qvrcamera_frame_t;

typedef enum {
    QVRCAMERA_PARAM_NUM_TYPE_UINT8  = 0,
    QVRCAMERA_PARAM_NUM_TYPE_UINT16,
    QVRCAMERA_PARAM_NUM_TYPE_UINT32,
    QVRCAMERA_PARAM_NUM_TYPE_UINT64,
    QVRCAMERA_PARAM_NUM_TYPE_INT8,
    QVRCAMERA_PARAM_NUM_TYPE_INT16,
    QVRCAMERA_PARAM_NUM_TYPE_INT32,
    QVRCAMERA_PARAM_NUM_TYPE_INT64,
    QVRCAMERA_PARAM_NUM_TYPE_FLOAT,
    QVRCAMERA_PARAM_NUM_TYPE_FLOAT_VECT,
    QVRCAMERA_PARAM_NUM_TYPE_MAX = 0xFF
} QVRCAMERA_PARAM_NUM_TYPE;

/**************************************************************************//**
* QVRCAMERA_CLIENT_STATUS
* -----------------------------------------------------------------------------
*   QVRCAMERA_CLIENT_DISCONNECTED
*      The client was unexpectedly disconnected from server. If this occurs,
*      the QVRCameraClient object must be deleted.
*   QVRCAMERA_CLIENT_CONNECTED
*      The Client is connected
******************************************************************************/
typedef enum {
    QVRCAMERA_CLIENT_DISCONNECTED = 0,
    QVRCAMERA_CLIENT_CONNECTED,
    QVRCAMERA_CLIENT_STATUS_MAX = 0xff
} QVRCAMERA_CLIENT_STATUS;

/**************************************************************************//**
* QVRCAMERA_CAMERA_STATUS
* -----------------------------------------------------------------------------
*   QVRCAMERA_CAMERA_CLIENT_DISCONNECTED
*      Client has been disconnected from the server.
*      It is expected that the user would then call DetachCamera to release
*      the Camera Handle.
*   QVRCAMERA_CAMERA_INITIALISING
*      Camera Device is being initialized as soon as its first client attaches.
*      The state will automatically transition to
*      QVRCAMERA_CAMERA_READY once initialization is complete.
*   QVRCAMERA_CAMERA_READY
*      Camera Device is ready and can be started by its Master Client.
*   QVRCAMERA_CAMERA_STARTING
*      Camera Device is starting after having receive a start command from
*      its Master client. The state will automatically transition to
*      QVRCAMERA_CAMERA_STARTED once startup is complete.
*   QVRCAMERA_CAMERA_STARTED
*      Camera Device is started/active. While in this state, only the client
*      that started the Camera DeviceVR Camera can modify its state.
*   QVRCAMERA_CAMERA_STOPPING
*      Camera Device is stopping. The state will automatically transition to
*      QVRCAMERA_CAMERA_READY once complete.
*   QVRCAMERA_CAMERA_ERROR
*      Camera Device has encountered an error.
******************************************************************************/
typedef enum {
    QVRCAMERA_CAMERA_CLIENT_DISCONNECTED,
    QVRCAMERA_CAMERA_INITIALISING,
    QVRCAMERA_CAMERA_READY,
    QVRCAMERA_CAMERA_STARTING,
    QVRCAMERA_CAMERA_STARTED,
    QVRCAMERA_CAMERA_STOPPING,
    QVRCAMERA_CAMERA_ERROR,
    QVRCAMERA_CAMERA_STATUS_MAX = 0xff
} QVRCAMERA_CAMERA_STATUS;

/**************************************************************************//**
* QVRCAMERA_BLOCK_MODE
* -----------------------------------------------------------------------------
*   QVRCAMERA_BLOCK_MODE is an option offered when invoking
*   QVRCameraDevice_GetFrame().
*
*   QVRCAMERA_MODE_BLOCKING
*     QVRCameraDevice_GetFrame() will block until the requested frame number
*     is available. If the requested frame number is available,
*     QVRCameraDevice_GetFrame() returns immediately.
*
*   QVRCAMERA_MODE_NON_BLOCKING
*     QVRCameraDevice_GetFrame() will not block. If the requested frame number
*     is not available, QVRCameraDevice_GetFrame() returns immediately with
*     return code QVR_CAM_FUTURE_FRAMENUMBER.
*
*   QVRCAMERA_MODE_NON_BLOCKING_SYNC
*     Behavior identical to QVRCAMERA_MODE_NON_BLOCKING, plus:
*     Timing-sync will measure QVRCameraDevice_GetFrame() call frequency and
*     adjust camera timing to minimize latency. Calls must be made at regular
*     intervals (ie ~30/45/60 hz). Note that this blocking mode will only be
*     available to devices of clients that were created with
*     QVR_KEY_CAM_CLIENT_TRACKING_WITH_SYNC.
******************************************************************************/
typedef enum {
  QVRCAMERA_MODE_BLOCKING,
  QVRCAMERA_MODE_NON_BLOCKING,
  QVRCAMERA_MODE_NON_BLOCKING_SYNC,
} QVRCAMERA_BLOCK_MODE;

/**************************************************************************//**
* QVRCAMERA_DROP_MODE
* -----------------------------------------------------------------------------
*   QVRCAMERA_DROP_MODE is an option offered when invoking
*   QVRCameraDevice_GetFrame().
*
*   QVRCAMERA_MODE_NEWER_IF_AVAILABLE
*     QVRCameraDevice_GetFrame() will attempt to return at least the
*     requested frame number or a newer if available. If the requested frame
*     number is in the future, QVRCameraDevice_GetFrame() either blocks or
*     returns with error code QVR_CAM_FUTURE_FRAMENUMBER depending on the
*     QVRCAMERA_BLOCK_MODE specified.
*
*   QVRCAMERA_MODE_EXPLICIT_FRAME_NUMBER
*     QVRCameraDevice_GetFrame() will attempt to return the requested frame
*     number. If frame number is too old, QVRCameraDevice_GetFrame() returns
*     with error code QVR_CAM_EXPIRED_FRAMENUMBER. If frame number is in the
*     future, QVRCameraDevice_GetFrame() either blocks or returns with error
*     code QVR_CAM_FUTURE_FRAMENUMBER depending on the QVRCAMERA_BLOCK_MODE
*     specified.
******************************************************************************/
typedef enum {
  QVRCAMERA_MODE_NEWER_IF_AVAILABLE,
  QVRCAMERA_MODE_EXPLICIT_FRAME_NUMBER
} QVRCAMERA_DROP_MODE;

/**************************************************************************//**
* camera_client_status_callback_fn
* -----------------------------------------------------------------------------
* Callback for handling a camera client status event
*    pCtx:    [in] The context passed in to SetClientStatusCallback()
*    state:   [in] client's new state (CONNECTED or DISCONNECTED)
* -----------------------------------------------------------------------------
* Note: SetClientStatusCallback() is deprecated. Use
*       RegisterForClientNotification() instead.
******************************************************************************/
typedef void (*camera_client_status_callback_fn)(void *pCtx,
        QVRCAMERA_CLIENT_STATUS state);

/**************************************************************************//**
* camera_status_callback_fn
* -----------------------------------------------------------------------------
* Callback for handling a camera status event
*    pCtx:    [in] The context passed in to SetCameraStatusCallback()
*    state:   [in] Camera's new state
* -----------------------------------------------------------------------------
* Note: SetCameraStatusCallback() is deprecated. Use
*       RegisterForCameraNotification() instead.
******************************************************************************/
typedef void (*camera_status_callback_fn)(void *pCtx,
        QVRCAMERA_CAMERA_STATUS state);

/**************************************************************************//**
* QVRCAMERA_CLIENT_NOTIFICATION
* -----------------------------------------------------------------------------
*   CAMERA_CLIENT_NOTIFICATION_STATE_CHANGED
*      The client state has changed. camera_client_notification_callback_fn
*      payload will contain
*      qvrservice_camera_client_state_change_notify_payload_t.
******************************************************************************/
typedef enum {
    CAMERA_CLIENT_NOTIFICATION_STATE_CHANGED=0,
    CAMERA_CLIENT_NOTIFICATION_MAX
} QVRCAMERA_CLIENT_NOTIFICATION;

/**************************************************************************//**
* qvrservice_camera_client_state_change_notify_payload_t
* -----------------------------------------------------------------------------
* This structure is used to pass the payload to
* camera_client_notification_callback_fn.
*   new_state     : new QVRCAMERA_CLIENT_STATUS
*   previous_state: previous QVRCAMERA_CLIENT_STATUS
******************************************************************************/
typedef struct {
    QVRCAMERA_CLIENT_STATUS new_state;
    QVRCAMERA_CLIENT_STATUS previous_state;
} qvrservice_camera_client_state_change_notify_payload_t;

/**************************************************************************//**
* camera_client_notification_callback_fn
* -----------------------------------------------------------------------------
* Callback for handling camera client notifications.
* Callback is registered using RegisterForClientNotification().
*    pCtx:          [in] The context passed to RegisterForClientNotification().
*    notification   [in] Notification value specifying the reason for the
*                        callback.
*    payload:       [in] Pointer to payload. Payload type depends on
*                        notification. Memory for the payload is allocated
*                        by qvr service client and will be released when
*                        callback returns.
*    payload_length:[in] Length of valid data in payload.
******************************************************************************/
typedef void (*camera_client_notification_callback_fn)(void *pCtx,
        QVRCAMERA_CLIENT_NOTIFICATION notification, void *payload,
        uint32_t payload_length);

/**************************************************************************//**
* QVRCAMERA_DEVICE_NOTIFICATION
* -----------------------------------------------------------------------------
*   CAMERA_DEVICE_NOTIFICATION_STATE_CHANGED
*      The camera state has changed. camera_device_notification_callback_fn
*      payload will contain
*      qvrservice_camera_device_state_change_notify_payload_t.
******************************************************************************/
typedef enum {
    CAMERA_DEVICE_NOTIFICATION_STATE_CHANGED=0,
    CAMERA_DEVICE_NOTIFICATION_MAX
} QVRCAMERA_DEVICE_NOTIFICATION;

/**************************************************************************//**
* qvrservice_camera_device_state_change_notify_payload_t
* -----------------------------------------------------------------------------
* This structure is used to pass the payload to
* camera_device_notification_callback_fn.
*   new_state     : new QVRCAMERA_CAMERA_STATUS
*   previous_state: previous QVRCAMERA_CAMERA_STATUS
******************************************************************************/
typedef struct {
    QVRCAMERA_CAMERA_STATUS new_state;
    QVRCAMERA_CAMERA_STATUS previous_state;
} qvrservice_camera_device_state_change_notify_payload_t;

/**************************************************************************//**
* camera_device_notification_callback_fn
* -----------------------------------------------------------------------------
* Callback for handling a camera status event
*    pCtx:    [in] The context passed in to SetClientStatusCallback()
*    state:   [in] Camera's new state
******************************************************************************/
typedef void (*camera_device_notification_callback_fn)(void *pCtx,
        QVRCAMERA_DEVICE_NOTIFICATION notification, void *payload,
        uint32_t payload_length);


typedef struct qvrcamera_client_ops {

    /**********************************************************************//**
    * Create()
    * -------------------------------------------------------------------------
    * \return
    *    Returns qvrcamera_client_handle_t
    * API level
    *    1 or higher
    * Timing requirements
    *    This function needs to be called first before calling any other
    *    functions listed below. The client context received by calling
    *    this function needs to be passed to the other functions listed
    *    below.
    * Notes
    *    None
    **************************************************************************/
    qvrcamera_client_handle_t (*Create)();

    /**********************************************************************//**
    * Destroy()
    * -------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrcamera_client_handle_t returned by Create().
    * \return
    *    None
    * API level
    *    1 or higher
    * Timing requirements
    *    This function needs to be called when app shuts down. This
    *    function will destroy the client context therefore the same client
    *    context can't be used in any other functions listed below.
    * Notes
    *    None
    **************************************************************************/
    void (*Destroy)(qvrcamera_client_handle_t client);

    /**********************************************************************//**
    * SetClientStatusCallback()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrcamera_client_handle_t returned by Create().
    *    cb:       [in] Callback function to be called to handle status events
    *    pCtx:     [in] Context to be passed to callback function
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    This function may be called at any time. The client will maintain only
    *    one callback, so subsequent calls to this function will overwrite any
    *    previous callbacks set. cb may be set to NULL to disable status
    *    callbacks.
    * Notes
    *    SetClientStatusCallback() is deprecated. Use
    *    RegisterForClientNotification() instead.
    **************************************************************************/
    int32_t (*SetClientStatusCallback)(qvrcamera_client_handle_t client,
        camera_client_status_callback_fn cb, void *pCtx);

    /**********************************************************************//**
    * AttachCamera()
    * -------------------------------------------------------------------------
    * \param
    *    client:      [in] qvrcamera_client_handle_t returned by Create().
    *    pCameraName: [in] name of the camera to open (QVRSERVICE_CAMERA_NAME_*)
    * \return
    *    Returns qvrcamera_device_handle_t
    * API level
    *    1 or higher
    * Timing requirements
    *    This function needs to be called when the app needs to atttach to
    *    a specific camera. This function will return a
    *    qvrcamera_device_handle_t to further be used for camera control API.
    * Notes
    *    None
    **************************************************************************/
    qvrcamera_device_handle_t (*AttachCamera)(qvrcamera_client_handle_t client,
            const char* pCameraName);

    /**********************************************************************//**
    * CreateWithKey()
    * -------------------------------------------------------------------------
    * \return
    *    Returns qvrcamera_client_handle_t
    * \param
    *   szKey:        [in] String used to uniquely identify this client. See
    *                      notes for more info.
    * API level
    *    2 or higher
    * Timing requirements
    *    This function needs to be called first before calling any other
    *    functions client functions. The client context received by calling
    *    this function needs to be passed to the other client functions.
    * Notes
    *    CreateWithKey() is used for two purposes:
    *      1) to create a camera client that has special abilities and/or
    *         privileges that are not available to clients created with
    *         Create().
    *      2) to create a camera client that will be used from both the ARM
    *         and DSP (both sides must call CreateWithKey() with the same key).
    *    The key may be pre-defined (see QVR_KEY_CAM_CLIENT_*), user-defined, or
    *    it may come from the VR service itself (i.e. via the plugin interface).
    **************************************************************************/
    qvrcamera_client_handle_t (*CreateWithKey)(const char* szKey);

    /**********************************************************************//**
    * InitRingBufferData()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrcamera_client_handle_t returned by Create().
    *    id:       [in] ID of the ring buffer to write to
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    This function will fail if pData is NULL or if the len parameter
    *    does not match the structure size of the specified ring buffer ID.
    **************************************************************************/
    int32_t (*InitRingBufferData)(qvrcamera_client_handle_t client,
        QVRSERVICE_RING_BUFFER_ID id);

    /**********************************************************************//**
    * WriteRingBufferData()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrcamera_client_handle_t returned by Create().
    *    id:       [in] ID of the ring buffer to write to
    *    pData:    [in] Pointer to data to write to the ring buffer
    *    len:      [in] Length of data pointed to by pData
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    This function will fail if pData is NULL or if the len parameter
    *    does not match the structure size of the specified ring buffer ID.
    **************************************************************************/
    int32_t (*WriteRingBufferData)(qvrcamera_client_handle_t client,
        QVRSERVICE_RING_BUFFER_ID id, void* pData, uint32_t len);

    /**********************************************************************//**
    * GetParam()
    * -------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrcamera_client_handle_t returned by Create().
    *    pName:   [in] NUL-terminated name of the parameter length/value to
    *                  retrieve. Must not be NULL.
    *    pLen: [inout] If pValue is NULL, pLen will be filled in with the
    *                  number of bytes (including the NUL terminator) required
    *                  to hold the value of the parameter specified by pName. If
    *                  pValue is non-NULL, pLen must point to an integer that
    *                  represents the length of the buffer pointed to by pValue.
    *                  pLen must not be NULL.
    *    pValue:  [in] Buffer to receive value.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    The pValue buffer will be filled in up to *pLen bytes (including NUL),
    *    so this may result in truncation of the value if the required length
    *    is larger than the size passed in pLen.
    *    See QVRCameraDeviceParam.h for a list of available parameters.
    **************************************************************************/
    int32_t (*GetParam)(qvrcamera_client_handle_t client, const char* pName,
          uint32_t* pLen, char* pValue);

    /**********************************************************************//**
    * SetParam()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrcamera_client_handle_t returned by Create().
    *    pName:    [in] NUL-terminated name of parameter value to set. Must not
    *                   be NULL.
    *    pValue:   [in] NUL-terminated value. Must not be NULL.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    See QVRCameraDeviceParam.h for a list of available parameters.
    **************************************************************************/
    int32_t (*SetParam)(qvrcamera_client_handle_t client, const char* pName,
        const char* pValue);

    /***********************************************************************//**
    * RegisterForClientNotification()
    * --------------------------------------------------------------------------
    * \param
    *    client:       [in] qvrcamera_client_handle_t returned by Create().
    *    notification: [in] QVRCAMERA_CLIENT_NOTIFICATION to register for.
    *    cb:           [in] call back function of type notification_callback_fn.
    *    pCtx:         [in] Context to be passed to callback function.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    This function may be called at any time.
    * Notes
    *    The client will maintain only one callback, so subsequent calls to
    *    this function will overwrite the previous callback. cb may be set to
    *    NULL to disable notification callbacks.
    ***************************************************************************/
    int32_t (*RegisterForClientNotification)(qvrcamera_client_handle_t client,
        QVRCAMERA_CLIENT_NOTIFICATION notification,
        camera_client_notification_callback_fn cb, void *pCtx);

    /**********************************************************************//**
    * AttachCameraWithParams()
    * -------------------------------------------------------------------------
    * \param
    *    client:      [in] qvrcamera_client_handle_t returned by Create().
    *    pCameraName: [in] name of the camera to open (QVRSERVICE_CAMERA_NAME_*)
    *    pParams      [in] An array of qvr_plugin_param_t elements
    *    nParams      [in] Number of elements in the params array
    * \return
    *    Returns qvrcamera_device_handle_t
    * API level
    *    3 or higher
    * Timing requirements
    *    This function needs to be called when the app needs to atttach to
    *    a specific camera. This function will return a
    *    qvrcamera_device_handle_t to further be used for camera control API.
    * Notes
    *    None
    **************************************************************************/
    qvrcamera_device_handle_t (*AttachCameraWithParams)(
        qvrcamera_client_handle_t client, const char* pCameraName,
        qvr_plugin_param_t pParams[], int32_t nParams);

    //Reserved for future use
    void* reserved[64 - 7];

} qvrcamera_client_ops_t;


typedef struct qvrcamera_ops {

    /**********************************************************************//**
    * DetachCamera()
    * -------------------------------------------------------------------------
    * \param
    *    camera:   [in]  qvrcamera_device_handle_t returned by AttachCamera().
    * \return
    *    None
    * API level
    *    1 or higher
    * Timing requirements
    *    This function needs to be called when app is done with controlling
    *    a given camera, or done accessing its frames.
    *    This function will destroy the camera context therefore
    *    the same camera context can't be used in any other functions listed
    *    below.
    * Notes
    *    None
    **************************************************************************/
    void (*DetachCamera)(qvrcamera_device_handle_t camera);

    /**********************************************************************//**
    * GetCameraState()
    * -------------------------------------------------------------------------
    * \param
    *    camera:   [in]  qvrcamera_device_handle_t returned by AttachCamera().
    *    pState:   [out] current camera state
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    **************************************************************************/
    int32_t (*GetCameraState)(qvrcamera_device_handle_t camera,
        QVRCAMERA_CAMERA_STATUS *pState);

    /**********************************************************************//**
    * SetCameraStatusCallback()
    * -------------------------------------------------------------------------
    * \param
    *    camera:   [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    cb:       [in] Callback function to be called to handle status events
    *    pCtx:     [in] Context to be passed to callback function
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    This function may be called at any time. The client will maintain only
    *    one callback, so subsequent calls to this function will overwrite any
    *    previous callbacks set. cb may be set to NULL to disable status
    *    callbacks.
    * Notes
    *    SetCameraStatusCallback() is deprecated. Use
    *    RegisterForCameraNotification() instead.
    **************************************************************************/
    int32_t (*SetCameraStatusCallback)(qvrcamera_device_handle_t camera,
        camera_status_callback_fn cb, void *pCtx);

    /**********************************************************************//**
    * GetParam()
    * -------------------------------------------------------------------------
    * \param
    *    camera:  [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    pName:   [in] NUL-terminated name of the parameter length/value to
    *                  retrieve. Must not be NULL.
    *    pLen: [inout] If pValue is NULL, pLen will be filled in with the
    *                  number of bytes (including the NUL terminator) required
    *                  to hold the value of the parameter specified by pName. If
    *                  pValue is non-NULL, pLen must point to an integer that
    *                  represents the length of the buffer pointed to by pValue.
    *                  pLen must not be NULL.
    *    pValue:  [in] Buffer to receive value.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    The pValue buffer will be filled in up to *pLen bytes (including NUL),
    *    so this may result in truncation of the value if the required length
    *    is larger than the size passed in pLen.
    *    See QVRCameraDeviceParam.h for a list of available parameters.
    **************************************************************************/
    int32_t (*GetParam)(qvrcamera_device_handle_t camera, const char* pName,
          uint32_t* pLen, char* pValue);

    /**********************************************************************//**
    * SetParam()
    * -------------------------------------------------------------------------
    * \param
    *    camera:   [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    pName:    [in] NUL-terminated name of parameter value to set. Must not
    *                   be NULL.
    *    pValue:   [in] NUL-terminated value. Must not be NULL.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    See QVRCameraDeviceParam.h for a list of available parameters.
    **************************************************************************/
    int32_t (*SetParam)(qvrcamera_device_handle_t camera, const char* pName,
        const char* pValue);

    /**********************************************************************//**
    * GetParamNum()
    * -------------------------------------------------------------------------
    * \param
    *    camera:  [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    pName:   [in] NUL-terminated name of the parameter length/value to
    *                  retrieve. Must not be NULL.
    *    type:    [in] type of the parameter requested.
    *    size:    [in] size of the numerical type which pointer is passed in
    *                  pValue, this size must be equal to the parameter's type
    *                  size.
    *    pValue: [out] Buffer to receive value.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    The pValue buffer will be filled in up to size bytes.
    *    See QVRCameraDeviceParam.h for a list of available parameters.
    **************************************************************************/
    int32_t (*GetParamNum)(qvrcamera_device_handle_t camera, const char* pName,
            QVRCAMERA_PARAM_NUM_TYPE type, uint8_t size, char* pValue);

    /**********************************************************************//**
    * SetParamNum()
    * -------------------------------------------------------------------------
    * \param
    *    camera: [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    pName:  [in] NUL-terminated name of the parameter length/value to
    *                 set. Must not be NULL.
    *    type:   [in] type of the parameter requested.
    *    size:   [in] size of the numerical type which pointer is passed in
    *                 pValue, this size must be equal to the parameter's type
    *                 size.
    *    pValue: [in] Buffer holding the parameter's value to be set.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    See QVRCameraDeviceParam.h for a list of available parameters.
    **************************************************************************/
    int32_t (*SetParamNum)(qvrcamera_device_handle_t camera, const char* pName,
            QVRCAMERA_PARAM_NUM_TYPE type, uint8_t size, const char* pValue);

    /**********************************************************************//**
    * Start()
    * -------------------------------------------------------------------------
    * Start the attached camera.
    * \param
    *    camera:   [in] qvrcamera_device_handle_t returned by AttachCamera().
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    *
    **************************************************************************/
    int32_t (*Start)(qvrcamera_device_handle_t camera);

    /**********************************************************************//**
    * Stop()
    * -------------------------------------------------------------------------
    * Stop the attached camera.
    * \param
    *    camera:   [in] qvrcamera_device_handle_t returned by AttachCamera().
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    **************************************************************************/
    int32_t (*Stop)(qvrcamera_device_handle_t camera);

    /**********************************************************************//**
    * GetCurrentFrameNumber()
    * -------------------------------------------------------------------------
    * Retrieves current / latest frame number received from the attached camera.
    * \param
    *    camera:   [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    fn:      [out] current / latest frame number received
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    **************************************************************************/
    int32_t (*GetCurrentFrameNumber)(qvrcamera_device_handle_t camera,
        int32_t *fn);

    /**********************************************************************//**
    * SetExposureAndGain()
    * -------------------------------------------------------------------------
    * Apply exposure and gain to the attached camera.
    * \param
    *    camera:      [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    exposure_ns: [in] exposure duration in nanoseconds to be applied
    *                      to the attached camera.
    *    iso_gain:    [in] iso gain to be applied to the attached camera.
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    * Notes
    *    These exposure and gain settings are applied by the QVR camera Service
    *    right after the next start of frame, and will take effect in a few
    *    frames depending on the ISP exposure pipeline (up to 6 frames).
    **************************************************************************/
    int32_t (*SetExposureAndGain) (qvrcamera_device_handle_t camera,
                                   uint64_t exposure_ns, int iso_gain);

    /**********************************************************************//**
    * GetFrame()
    * -------------------------------------------------------------------------
    * GetFrame allows the client to retrieve by frame number, the camera
    * frame metadata and buffer.
    * When GetFrame returns the frame buffer is automatically locked
    * and ready for use.
    * ReleaseFrame must be invoked in order to release the frame buffer lock.
    * \param
    *    camera:      [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    fn:       [inout] input: requested frame number
    *                      output: *fn is updated with the actual
    *                      returned frame number.
    *    block:       [in] refer to QVRCAMERA_BLOCK_MODE description
    *    drop:        [in] refer to QVRCAMERA_DROP_MODE description.
    *    pframe:     [out] frame metadata and buffer pointer structure
    * \return
    *    returns QVR_CAM_SUCCESS upon success,
    *    QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    **************************************************************************/
    int32_t (*GetFrame)(qvrcamera_device_handle_t camera,
                        int32_t *fn,
                        QVRCAMERA_BLOCK_MODE block,
                        QVRCAMERA_DROP_MODE drop,
                        qvrcamera_frame_t* pframe);

    /**********************************************************************//**
    * ReleaseFrame()
    * -------------------------------------------------------------------------
    * Releases the lock on the frame buffer
    * \param
    *    camera:      [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    fn:          [in] frame number for which the frame buffer lock is
    *                      to be released
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    1 or higher
    **************************************************************************/
    int32_t (*ReleaseFrame)(qvrcamera_device_handle_t client, int32_t fn);

    /**********************************************************************//**
    * SetCropRegion()
    * -------------------------------------------------------------------------
    * Sets the crop region on the attached camera
    * \param
    *    camera:      [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    l_top:       [in] Specifies the top (left) corner of the crop region in
    *                      pixels. If the crop region starts at the first pixel,
    *                      this value should be zero.
    *    l_left:      [in] Specifies the left (top) corner of the crop region in
    *                      pixels. If the crop region starts at the first pixel,
    *                      this value should be zero.
    *    l_width:     [in] Specifies the width of the crop region in pixels
    *    l_height:    [in] Specifies the height of the crop region in pixels
    *    r_top:       [in] Specifies the top (left) corner of the crop region in
    *                      pixels. If the crop region starts at the first pixel,
    *                      this value should be zero.
    *    r_left:      [in] Specifies the left (top) corner of the crop region in
    *                      pixels. If the crop region starts at the first pixel,
    *                      this value should be zero.
    *    r_width:     [in] Specifies the width of the crop region in pixels
    *    r_height:    [in] Specifies the height of the crop region in pixels
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    2 or higher
    **************************************************************************/
    int32_t (*SetCropRegion)(qvrcamera_device_handle_t client,
                            uint32_t l_top,
                            uint32_t l_left,
                            uint32_t l_width,
                            uint32_t l_height,
                            uint32_t r_top,
                            uint32_t r_left,
                            uint32_t r_width,
                            uint32_t r_height);

    /***********************************************************************//**
    * RegisterForCameraNotification()
    * --------------------------------------------------------------------------
    * \param
    *    client:       [in] qvrcamera_device_handle_t returned by Create().
    *    notification: [in] QVRCAMERA_DEVICE_NOTIFICATION to register for.
    *    cb:           [in] callback function of type notification_callback_fn.
    *    pCtx:         [in] Context to be passed to callback function.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    This function may be called at any time.
    * Notes
    *    The client will maintain only one callback, so subsequent calls to
    *    this function will overwrite the previous callback. cb may be set to
    *    NULL to disable notification callbacks.
    ***************************************************************************/
    int32_t (*RegisterForCameraNotification)(qvrcamera_device_handle_t camera,
            QVRCAMERA_DEVICE_NOTIFICATION notification, camera_device_notification_callback_fn cb,
            void *pCtx);

    /**********************************************************************//**
    * SetGammaCorrectionValue()
    * -------------------------------------------------------------------------
    * Apply gamma correction value to the attached camera.
    * \param
    *    camera:      [in] qvrcamera_device_handle_t returned by AttachCamera().
    *    gamma:       [in] gamma correction value to be applied
    * \return
    *    returns QVR_CAM_SUCCESS upon success, QVR_CAM_ERROR otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    This function may be called at any time.
    * Notes
    *    This gamma correction setting is applied by the QVR camera Service
    *    right after the next start of frame, and will take effect immediately.
    **************************************************************************/
    int32_t (*SetGammaCorrectionValue) (qvrcamera_device_handle_t camera,
                                   float gamma_correction);

    //Reserved for future use
    void* reserved[64 - 3];

} qvrcamera_ops_t;

typedef struct qvrcamera_client{
    QVRCAMERA_API_VERSION api_version;
    qvrcamera_client_ops_t* ops;
} qvrcamera_client_t;

typedef struct qvrcamera {
    QVRCAMERA_API_VERSION api_version;
    qvrcamera_ops_t* ops;
} qvrcamera_device_t;

qvrcamera_client_t* getQvrCameraClientInstance(void);
qvrcamera_device_t* getQvrCameraDeviceInstance(void);


// QVRCameraClient helper
#ifdef __hexagon__
#define QVRCAMERA_CLIENT_LIB "libqvr_cam_dsp_driver_skel.so"
#else
#define QVRCAMERA_CLIENT_LIB "libqvrcamera_client.so"
#endif

typedef struct {
    void* libHandle;
    qvrcamera_client_t* client;
    qvrcamera_client_handle_t clientHandle;
} qvrcamera_client_helper_t;

typedef struct {
    qvrcamera_client_t* client;
    qvrcamera_device_t* cam;
    qvrcamera_device_handle_t cameraHandle;
} qvrcamera_device_helper_t;

static inline QVRCAMERA_API_VERSION QVRCameraClient_GetVersion(
    qvrcamera_client_helper_t* me)
{
    return me->client->api_version;
}

static inline qvrcamera_client_helper_t* QVRCameraClient_Create()
{
    qvrcamera_client_helper_t* me = (qvrcamera_client_helper_t*)
                                    malloc(sizeof(qvrcamera_client_helper_t));
    if(!me) return NULL;

    typedef qvrcamera_client_t* (*qvrcamera_client_wrapper_fn)(void);
    qvrcamera_client_wrapper_fn qvrCameraClient;

    me->libHandle = NULL;
    qvrCameraClient = (qvrcamera_client_wrapper_fn)dlsym(RTLD_DEFAULT,
                                                "getQvrCameraClientInstance");
    if (!qvrCameraClient) {
        me->libHandle = dlopen( QVRCAMERA_CLIENT_LIB, RTLD_NOW);
        if (!me->libHandle) {
            free(me);
            me = NULL;
            return NULL;
        }

        qvrCameraClient = (qvrcamera_client_wrapper_fn)dlsym(me->libHandle,
                                                    "getQvrCameraClientInstance");
    }

    if (!qvrCameraClient) {
        if (me->libHandle != NULL) {
            dlclose(me->libHandle);
            me->libHandle = NULL;
        }
        free(me);
        me = NULL;
        return NULL;
    }

    me->client = qvrCameraClient();
    me->clientHandle = me->client->ops->Create();
    if(!me->clientHandle){
        if (me->libHandle != NULL) {
            dlclose(me->libHandle);
            me->libHandle = NULL;
        }
        free(me);
        me = NULL;
        return NULL;
    }

    return me;
}

static inline qvrcamera_client_helper_t* QVRCameraClient_CreateWithKey(
        const char* szKey)
{
    qvrcamera_client_helper_t* me = (qvrcamera_client_helper_t*)
                                    malloc(sizeof(qvrcamera_client_helper_t));
    if(!me) return NULL;

    typedef qvrcamera_client_t* (*qvrcamera_client_wrapper_fn)(void);
    qvrcamera_client_wrapper_fn qvrCameraClient;

    me->libHandle = NULL;
    qvrCameraClient = (qvrcamera_client_wrapper_fn)dlsym(RTLD_DEFAULT,
                                                "getQvrCameraClientInstance");
    if (!qvrCameraClient) {
        me->libHandle = dlopen( QVRCAMERA_CLIENT_LIB, RTLD_NOW);
        if (!me->libHandle) {
            free(me);
            me = NULL;
            return NULL;
        }

        qvrCameraClient = (qvrcamera_client_wrapper_fn)dlsym(me->libHandle,
                                                    "getQvrCameraClientInstance");
    }

    if (!qvrCameraClient) {
        if (me->libHandle != NULL) {
            dlclose(me->libHandle);
            me->libHandle = NULL;
        }
        free(me);
        me = NULL;
        return NULL;
    }

    me->client = qvrCameraClient();
    me->clientHandle = me->client->ops->CreateWithKey(szKey);
    if(!me->clientHandle){
        if (me->libHandle != NULL) {
            dlclose(me->libHandle);
            me->libHandle = NULL;
        }
        free(me);
        me = NULL;
        return NULL;
    }
    return me;
}

static inline void QVRCameraClient_Destroy(qvrcamera_client_helper_t* me)
{
    if(!me) return;

    if(me->client->ops->Destroy){
        me->client->ops->Destroy( me->clientHandle);
    }

    if(me->libHandle ){
        dlclose(me->libHandle);
        me->libHandle = NULL;
    }
    free(me);
    me = NULL;
}

// SetClientStatusCallback() is deprecated. Use RegisterForClientNotification()
// instead.
static inline int32_t QVRCameraClient_SetClientStatusCallback(
   qvrcamera_client_helper_t* helper, camera_client_status_callback_fn cb,
   void *pCtx)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->client->ops->SetClientStatusCallback)
        return QVR_CAM_API_NOT_SUPPORTED;

    return helper->client->ops->SetClientStatusCallback(
        helper->clientHandle, cb, pCtx);
}

static inline qvrcamera_device_helper_t* QVRCameraClient_AttachCamera(
    qvrcamera_client_helper_t* helper, const char* pCameraName)
{
    qvrcamera_device_helper_t* me = (qvrcamera_device_helper_t*)
        malloc(sizeof(qvrcamera_device_helper_t));
    if(!me) return NULL;

    me->client = helper->client;

    if(!helper->client->ops->AttachCamera) {
        free(me);
        me = NULL;
        return NULL;
    }

    me->cameraHandle = helper->client->ops->AttachCamera(
                                  helper->clientHandle, pCameraName);
    if (!me->cameraHandle) {
        free(me);
        me = NULL;
        return NULL;
    }

    typedef qvrcamera_device_t* (*qvrcamera_device_wrapper_fn)(void);
    qvrcamera_device_wrapper_fn qvrCameraDevice;

    qvrCameraDevice = (qvrcamera_device_wrapper_fn)dlsym(RTLD_DEFAULT,
        "getQvrCameraDeviceInstance");
    if (!qvrCameraDevice) {
        qvrCameraDevice = (qvrcamera_device_wrapper_fn)dlsym(helper->libHandle,
            "getQvrCameraDeviceInstance");
    }

    if (!qvrCameraDevice) {
        free(me);
        me = NULL;
        return NULL;
    }

    me->cam = qvrCameraDevice();

    return me;
}

static inline qvrcamera_device_helper_t* QVRCameraClient_AttachCameraWithParams(
    qvrcamera_client_helper_t* helper, const char* pCameraName,
    qvr_plugin_param_t pParams[], int32_t nParams)
{
    qvrcamera_device_helper_t* me = (qvrcamera_device_helper_t*)
        malloc(sizeof(qvrcamera_device_helper_t));
    if(!me) return NULL;

    me->client = helper->client;

    if(!helper->client->ops->AttachCameraWithParams) {
        free(me);
        me = NULL;
        return NULL;
    }

    me->cameraHandle = helper->client->ops->AttachCameraWithParams(
        helper->clientHandle, pCameraName, pParams, nParams);
    if (!me->cameraHandle) {
        free(me);
        me = NULL;
        return NULL;
    }

    typedef qvrcamera_device_t* (*qvrcamera_device_wrapper_fn)(void);
    qvrcamera_device_wrapper_fn qvrCameraDevice;

    qvrCameraDevice = (qvrcamera_device_wrapper_fn)dlsym(RTLD_DEFAULT,
        "getQvrCameraDeviceInstance");
    if (!qvrCameraDevice) {
        qvrCameraDevice = (qvrcamera_device_wrapper_fn)dlsym(helper->libHandle,
            "getQvrCameraDeviceInstance");
    }

    if (!qvrCameraDevice) {
        free(me);
        me = NULL;
        return NULL;
    }

    me->cam = qvrCameraDevice();

    return me;
}

static inline int32_t QVRCameraClient_InitRingBufferData(
    qvrcamera_client_helper_t* helper, QVRSERVICE_RING_BUFFER_ID id)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->client->ops->InitRingBufferData) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->client->ops->InitRingBufferData(
        helper->clientHandle, id);
}

static inline int32_t QVRCameraClient_WriteRingBufferData(
    qvrcamera_client_helper_t* helper, QVRSERVICE_RING_BUFFER_ID id,
    void* pData, uint32_t len)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->client->ops->WriteRingBufferData) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->client->ops->WriteRingBufferData(
        helper->clientHandle, id, pData, len);
}

static inline int32_t QVRCameraClient_GetParam(
    qvrcamera_client_helper_t* helper, const char* pName, uint32_t* pLen,
    char* pValue)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->client->ops->GetParam) return QVR_CAM_API_NOT_SUPPORTED;
    return helper->client->ops->GetParam(
        helper->clientHandle, pName, pLen, pValue);
}
static inline int32_t QVRCameraClient_SetParam(
    qvrcamera_client_helper_t* helper, const char* pName, const char* pValue)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->client->ops->SetParam) return QVR_CAM_API_NOT_SUPPORTED;
    return helper->client->ops->SetParam(
        helper->clientHandle, pName, pValue);
}

static inline int32_t QVRCameraClient_RegisterForClientNotification(
   qvrcamera_client_helper_t* helper,
   QVRCAMERA_CLIENT_NOTIFICATION notification,
   camera_client_notification_callback_fn cb,
   void *pCtx)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->client->ops->RegisterForClientNotification)
        return QVR_CAM_API_NOT_SUPPORTED;

    return helper->client->ops->RegisterForClientNotification(
        helper->clientHandle, notification, cb, pCtx);
}

// QVRCameraDevice helper
static inline QVRCAMERA_API_VERSION QVRCameraDevice_GetVersion(
    qvrcamera_device_helper_t* me)
{
    return me->cam->api_version;
}

static inline void QVRCameraDevice_DetachCamera(
    qvrcamera_device_helper_t* helper)
{
    if(!helper) return;

    if(helper->cam->ops->DetachCamera){
        helper->cam->ops->DetachCamera( helper->cameraHandle);
    }

    free(helper);
    helper = NULL;
}

static inline int32_t QVRCameraDevice_GetCameraState(
    qvrcamera_device_helper_t* helper, QVRCAMERA_CAMERA_STATUS *pState)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->GetCameraState) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->GetCameraState(
        helper->cameraHandle, pState);
}

// SetCameraStatusCallback() is deprecated. Use RegisterForCameraNotification()
// instead.
static inline int32_t QVRCameraDevice_SetCameraStatusCallback(
    qvrcamera_device_helper_t* helper, camera_status_callback_fn cb, void *pCtx)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetCameraStatusCallback)
        return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->SetCameraStatusCallback(
        helper->cameraHandle, cb, pCtx);
}

static inline int32_t QVRCameraDevice_GetParam(
    qvrcamera_device_helper_t* helper, const char* pName, uint32_t* pLen,
    char* pValue)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->GetParam) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->GetParam(
        helper->cameraHandle, pName, pLen, pValue);
}

static inline int32_t QVRCameraDevice_SetParam(
    qvrcamera_device_helper_t* helper, const char* pName, const char* pValue)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetParam) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->SetParam(
        helper->cameraHandle, pName, pValue);
}

static inline int32_t QVRCameraDevice_GetParamNum(
    qvrcamera_device_helper_t* helper, const char* pName,
    QVRCAMERA_PARAM_NUM_TYPE type, uint32_t size, char* pValue)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->GetParamNum) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->GetParamNum(
        helper->cameraHandle, pName, type, size, pValue);
}

static inline int32_t QVRCameraDevice_SetParamNum(
    qvrcamera_device_helper_t* helper, const char* pName,
    QVRCAMERA_PARAM_NUM_TYPE type, uint32_t size, char* pValue)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetParamNum) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->SetParamNum(
        helper->cameraHandle, pName, type, size, pValue);
}

static inline int32_t QVRCameraDevice_Start(qvrcamera_device_helper_t* helper)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->Start) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->Start(
        helper->cameraHandle);
}

static inline int32_t QVRCameraDevice_Stop(qvrcamera_device_helper_t* helper)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetParamNum) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->Stop(
        helper->cameraHandle);
}

static inline int32_t QVRCameraDevice_SetExposureAndGain(
    qvrcamera_device_helper_t* helper, uint64_t exposure_ns, int iso_gain)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetExposureAndGain) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->SetExposureAndGain(
        helper->cameraHandle,exposure_ns,iso_gain);
}

static inline int32_t QVRCameraDevice_GetCurrentFrameNumber(
    qvrcamera_device_helper_t* helper, int32_t* fn)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->GetCurrentFrameNumber)
        return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->GetCurrentFrameNumber(
        helper->cameraHandle,fn);
}

static inline int32_t QVRCameraDevice_GetFrame(
    qvrcamera_device_helper_t* helper,
    int32_t *fn,
    QVRCAMERA_BLOCK_MODE block,
    QVRCAMERA_DROP_MODE drop,
    qvrcamera_frame_t* pframe)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->GetFrame) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->GetFrame(
        helper->cameraHandle,fn,block,drop,pframe);
}

static inline int32_t QVRCameraDevice_ReleaseFrame(
    qvrcamera_device_helper_t* helper,
    int32_t fn)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->ReleaseFrame) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->ReleaseFrame(
        helper->cameraHandle,fn);
}

static inline int32_t QVRCameraDevice_SetCropRegion(
    qvrcamera_device_helper_t* helper,
    uint32_t l_top,
    uint32_t l_left,
    uint32_t l_width,
    uint32_t l_height,
    uint32_t r_top,
    uint32_t r_left,
    uint32_t r_width,
    uint32_t r_height)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetCropRegion) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->SetCropRegion(
        helper->cameraHandle, l_top, l_left, l_width, l_height,r_top, r_left, r_width, r_height);
}

static inline int32_t QVRCameraDevice_SetGammaCorrectionValue(
    qvrcamera_device_helper_t* helper, float gamma)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->SetGammaCorrectionValue) return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->SetGammaCorrectionValue(
        helper->cameraHandle,gamma);
}

static inline int32_t QVRCameraDevice_RegisterForCameraNotification(
    qvrcamera_device_helper_t* helper,
    QVRCAMERA_DEVICE_NOTIFICATION notification,
    camera_device_notification_callback_fn cb,
    void *pCtx)
{
    if(!helper) return QVR_CAM_INVALID_PARAM;
    if(!helper->cam->ops->RegisterForCameraNotification)
        return QVR_CAM_API_NOT_SUPPORTED;

    return helper->cam->ops->RegisterForCameraNotification(
        helper->cameraHandle, notification, cb, pCtx);
}

// Debug helpers
#define QVR_CASE_RETURN_STR(a)  case a: return #a

static inline const char* QVRCameraHelper_ShowState(QVRCAMERA_CAMERA_STATUS state)
{
    switch(state){
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_CLIENT_DISCONNECTED);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_INITIALISING);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_READY);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_STARTING);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_STARTED);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_STOPPING);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_ERROR);
        QVR_CASE_RETURN_STR(QVRCAMERA_CAMERA_STATUS_MAX);
        default: return "unknown";
    }
}

static inline const char* QVRCameraHelper_ShowCamError(int error)
{
    switch(error){
        QVR_CASE_RETURN_STR(QVR_CAM_SUCCESS);
        QVR_CASE_RETURN_STR(QVR_CAM_ERROR);
        QVR_CASE_RETURN_STR(QVR_CAM_CALLBACK_NOT_SUPPORTED);
        QVR_CASE_RETURN_STR(QVR_CAM_API_NOT_SUPPORTED);
        QVR_CASE_RETURN_STR(QVR_CAM_INVALID_PARAM);
        QVR_CASE_RETURN_STR(QVR_CAM_EXPIRED_FRAMENUMBER);
        QVR_CASE_RETURN_STR(QVR_CAM_FUTURE_FRAMENUMBER);
        QVR_CASE_RETURN_STR(QVR_CAM_DROPPED_FRAMENUMBER);
        QVR_CASE_RETURN_STR(QVR_CAM_DEVICE_STOPPING);
        QVR_CASE_RETURN_STR(QVR_CAM_DEVICE_NOT_STARTED);
        QVR_CASE_RETURN_STR(QVR_CAM_DEVICE_DETACHING);
        QVR_CASE_RETURN_STR(QVR_CAM_DEVICE_ERROR);
        default: return "unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* QVRCAMERA_CLIENT_H */
