/******************************************************************************/
/*! \file  QVRServiceClient.h */
/*
* Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
* All Rights Reserved
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/
#ifndef QVRSERVICE_CLIENT_H
#define QVRSERVICE_CLIENT_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "QVRTypes.h"
#include "QVRPluginData.h"

/**************************************************************************//**
* Error codes
******************************************************************************/
#define QVR_SUCCESS                    0
#define QVR_ERROR                      -1
#define QVR_CALLBACK_NOT_SUPPORTED     -2
#define QVR_API_NOT_SUPPORTED          -3
#define QVR_INVALID_PARAM              -4
#define QVR_BUSY                       -5


/**************************************************************************//**
* QVRSERVICE_SERVICE_VERSION
* -----------------------------------------------------------------------------
* Description
*   Can be used with GetParam() to retrieve the VR Service's version.
* Access
*   Read only
* Notes
*   None
******************************************************************************/
#define QVRSERVICE_SERVICE_VERSION    "service-version"

/**************************************************************************//**
* QVRSERVICE_CLIENT_VERSION
* -----------------------------------------------------------------------------
* Description
*   Can be used with GetParam() to retrieve the VR Service client's version.
* Access
*   Read only
* Notes
*   None
******************************************************************************/
#define QVRSERVICE_CLIENT_VERSION     "client-version"

/**************************************************************************//**
* QVRSERVICE_TRACKER_ANDROID_OFFSET_NS
* -----------------------------------------------------------------------------
* Description
*   Can be used with GetParam() to retrieve the fixed offset between the
*   tracker time domain (QTimer) and the Android time domain in ns.
* Access
*   Read only
* Notes
*   None
******************************************************************************/
#define QVRSERVICE_TRACKER_ANDROID_OFFSET_NS "tracker-android-offset-ns"

/**************************************************************************//**
* QVRSERVICE Plugin Names
******************************************************************************/
#define QVRSERVICE_PLUGIN_EYE_TRACKING     "plugin-eye-tracking"

/**************************************************************************//**
* QVRSERVICE_VRMODE_STATE
* -----------------------------------------------------------------------------
*   VRMODE_UNSUPPORTED
*      VR Mode is unsupported on this device.
*   VRMODE_STARTING
*      VR Mode is starting. The state will automatically transition to
*      VRMODE_STARTED once startup is complete.
*   VRMODE_STARTED
*      VR Mode is started/active. While in this state, only the client that
*      started VR Mode can modify the VR Mode state.
*   VRMODE_STOPPING
*      VR Mode is stopping. The state will automatically transition to
*      VRMODE_STOPPED once complete.
*   VRMODE_STOPPED
*      VR Mode is stopped/inactive. While in this state, any client can modify
*      the VR Mode state.
******************************************************************************/
typedef enum {
    VRMODE_UNSUPPORTED = 0,
    VRMODE_STARTING,
    VRMODE_STARTED,
    VRMODE_STOPPING,
    VRMODE_STOPPED,
} QVRSERVICE_VRMODE_STATE;

/**************************************************************************//**
* QVRSERVICE_TRACKING_MODE
* -----------------------------------------------------------------------------
*   TRACKING_MODE_NONE
*      Tracking is disabled. Calls to GetHeadTrackingData() will fail.
*   TRACKING_MODE_ROTATIONAL
*      Rotational mode provides tracking information using 3 degrees of freedom,
*      i.e. "what direction am I facing?". When this mode is used, the rotation
*      quaternion in the qvrservice_head_tracking_data_t structure will be
*      filled in when GetHeadTrackingData() is called.
*   TRACKING_MODE_POSITIONAL
*      Positional mode provides tracking information using 6 degrees of freedom,
*      i.e. "what direction am I facing, and where am I relative to my starting
*      point?" When this mode is used, both the rotation quaternion and
*      translation vector in the qvrservice_head_tracking_data_t structure
*      will be filled in when GetHeadTrackingData() is called.
*   TRACKING_MODE_ROTATIONAL_MAG
*      Rotational_Mag mode provides tracking information similar to
*      TRACKING_MODE_ROTATIONAL but without any drift over time. The drift
*      sensitive apps (i.e. movie theater) would use this tracking mode.
*      However, this mode may require spacial movement of the device to have
*      the mag sensor calibrated (i.e. figure 8 movement). Also, this mode
*      is very sensitive to magnetic fields. In case of uncalibrated mag
*      data, this mode automatically defaults to TRACKING_MODE_ROTATIONAL which
*      may exhibit slow drift over time.
******************************************************************************/
typedef enum {
    TRACKING_MODE_NONE = 0,
    TRACKING_MODE_ROTATIONAL = 0x1,
    TRACKING_MODE_POSITIONAL = 0x2,
    TRACKING_MODE_ROTATIONAL_MAG = 0x4,
} QVRSERVICE_TRACKING_MODE;

/**************************************************************************//**
* qvrservice_sensor_data_raw_t
* -----------------------------------------------------------------------------
* This structure contains raw sensor values in the following format:
*   gts:        Timestamp of the gyroscope data in ns (QTimer time domain)
*   ats:        Timestamp of the accelerometer data in in (QTimer time domain)
*   gx, gy, gz: Gyroscope X, Y, and Z values in m/s/s
*   ax, ay, az: Accelerometer X, Y, and Z values in rad/s
*   mts:        Timestamp of the magnetometer data in ns (QTimer time domain)
*   mx, my, mz: Magnetometer X, Y, and Z values in uT
*   reserved:   Reserved for future use
******************************************************************************/
typedef struct {
    uint64_t gts;
    uint64_t ats;
    float    gx;
    float    gy;
    float    gz;
    float    ax;
    float    ay;
    float    az;
    uint64_t mts;
    float    mx;
    float    my;
    float    mz;
    uint8_t  reserved[4];
} qvrservice_sensor_data_raw_t;

/**************************************************************************//**
* QVRSERVICE_DISP_INTERRUPT_ID
* -----------------------------------------------------------------------------
*   DISP_INTERRUPT_VSYNC
*      This is the display VSYNC signal. It fires at the beginning of every
*      frame.
*      Interrupt config data: pointer to qvrservice_vsync_interrupt_config_t
*   DISP_INTERRUPT_LINEPTR
*      This interrupt can be configured to interrupt after a line of data has
*      been transmitted to the display.
*      Interrupt config data: pointer to qvrservice_lineptr_interrupt_config_t
******************************************************************************/
typedef enum {
    DISP_INTERRUPT_VSYNC = 0,
    DISP_INTERRUPT_LINEPTR,
    DISP_INTERRUPT_MAX
} QVRSERVICE_DISP_INTERRUPT_ID;

/**************************************************************************//**
* disp_interrupt_callback_fn
* -----------------------------------------------------------------------------
* Callback for handling a display interrupt
*    pCtx:    The context passed in to SetDisplayInterruptConfig()
*    ts:      The timestamp of the hardware interrupt
******************************************************************************/
typedef void (*disp_interrupt_callback_fn)(void *pCtx, uint64_t ts);

/**************************************************************************//**
* qvrservice_vsync_interrupt_config_t
* -----------------------------------------------------------------------------
* This structure is passed to SetDisplayInterruptConfig() and registers a
* callback to be notified whenever a VSYNC interrupt occurs.
*    cb:      Callback to call when interrupt occurs. Set to NULL to disable
*             callbacks.
*    ctx:     Context passed to callback
******************************************************************************/
typedef struct {
    disp_interrupt_callback_fn cb;
    void *ctx;
} qvrservice_vsync_interrupt_config_t;

/**************************************************************************//**
* qvrservice_lineptr_interrupt_config_t
* -----------------------------------------------------------------------------
* This structure is passed to SetDisplayInterruptConfig() and serves two
* purposes: 1) configure lineptr interrupts, and 2) register a callback to be
* notified whenever a lineptr interrupt occurs.
*    cb:      Callback to call when interrupt occurs. Set to NULL to disable
*             callbacks.
*    ctx:     Context passed to callback
*    line:    1 to N, where N is the width of the display (in pixels), enables
*             interrupts. Set to 0 to disable interrupts.
*
* Note: with the introduction of interrupt capture (see
* SetDisplayInterruptCapture()), enabling/disabling of callbacks is decoupled
* from configuring the lineptr interrupt. In other words, on devices that do
* not support callbacks, this structure still must be used to configure the
* lineptr interrupt.
******************************************************************************/
typedef struct {
    disp_interrupt_callback_fn cb;
    void *ctx;
    uint32_t line;
} qvrservice_lineptr_interrupt_config_t;

/**************************************************************************//**
* qvrservice_ts_t
* -----------------------------------------------------------------------------
* This structure can be used to retrieve display interrupt timestamps.
*    ts:       timestamp in ns, relative to BOOTTIME
*    count:    number of interrupts received since VR Mode started
*    reserved: reserved for future use
******************************************************************************/
typedef struct {
    uint64_t ts;
    uint32_t count;
    uint32_t reserved;
} qvrservice_ts_t;

/**************************************************************************//**
* QVRSERVICE_CLIENT_STATUS
* -----------------------------------------------------------------------------
*This enum is deprecated and is replaced by QVRSERVICE_CLIENT_NOTIFICATION
*   STATUS_DISCONNECTED
*      The client was unexpectedly disconnected from server. If this occurs,
*      the QVRServiceClient object must be deleted.
*   STATUS_STATE_CHANGED
*      The VR Mode state has changed. arg1 will contain the new state,
*      arg2 will contain the previous state.
*   STATUS_SENSOR_ERROR
*      The sensor stack has detected an error. The arg parameters will be set
*      to useful values to help identify the error (TBD).
******************************************************************************/
typedef enum {
    STATUS_DISCONNECTED = 0,
    STATUS_STATE_CHANGED,
    STATUS_SENSOR_ERROR,
    STATUS_MAX = 0xffffffff
} QVRSERVICE_CLIENT_STATUS;

/**************************************************************************//**
* client_status_callback
* -----------------------------------------------------------------------------
* Callback for handling a client status event
* This callback is deprecated and is replaced by notification_callback_fn
*    pCtx:    [in] The context passed in to SetClientStatusCallback()
*    status:  [in] Status value specifying the reason for the callback
*    arg1:    [in] Argument 1 (depends on status)
*    arg2:    [in] Argument 2 (depends on status)
******************************************************************************/
typedef void (*client_status_callback_fn)(void *pCtx,
    QVRSERVICE_CLIENT_STATUS status, uint32_t arg1, uint32_t arg2);

/**************************************************************************//**
* QVRSERVICE_CLIENT_NOTIFICATION
* -----------------------------------------------------------------------------
*   NOTIFICATION_DISCONNECTED
*      The client was unexpectedly disconnected from server. If this occurs,
*      the QVRServiceClient object must be deleted. notification_callback_fn
*      payload will be NULL.
*   NOTIFICATION_STATE_CHANGED
*      The VR Mode state has changed. notification_callback_fn payload will
*      contain qvrservice_state_notify_payload_t.
*   NOTIFICATION_SENSOR_ERROR
*      The sensor stack has detected an error. notification_callback_fn
*      payload will contain useful values to help identify the error(TBD).
*   NOTIFICATION_SUBSYSTEM_ERROR
*      The Subsystem has detected an error. notification_callback_fn
*      payload (qvrservice_subsystem_error_notify_payload_t struct)
*      will contain useful values to help identify the error.
*   NOTIFICATION_THERMAL_INFO
*      Thermal information from system. notification_callback_fn payload
*      will contain qvrservice_therm_notify_payload_t. Notification will stop
*      when VR mode stops.
******************************************************************************/
typedef enum {
    NOTIFICATION_DISCONNECTED = 0,
    NOTIFICATION_STATE_CHANGED,
    NOTIFICATION_SENSOR_ERROR,
    NOTIFICATION_THERMAL_INFO,
    NOTIFICATION_SUBSYSTEM_ERROR,
    NOTIFICATION_MAX
} QVRSERVICE_CLIENT_NOTIFICATION;

/**************************************************************************//**
* notification_callback_fn
* -----------------------------------------------------------------------------
* Callback for handling client notifications.
* Callback is registered using RegisterForNotification().
*    pCtx:          [in] The context passed to RegisterForNotification().
*    notification   [in] Notification value specifying the reason for the
*                        callback.
*    payload:       [in] Pointer to payload. Payload type depends on
*                        notification. Memory for the payload is allocated
*                        by qvr service client and will be released when
*                        call back returns.
*    payload_length:[in] Length of valid data in payload.
******************************************************************************/
typedef void (*notification_callback_fn)(void *pCtx,
    QVRSERVICE_CLIENT_NOTIFICATION notification, void *payload,
    uint32_t payload_length);

/**************************************************************************//**
* QVRSERVICE_TEMP_LEVEL
* -----------------------------------------------------------------------------
*   TEMP_SAFE
*      hardware is at a safe operating temperature
*   TEMP_LEVEL_[x]
*      hardware is at a level at which corrective actions can be taken
*   TEMP_CRITICAL
*      hardware is at a critical operating temperature
*      system may experience extreme drop in performance or shutdown.
******************************************************************************/
typedef enum {
    TEMP_SAFE = 0,
    TEMP_LEVEL_1,
    TEMP_LEVEL_2,
    TEMP_LEVEL_3,
    TEMP_CRITICAL
} QVRSERVICE_TEMP_LEVEL;

/**************************************************************************//**
* QVRSERVICE_MITIGATION_ACTION
* -----------------------------------------------------------------------------
*   MIT_ACT_NONE
*       No action needed
*   MIT_ACT_INC
*       Raise the FPS or EYE BUFFER RESOLUTION
*   MIT_ACT_DEC
*      Reduce the FPS or EYE BUFFER RESOLUTION
******************************************************************************/
typedef enum {
    MIT_ACT_NONE,
    MIT_ACT_INC,
    MIT_ACT_DEC
} QVRSERVICE_MITIGATION_ACTION;

/**************************************************************************//**
* QVRSERVICE_HW_TYPE
* -----------------------------------------------------------------------------
*   HW_TYPE_CPU
*      Hardware type is CPU
*   HW_TYPE_GPU
*      Hardware type is GPU
*   HW_TYPE_SKIN
*      Hardware type is SKIN
******************************************************************************/
typedef enum {
    HW_TYPE_CPU = 1,
    HW_TYPE_GPU,
    HW_TYPE_SKIN,
    MAX_HW_TYPE
} QVRSERVICE_HW_TYPE;

/**************************************************************************//**
* QVRSERVICE_PERF_LEVEL
* -----------------------------------------------------------------------------
*   Used to specify performance levels for HW_TYPE_CPU and HW_TYPE_GPU
*   PERF_LEVEL_DEFAULT
*      Both CPU and GPU will run at default system settings if either
*      HW_TYPE_CPU or HW_TYPE_GPU votes for this.
*   PERF_LEVEL_[x]
*      1 is the lowest performance level and 5 is the highest
*      level for CPU and GPU can be selected as per the app requirements
******************************************************************************/
typedef enum {
    PERF_LEVEL_DEFAULT = 0,
    PERF_LEVEL_1,
    PERF_LEVEL_2,
    PERF_LEVEL_3,
    MAX_PERF_LEVEL
} QVRSERVICE_PERF_LEVEL;

/**************************************************************************//**
* qvrservice_perf_level_t
* -----------------------------------------------------------------------------
* This structure is used to pass the performance levels to
* SetOperatingLevel()
*   hw_type    : QVRSERVICE_HW_TYPE to vote for.
                 Supports only HW_TYPE_CPU and HW_TYPE_GPU
*   perf_level : QVRSERVICE_PERF_LEVEL requested for
******************************************************************************/
typedef struct {
    QVRSERVICE_HW_TYPE hw_type;
    QVRSERVICE_PERF_LEVEL perf_level;
} qvrservice_perf_level_t;

/**************************************************************************//**
* QVRSERVICE_THREAD_TYPE
* -----------------------------------------------------------------------------
* RENDER
*     Thread renders frames to GPU
* WARP
*     Thread sends time warp request to GPU
* CONTROLLER
*     Thread handles controller signals
* NORMAL
*     Thread handles other VR APP actions
******************************************************************************/
typedef enum {
    RENDER = 0,
    WARP,
    CONTROLLER,
    NORMAL,
    MAX_THREAD_TYPE
} QVRSERVICE_THREAD_TYPE;

/**************************************************************************//**
* qvrservice_state_notify_payload_t
* -----------------------------------------------------------------------------
* This structure is used to pass the payload to notification_callback_fn
* registered for NOTIFICATION_STATE_CHANGED
*   new_state     : new QVRSERVICE_VRMODE_STATE
*   previous_state: previous QVRSERVICE_VRMODE_STATE
******************************************************************************/
typedef struct {
    QVRSERVICE_VRMODE_STATE new_state;
    QVRSERVICE_VRMODE_STATE previous_state;
} qvrservice_state_notify_payload_t;

/**************************************************************************//**
* qvrservice_therm_notify_payload_t
* -----------------------------------------------------------------------------
* This structure is used to pass the payload to notification_callback_fn
* registered for NOTIFICATION_THERMAL_INFO
*   hw_type :    QVRSERVICE_HW_TYPE for which notification was triggered
*   temp_level:  QVRSERVICE_TEMP_LEVEL of the hardware
*   eye_buf_res: QVRSERVICE_MITIGATION_ACTION for eye buffer resolution
*   fps:         QVRSERVICE_MITIGATION_ACTION for fps
******************************************************************************/
typedef struct {
    QVRSERVICE_HW_TYPE hw_type;
    QVRSERVICE_TEMP_LEVEL temp_level;
    QVRSERVICE_MITIGATION_ACTION eye_buf_res;
    QVRSERVICE_MITIGATION_ACTION fps;
    QVRSERVICE_MITIGATION_ACTION reserved[3];
} qvrservice_therm_notify_payload_t;

/**************************************************************************//**
* QVRSERVICE_SUBSYSTEM_TYPE
* -----------------------------------------------------------------------------
*   QVRSERVICE_SUBSYSTEM_TYPE_SENSOR
*      Represents sensor subsystem
*   QVRSERVICE_SUBSYSTEM_TYPE_TRACKING
*      Represents tracking subsystem
******************************************************************************/
typedef enum{
    QVRSERVICE_SUBSYSTEM_TYPE_SENSOR = 0,
    QVRSERVICE_SUBSYSTEM_TYPE_TRACKING
}QVRSERVICE_SUBSYSTEM_TYPE;

/**************************************************************************//**
* QVRSERVICE_SUBSYSTEM_ERROR_STATE
* -----------------------------------------------------------------------------
*   QVRSERVICE_SUBSYSTEM_ERROR_STATE_RECOVERING
*      Indicates qvrservice is trying to recover from subsystem error
*   QVRSERVICE_SUBSYSTEM_ERROR_STATE_RECOVERED
*      Indicates qvrservice is successfully recovered from subsystem error
*   QVRSERVICE_SUBSYSTEM_ERROR_STATE_UNRECOVERABLE
*      Indicates qvrservice is unable to recover from the subsystem error,
*      device restart is required
******************************************************************************/
typedef enum{
    QVRSERVICE_SUBSYSTEM_ERROR_STATE_RECOVERING = 0,
    QVRSERVICE_SUBSYSTEM_ERROR_STATE_RECOVERED,
    QVRSERVICE_SUBSYSTEM_ERROR_STATE_UNRECOVERABLE
}QVRSERVICE_SUBSYSTEM_ERROR_STATE;

/**************************************************************************//**
* qvrservice_subsystem_error_notify_payload_t
* -----------------------------------------------------------------------------
* This structure is used to pass the payload to notification_callback_fn
* registered for NOTIFICATION_SUBSYSTEM_ERROR
*   subsystem_type:  QVRSERVICE_SUBSYSTEM_TYPE
*   error_state:     QVRSERVICE_SUBSYSTEM_ERROR_STATE
*   param:           if QVRSERVICE_SUBSYSTEM_ERROR_STATE is
*                    QVRSERVICE_SUBSYSTEM_ERROR_STATE_RECOVERING, then indicates
*                    expected time to recovery in seconds
******************************************************************************/
typedef struct {
    QVRSERVICE_SUBSYSTEM_TYPE subsystem_type;
    QVRSERVICE_SUBSYSTEM_ERROR_STATE error_state;
    uint64_t param;
} qvrservice_subsystem_error_notify_payload_t;


/**************************************************************************//**
* QVRSERVICE_EYE_TRACKING_MODE_XXX
* -----------------------------------------------------------------------------
* These macros are used with the GetEyeTrackingMode() and SetEyeTrackingMode()
* APIs.
*   QVRSERVICE_EYE_TRACKING_MODE_NONE
*      Indicates that eye tracking is not supported or disabled
*   QVRSERVICE_EYE_TRACKING_MODE_DUAL
*      Indicates that stereo eye tracking is supported or enabled
******************************************************************************/
#define QVRSERVICE_EYE_TRACKING_MODE_NONE       0x0
#define QVRSERVICE_EYE_TRACKING_MODE_DUAL       0x1


/**************************************************************************//**
* Client APIs to communicate with QVRService. Typical call flow is as follows:
*   1. Create QVRServiceClient object
*   2. Call GetVRMode() to verify VR mode is supported and in the STOPPED state
*   3. Call RegisterForNotification() to get notified of events
*   4. Call GetTrackingMode()/SetTrackinMode() to configure tracking
*   5. Call SetDisplayInterruptConfig() to handle display interrupts
*   6. Call StartVRMode() to start VR mode
*       - Handle display interrupt events
*       - Call GetHeadTrackingData() to read latest tracking data
*   7. Call StopVRMode() when to end VR mode
*   8. Delete QVRServiceClient object
******************************************************************************/

/**************************************************************************//**
* QVRSERVICECLIENT_API_VERSION
* -----------------------------------------------------------------------------
* Defines the API versions of this interface. The api_version member of the
* qvrservice_client_t structure must be set to accurately reflect the version
* of the API that the implementation supports.
******************************************************************************/
typedef enum {
    QVRSERVICECLIENT_API_VERSION_1 = 1,
    QVRSERVICECLIENT_API_VERSION_2 = 2,
    QVRSERVICECLIENT_API_VERSION_3 = 3,
    QVRSERVICECLIENT_API_VERSION_4 = 4,
    QVRSERVICECLIENT_API_VERSION_5 = 5,
} QVRSERVICECLIENT_API_VERSION;

/**************************************************************************//**
* QVRSERVICE_TRANSFORMATION_MATRIX_TYPE
* -----------------------------------------------------------------------------
* QVRSERVICE_LATE_LATCHING_PRE_TRANSFORMATION_MAT
*     Apply this matrix on the left side of the predicted pose in late latching
* QVRSERVICE_LATE_LATCHING_POST_TRANSFORMATION_MAT
*     Apply this matrix on the right side of the predicted pose in late latching
******************************************************************************/
typedef enum {
    QVRSERVICE_LATE_LATCHING_PRE_TRANSFORMATION_MAT= 0,
    QVRSERVICE_LATE_LATCHING_POST_TRANSFORMATION_MAT,
    QVRSERVICE_MAX_TRANSFORMATION_MAT
} QVRSERVICE_TRANSFORMATION_MATRIX_TYPE;

/// float precision 3D vector
typedef struct XrVector3f {
   float x;
   float y;
   float z;
} XrVector3f;

/// a single point within a point cloud
typedef struct XrMapPointQTI {
   XrVector3f position;
   uint32_t id;
} XrMapPointQTI;

/// point cloud
typedef struct XrPointCloudQTI {
   uint64_t timestamp;          //!< timestamp in Android boottime ns of when the points were updated
   uint32_t max_points;         //!< maximum number of elements in the points array
   uint32_t num_points;         //!< number of valid points in the points array
   uint32_t reserved1;          //!< reserved for future use
   uint32_t reserved2;          //!< reserved for future use
   XrMapPointQTI points[];      //!< map points
} XrPointCloudQTI;

typedef void* qvrservice_client_handle_t;

typedef struct qvrservice_client_ops {

    /**********************************************************************//**
    * Create()
    * -------------------------------------------------------------------------
    * \return
    *    Returns qvrservice_client_handle_t
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
    qvrservice_client_handle_t (*Create)();

    /**********************************************************************//**
    * Destroy()
    * -------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrservice_client_handle_t returned by Create().
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
    void (*Destroy)(qvrservice_client_handle_t client);

    /**********************************************************************//**
    * SetClientStatusCallback()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    cb:       [in] Callback function to be called to handle status events
    *    pCtx:     [in] Context to be passed to callback function
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    This function may be called at any time. The client will maintain only
    *    one callback, so subsequent calls to this function will overwrite any
    *    previous callbacks set. cb may be set to NULL to disable status
    *    callbacks.
    * Notes
    *    This API is deprecated and is replaced by RegisterForNotification()
    **************************************************************************/
    int32_t (*SetClientStatusCallback)(qvrservice_client_handle_t client,
        client_status_callback_fn cb, void *pCtx);

    /**********************************************************************//**
    * GetVRMode()
    * -------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrservice_client_handle_t returned by Create().
    * \return
    *    Current VR Mode state
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    See QVRSERVICE_VRMODE_STATE for more info
    **************************************************************************/
    QVRSERVICE_VRMODE_STATE (*GetVRMode)(qvrservice_client_handle_t client);

    /**********************************************************************//**
    * StartVRMode()
    * -------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrservice_client_handle_t returned by Create().
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    This function may be called at any time if the VR Mode is in the
    *    VRMODE_STOPPED state. Calling this function while the VR Mode is in
    *    any other state will return an error.
    * Notes
    *    The caller should not assume that VR Mode configuration (e.g. calls
    *    to SetParam(), SetDisplayInterruptConfig()) will persist through
    *    start/stop cycles. Therefore it is recommended to always reconfigure
    *    VR Mode to suit the caller's use case prior to or after calling
    *    StartVRMode(). See QVRSERVICE_VRMODE_STATE for more info.
    **************************************************************************/
    int32_t (*StartVRMode)(qvrservice_client_handle_t client);

    /**********************************************************************//**
    * StopVRMode()
    * -------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrservice_client_handle_t returned by Create().
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    See QVRSERVICE_VRMODE_STATE for more info
    **************************************************************************/
    int32_t (*StopVRMode)(qvrservice_client_handle_t client);

    /***********************************************************************//**
    * GetTrackingMode()
    * --------------------------------------------------------------------------
    * \param
    *    client:           [in] qvrservice_client_handle_t returned by Create().
    *    pCurrentMode:    [out] If non-NULL, will be set to the currently
    *                           configured tracking mode
    *    pSupportedModes: [out] If non-NULL, will be set to a bitmask
    *                           representing which tracking modes are supported
    *                           by the device.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    This function may be called at any time.
    * Notes
    *    None
    ***************************************************************************/
    int32_t (*GetTrackingMode)(qvrservice_client_handle_t client,
        QVRSERVICE_TRACKING_MODE *pCurrentMode,
        uint32_t *pSupportedModes);

    /***********************************************************************//**
    * SetTrackingMode()
    * --------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrservice_client_handle_t returned by Create().
    *    mode:    [in] Tracking mode to set
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    This function must be called prior to calling StartVRMode().
    * Notes
    *    None
    ***************************************************************************/
    int32_t (*SetTrackingMode)(qvrservice_client_handle_t client,
         QVRSERVICE_TRACKING_MODE mode);

    /***********************************************************************//**
    * SetDisplayInterruptConfig()
    * --------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    id:       [in] Display interrupt to use (see
    *                   QVRSERVICE_DISP_INTERRUPT_ID)
    *    pCfg:     [in] Configuration information for display interrupt
    *    cfgSize:  [in] Size of the data passed in the pCfg pointer
    * \return
    *    If the configuration information specified a callback but callbacks
    *    are not supported on the device, then QVR_CALLBACK_NOT_SUPPORTED is
    *    returned. Otherwise returns QVR_SUCCESS upon success or QVR_ERROR if
    *    another error occurred.
    * API level
    *    1 or higher
    * Timing requirements
    *    This function may be called at any time, but callbacks will only occur
    *    when VR Mode is in the VRMODE_STARTED state and the device supports
    *    callbacks.
    * Notes
    *    If VR Mode is in the VRMODE_STARTED state, this function will only
    *    work if called by the same client that started VR Mode. If running
    *    on a device that doesn't support callbacks, see
    *    SetDisplayInterruptCapture() and GetDisplayInterruptTimestamp() as
    *    an alternate method to get display interrupt timestamps.
    ***************************************************************************/
    int32_t (*SetDisplayInterruptConfig)(qvrservice_client_handle_t client,
        QVRSERVICE_DISP_INTERRUPT_ID id,
        void *pCfg, uint32_t cfgSize);

    /***********************************************************************//**
    * SetThreadPriority()
    * --------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    tid:      [in] Thread ID of the thread whose priority will be changed
    *    policy:   [in] Scheduling policy to set. Use SCHED_FIFO or SCHED_RR for
    *                   real-time performance.
    *    priority: [in] Priority value. For real-time policies, the values can
    *                   be in the range of 1-99 (higher the number, higher the
    *                   priority).
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    See sched.h for more info.
    *    This API is deprecated and is replaced by SetThreadAttributesByType()
    ***************************************************************************/
    int32_t (*SetThreadPriority)(qvrservice_client_handle_t client, int tid,
          int policy, int priority);

    /**********************************************************************//**
    * GetParam()
    * -------------------------------------------------------------------------
    * \param
    *    client:    [in] qvrservice_client_handle_t returned by Create().
    *    pName:     [in] NUL-terminated name of the parameter length/value to
    *                    retrieve. Must not be NULL.
    *    pLen:   [inout] If pValue is NULL, pLen will be filled in with the
    *                    number of bytes (including the NUL terminator) required
    *                    to hold the value of the parameter specified by pName.
    *                    If pValue is non-NULL, pLen must point to an integer
    *                    that represents the length of the buffer pointed to by
    *                    pValue. pLen must not be NULL.
    *    pValue:    [in] Buffer to receive value.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    The pValue buffer will be filled in up to *pLen bytes (including NUL),
    *    so this may result in truncation of the value if the required length
    *    is larger than the size passed in pLen.
    **************************************************************************/
    int32_t (*GetParam)(qvrservice_client_handle_t client, const char* pName,
          uint32_t* pLen, char* pValue);

    /**********************************************************************//**
    * SetParam()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    pName:    [in] NUL-terminated name of parameter value to set. Must not
    *                   be NULL.
    *    pValue:   [in] NUL-terminated value. Must not be NULL.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    Some parameters may only be able to be set when VR Mode is in the
    *    VRMODE_STOPPED state. Additionally, some parameters may only be able to
    *    be set when VR Mode is in the VRMODE_STARTED state if SetParam() is
    *    called from the same client that started VR Mode.
    * Notes
    *    None
    **************************************************************************/
    int32_t (*SetParam)(qvrservice_client_handle_t client, const char* pName,
        const char* pValue);

    /**********************************************************************//**
    * GetSensorRawData()
    * -------------------------------------------------------------------------
    * \param
    *    client:    [in] qvrservice_client_handle_t returned by Create().
    *    ppData:   [out] Address of pointer to qvrservice_sensor_data_raw_t
    *                    structure that will contain the raw data values.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    None
    **************************************************************************/
    int32_t (*GetSensorRawData)(qvrservice_client_handle_t client,
        qvrservice_sensor_data_raw_t **ppData);

    /**********************************************************************//**
    * GetHeadTrackingData()
    * -------------------------------------------------------------------------
    * \param
    *    client:    [in] qvrservice_client_handle_t returned by Create().
    *    ppData:   [out] Address of pointer to qvrservice_head_tracking_data_t
    *                    structure that will be updated to point to the pose
    *                    data within a ring buffer. See Notes for more info.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    1 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *  - To minimize latency, a ring buffer is used to hold many poses, and a
    *    call to this function will set ppData to point to a single pose within
    *    that ring buffer. The buffer will be sized appropriately to provide at
    *    a minimum 80 milliseconds worth of poses. As such, the data pointed to
    *    by ppData may change after that amount of time passes.
    **************************************************************************/
    int32_t (*GetHeadTrackingData)(qvrservice_client_handle_t client,
        qvrservice_head_tracking_data_t **ppData);

    /**********************************************************************//**
    * GetRingBufferDescriptor()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    id   :    [in] ID of the ring buffer descriptor to retrieve
    *    pDesc:   [out] Ring buffer descriptor
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    Client needs to read the index value from ring buffer's index offset,
    *    then should access the element from the buffer's ring offset
    *    for retrieving the latest updated element in the buffer
    *    Client may obtain the memory address of the ring buffer by invoking mmap
    *    using the pDesc returned attributes:
    *       - file descriptor (pDesc->fd)
    *       - and size (pDesc->size)
    *    If Client invokes mmap, client shall invoke munmap when the buffer is
    *    not needed anymore.
    *    Client shall always close the returned file descriptor (pDesc->fd)
    *    when the buffer is not needed anymore.
    **************************************************************************/
    int32_t (*GetRingBufferDescriptor)(qvrservice_client_handle_t client,
        QVRSERVICE_RING_BUFFER_ID id,
        qvrservice_ring_buffer_desc_t *pDesc);

    /**********************************************************************//**
    * GetHistoricalHeadTrackingData()
    * -------------------------------------------------------------------------
    * \param
    *    client:        [in] qvrservice_client_handle_t returned by Create().
    *    ppData:       [out] Address of pointer to
    *                        qvrservice_head_tracking_data_t structure that will
    *                        be updated to point to the pose data within a ring
    *                        buffer. See Notes for more info.
    *    timestampNs:   [in] The time in nanoseconds for the pose requested
    *                        (which must be a time in the past). This value must
    *                        be in the Tracker (Qtimer) time domain. A value of
    *                        0 requests the most recent pose.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *  - To minimize latency, a ring buffer is used to hold many poses, and a
    *    call to this function will set ppData to point to a single pose within
    *    that ring buffer. The buffer will be sized appropriately to provide at
    *    a minimum 80 milliseconds worth of poses. As such, the data pointed to
    *    by ppData may change after that amount of time passes.
    *  - Use QVRSERVICE_TRACKER_ANDROID_OFFSET_NS for converting between
    *    Tracker and Android time domains.
    **************************************************************************/
    int32_t (*GetHistoricalHeadTrackingData)(
        qvrservice_client_handle_t client,
        qvrservice_head_tracking_data_t **ppData, int64_t timestampNs);

    /**********************************************************************//**
    * SetDisplayInterruptCapture()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    id:       [in] Display interrupt to configure capture on (see
    *                   QVRSERVICE_DISP_INTERRUPT_ID).
    *    mode:     [in] Set to 1 to enable capture, 0 to disable
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time, but timestamp capture
    *    will only occur while VR Mode is in the VRMODE_STARTED state.
    * Notes
    *    SetDisplayInterruptCapture() is meant to be used on devices that
    *    do not support display interrupt client callbacks. See
    *    GetDisplayInterruptTimestamp() and SetDisplayInterruptConfig() for
    *    more info.
    **************************************************************************/
    int32_t (*SetDisplayInterruptCapture)(qvrservice_client_handle_t client,
        QVRSERVICE_DISP_INTERRUPT_ID id, uint32_t mode);

    /**********************************************************************//**
    * GetDisplayInterruptTimestamp()
    * -------------------------------------------------------------------------
    * \param
    *    client:   [in] qvrservice_client_handle_t returned by Create().
    *    id:       [in] Display interrupt to retrieve the timestamp for (see
    *                   QVRSERVICE_DISP_INTERRUPT_ID).
    *    ppTs:    [out] Pointer to qvrservice_ts_t to receive the most recent
    *                   timestamp for the specified display interrupt.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    2 or higher
    * Timing requirements
    *    None. This function may be called at any time. However, timestamps
    *    will only be updated if SetDisplayInterruptCapture() has been called
    *    to enable timestamp capture and VR Mode is in the VRMODE_STARTED state.
    * Notes
    *    GetDisplayInterruptTimestamp() is meant to be used on devices that
    *    do not support display interrupt client callbacks. See
    *    SetDisplayInterruptCapture() and SetDisplayInterruptConfig() for more
    *    info.
    **************************************************************************/
    int32_t (*GetDisplayInterruptTimestamp)(qvrservice_client_handle_t client,
        QVRSERVICE_DISP_INTERRUPT_ID id, qvrservice_ts_t** ppTs);

    /***********************************************************************//**
    * RegisterForNotification()
    * --------------------------------------------------------------------------
    * \param
    *    client:       [in] qvrservice_client_handle_t returned by Create().
    *    notification: [in] QVRSERVICE_CLIENT_NOTIFICATION to register for.
    *    cb:           [in] call back function of type notification_callback_fn.
    *    pCtx:         [in] Context to be passed to callback function.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    3 or higher
    * Timing requirements
    *    This function may be called at any time.
    * Notes
    *    The client will maintain only one callback, so subsequent calls to
    *    this function will overwrite the previous callback. cb may be set to
    *    NULL to disable notification callbacks.
    ***************************************************************************/
    int32_t (*RegisterForNotification)(qvrservice_client_handle_t client,
        QVRSERVICE_CLIENT_NOTIFICATION notification, notification_callback_fn cb,
        void *pCtx);

    /***********************************************************************//**
    * SetThreadAttributesByType()
    * --------------------------------------------------------------------------
    * \param
    *    client:      [in] qvrservice_client_handle_t returned by Create().
    *    tid:         [in] Thread ID of the thread whose type is set
    *    thread_type: [in] QVRSERVICE_THREAD_TYPE of the thread
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    3 or higher
    * Timing requirements
    *    None.
    * Notes
    *    Thread attributes can include scheduler priority, scheduler
    *    policy and cpu affinity.
    ***************************************************************************/
    int32_t (*SetThreadAttributesByType)(qvrservice_client_handle_t client,
        int tid, QVRSERVICE_THREAD_TYPE thread_type);

    /***********************************************************************//**
    * SetOperatingLevel()
    * --------------------------------------------------------------------------
    * \param
    *    client:          [in] qvrservice_client_handle_t returned by Create().
    *    perf_levels:     [in] array of qvrservice_perf_level_t
    *    num_perf_levels: [in] num of values in perf_levels array
    *    char *:               Reserved for future use. Set to NULL.
    *    uint32_t *:           Reserved for future use. Set to NULL.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    3 or higher
    * Timing requirements
    *     VR mode should be started before calling this function.
    * Notes
    *    All the operating levels set on the previous call will be
    *    revoked in the subsequent calls.
    ***************************************************************************/
    int32_t (*SetOperatingLevel)(qvrservice_client_handle_t client,
        qvrservice_perf_level_t * perf_levels, uint32_t num_perf_levels,
        char *, uint32_t *);

    /***********************************************************************//**
    * GetEyeTrackingMode()
    * --------------------------------------------------------------------------
    * \param
    *    client:           [in] qvrservice_client_handle_t returned by Create().
    *    pCurrentMode:    [out] If non-NULL, will be set to the currently
    *                           configured eye tracking mode
    *    pSupportedModes: [out] If non-NULL, will be set to a bitmask
    *                           representing which eye tracking modes are
    *                           supported by the device.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    3 or higher
    * Timing requirements
    *    This function may be called at any time.
    * Notes
    *    See QVRSERVICE_EYE_TRACKING_MODE_XXX for more info.
    ***************************************************************************/
    int32_t (*GetEyeTrackingMode)(qvrservice_client_handle_t client,
        uint32_t *pCurrentMode, uint32_t *pSupportedModes);

    /***********************************************************************//**
    * SetEyeTrackingMode()
    * --------------------------------------------------------------------------
    * \param
    *    client:  [in] qvrservice_client_handle_t returned by Create().
    *    mode:    [in] Eye Tracking mode to set
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    3 or higher
    * Timing requirements
    *    This function must be called prior to calling StartVRMode().
    * Notes
    *    None
    ***************************************************************************/
    int32_t (*SetEyeTrackingMode)(qvrservice_client_handle_t client,
         uint32_t mode);

    /**********************************************************************//**
    * GetEyeTrackingData()
    * -------------------------------------------------------------------------
    * \param
    *    client:       [in] qvrservice_client_handle_t returned by Create().
    *    ppData:      [out] Address of pointer to qvrservice_eye_tracking_data_t
    *                       structure that will be updated to point to the pose
    *                       data within a ring buffer. See Notes for more info.
    *    timestampNs:  [in] The time in nanoseconds for the eye pose requested
    *                       (which must be a time in the past). This value must
    *                       be in the Tracker (Qtimer) time domain. A value of
    *                       0 requests the most recent pose.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    3 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *  - To minimize latency, a ring buffer is used to hold many poses, and a
    *    call to this function will set ppData to point to a single pose within
    *    that ring buffer. The buffer will be sized appropriately to provide at
    *    a minimum 0.5 seconds worth of poses. As such, the data pointed to by
    *    ppData may change after that amount of time passes.
    *  - Use QVRSERVICE_TRACKER_ANDROID_OFFSET_NS for converting between
    *    Tracker and Android time domains.
    **************************************************************************/
    int32_t (*GetEyeTrackingData)(qvrservice_client_handle_t client,
        qvrservice_eye_tracking_data_t **ppData, int64_t timestampNs);

    /***********************************************************************//**
    * ActivatePredictedHeadTrackingPoseElement()
    * --------------------------------------------------------------------------
    * \param
    *    client:        [in] qvrservice_client_handle_t returned by Create().
    *    element_id :[inout] element id activated on predicted pose ring buffer
    *                        if -1, then new element will be activated with
    *                        predicted pose and element_id will be updated,
    *                        otherwise, only forward prediction
    *                        delay will be updated for the requested element
    *    target_prediction_timestamp_ns:
    *                   [in] targetted prediction time in QTIMER based NS
    *                        resolution. This will be used to compute the amount
    *                        of forward prediction delay that needs to be applied.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    4 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *   This API allows the upper layer having a fixed buffer in the
    *   shared memory (indexed by the element_id)
    *   that is continuously updated with predictive pose for the target
    *   prediction delay. When current time reaches the target
    *   prediction timestamp, tracker stops updating the buffer.
    ***************************************************************************/
    int32_t (*ActivatePredictedHeadTrackingPoseElement)(qvrservice_client_handle_t client,
        int16_t* element_id, int64_t target_prediction_timestamp_ns);

    /***********************************************************************//**
    * SetTransformationMatrix()
    * --------------------------------------------------------------------------
    * \param
    *    client:[in] qvrservice_client_handle_t returned by Create().
    *    type:  [in] Transformation matrix type
    *    mat4x4:[in] Transformation matrix to be applied.
    *                For example, in late latching, transformation matrixes
    *                are applied as follows:
    *                QVRSERVICE_LATE_LATCHING_PRE_TRANSFORMATION_MAT
    *                * predicated_pose *
    *                QVRSERVICE_LATE_LATCHING_POST_TRANSFORMATION_MAT
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    4 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    None
    ***************************************************************************/
    int32_t (*SetTransformationMatrix)(qvrservice_client_handle_t client,
        QVRSERVICE_TRANSFORMATION_MATRIX_TYPE type, float mat4x4[16]);

    /**********************************************************************//**
    * GetHwTransforms()
    * -------------------------------------------------------------------------
    * \param
    *    client:          [in] qvrservice_client_handle_t returned by Create().
    *    pnTransforms: [inout] If transforms is NULL, pnTransforms will be
    *                          filled in with the number of transforms that may
    *                          be returned. If transforms is non-NULL, then
    *                          pnTransforms must describe how many elements the
    *                          transforms array can hold. pnTransforms must not
    *                          be NULL.
    *    transforms:     [out] Pointer to array to receive hardware transforms.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    4 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    None
    **************************************************************************/
    int32_t (*GetHwTransforms)(qvrservice_client_handle_t client,
        uint32_t *pNumTransforms, qvrservice_hw_transform_t transforms[]);

    /**********************************************************************//**
    * GetPluginDataHandle()
    * -------------------------------------------------------------------------
    * \param
    *    client:     [in] qvrservice_client_handle_t returned by Create().
    *    pluginName: [in] a valid QVRService plugin name defined by a constant
    *                     QVRSERVICE_PLUGIN_xxx
    * \return
    *    Returns a qvrplugin_data_t* if successful, NULL otherwise
    * API level
    *    4 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    The caller must be sure to call ReleasePluginDataHandle() when the
    *    plugin data is no longer needed. Failure to do so prior to calling
    *    Destroy() on the client may result in the client resources not being
    *    completely deallocated.
    *    Refer to QVRPluginData.h for a description of the functions
    *    supported by qvrplugin_data_t.
    **************************************************************************/
    qvrplugin_data_t* (*GetPluginDataHandle)(qvrservice_client_handle_t client,
        const char* pluginName);

    /**********************************************************************//**
    * ReleasePluginDataHandle()
    * -------------------------------------------------------------------------
    * \param
    *    client:     [in] qvrservice_client_handle_t returned by Create().
    *    handle:     [in] qvrplugin_data_t* returned by GetPluginDataHandle().
    * \return
    *    None
    * API level
    *    4 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *    None
    **************************************************************************/
    void (*ReleasePluginDataHandle)(qvrservice_client_handle_t client,
        qvrplugin_data_t* handle);

    /**********************************************************************//**
    * GetEyeTrackingDataWithFlags()
    * -------------------------------------------------------------------------
    * \param
    *    client:       [in] qvrservice_client_handle_t returned by Create().
    *    ppData:      [out] Address of pointer to qvrservice_eye_tracking_data_t
    *                       structure that will be updated to point to the pose
    *                       data within a ring buffer. See Notes for more info.
    *    timestampNs:  [in] The time in nanoseconds for the eye pose requested
    *                       (which must be a time in the past). This value must
    *                       be in the Tracker (Qtimer) time domain. A value of
    *                       0 requests the most recent pose.
    *    flags:        [in] Flags. See qvr_eye_tracking_data_flags_t for values.
    * \return
    *    Returns 0 upon success, -1 otherwise
    * API level
    *    5 or higher
    * Timing requirements
    *    None. This function may be called at any time.
    * Notes
    *  - To minimize latency, a ring buffer is used to hold many poses, and a
    *    call to this function will set ppData to point to a single pose within
    *    that ring buffer. The buffer will be sized appropriately to provide at
    *    a minimum 0.5 seconds worth of poses. As such, the data pointed to by
    *    ppData may change after that amount of time passes.
    *  - Use QVRSERVICE_TRACKER_ANDROID_OFFSET_NS for converting between
    *    Tracker and Android time domains.
    **************************************************************************/
    int32_t (*GetEyeTrackingDataWithFlags)(
        qvrservice_client_handle_t       client,
        qvrservice_eye_tracking_data_t **ppData,
        int64_t                          timestampNs,
        qvr_eye_tracking_data_flags_t    flags);

    /***************************************************************************//**
    * Returns a point cloud
    *
    * \param[in]
    *    me:          qvrservice_client_handle_t returned by Create()
    * \param[out]
    *    point_cloud: the point cloud
    * \return
    *    0 upon success, error code otherwise
    * \par API level
    *    5 or higher
    * \par Timing requirements
    *    None. This function may be called at any time.
    * \par Notes
    *    The caller must call ReleasePointCloud() when the
    *    point cloud data is no longer needed.
    **************************************************************************/
    int32_t (*GetPointCloud)(qvrservice_client_handle_t client,
        XrPointCloudQTI** point_cloud);

    /***************************************************************************//**
    * Returns a point cloud
    *
    * \param[in]
    *    me:          qvrservice_client_handle_t returned by Create()
    * \param[out]
    *    point_cloud: point cloud returned by GetPointCloud()
    * \return
    *    0 upon success, error code otherwise
    * \par API level
    *    5 or higher
    * \par Timing requirements
    *    None. This function may be called at any time.
    * \par Notes
    *    None
    **************************************************************************/
    int32_t (*ReleasePointCloud)(qvrservice_client_handle_t client,
        XrPointCloudQTI* point_cloud);

    //Reserved for future use
    void* reserved[64 - 18];

}qvrservice_client_ops_t;

typedef struct qvrservice_client{
    int api_version;
    qvrservice_client_ops_t* ops;
} qvrservice_client_t;

qvrservice_client_t* getQvrServiceClientInstance(void);


//helper
#define QVRSERVICE_CLIENT_LIB "libqvrservice_client.so"

typedef struct {
    void* libHandle;
    qvrservice_client_t* client;
    qvrservice_client_handle_t clientHandle;

} qvrservice_client_helper_t;


static inline qvrservice_client_helper_t* QVRServiceClient_Create()
{
    qvrservice_client_helper_t* me = (qvrservice_client_helper_t*)
                                    malloc(sizeof(qvrservice_client_helper_t));
    if(!me) return NULL;
    me->libHandle = dlopen( QVRSERVICE_CLIENT_LIB, RTLD_NOW);
    if (!me->libHandle) {
        free(me);
        return NULL;
    }

    typedef qvrservice_client_t* (*qvrservice_client_wrapper_fn)(void);
    qvrservice_client_wrapper_fn qvrServiceClient;

    qvrServiceClient = (qvrservice_client_wrapper_fn)dlsym(me->libHandle,
                                                "getQvrServiceClientInstance");
    if (!qvrServiceClient) {
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }
    me->client = qvrServiceClient();
    me->clientHandle = me->client->ops->Create();
    if(!me->clientHandle){
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }

    return me;
}

static inline void QVRServiceClient_Destroy(qvrservice_client_helper_t* me)
{
    if(!me) return;

    if (me->client->ops->Destroy){
        me->client->ops->Destroy( me->clientHandle);
    }

    if(me->libHandle ){
        dlclose(me->libHandle);
    }
    free(me);
    me = NULL;
}

static inline int32_t QVRServiceClient_SetClientStatusCallback(
   qvrservice_client_helper_t* helper, client_status_callback_fn cb, void *pCtx)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetClientStatusCallback) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->SetClientStatusCallback(
        helper->clientHandle, cb, pCtx);
}

static inline QVRSERVICE_VRMODE_STATE QVRServiceClient_GetVRMode(
    qvrservice_client_helper_t* helper)
{
    if(!helper) return VRMODE_UNSUPPORTED;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return VRMODE_UNSUPPORTED;
    if(helper && helper->client->ops->GetVRMode){
        return helper->client->ops->GetVRMode(helper->clientHandle);
    }
    return VRMODE_UNSUPPORTED;
}

static inline int32_t QVRServiceClient_StartVRMode(
    qvrservice_client_helper_t* helper)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->StartVRMode) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->StartVRMode(helper->clientHandle);
}

static inline int32_t QVRServiceClient_StopVRMode(
    qvrservice_client_helper_t* helper)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->StopVRMode) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->StopVRMode(helper->clientHandle);
}

static inline int32_t QVRServiceClient_GetTrackingMode(
    qvrservice_client_helper_t* helper,
    QVRSERVICE_TRACKING_MODE *pCurrentMode, uint32_t *pSupportedModes)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetTrackingMode) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->GetTrackingMode(
        helper->clientHandle, pCurrentMode, pSupportedModes);
}

static inline int32_t QVRServiceClient_SetTrackingMode(
    qvrservice_client_helper_t* helper, QVRSERVICE_TRACKING_MODE mode)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetTrackingMode) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->SetTrackingMode(helper->clientHandle, mode);
}

static inline int32_t QVRServiceClient_SetDisplayInterruptConfig(
    qvrservice_client_helper_t* helper, QVRSERVICE_DISP_INTERRUPT_ID id,
    void *pCfg, uint32_t cfgSize)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetDisplayInterruptConfig) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->SetDisplayInterruptConfig(
        helper->clientHandle, id, pCfg, cfgSize);
}

/*
*    This API is deprecated and is replaced by
*    SetThreadAttributesByType()
*/
static inline int32_t QVRServiceClient_SetThreadPriority(
    qvrservice_client_helper_t* helper, int tid, int policy, int priority)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetThreadPriority) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->SetThreadPriority(
        helper->clientHandle, tid, policy, priority);
}

static inline int32_t QVRServiceClient_GetParam(
    qvrservice_client_helper_t* helper, const char* pName, uint32_t* pLen,
    char* pValue)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetParam) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetParam(
        helper->clientHandle, pName, pLen, pValue);
}

static inline int32_t QVRServiceClient_SetParam(
    qvrservice_client_helper_t* helper, const char* pName, const char* pValue)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetParam) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->SetParam(
        helper->clientHandle, pName, pValue);
}

static inline int32_t QVRServiceClient_GetSensorRawData(
    qvrservice_client_helper_t* helper, qvrservice_sensor_data_raw_t **ppData)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetSensorRawData) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetSensorRawData(
        helper->clientHandle, ppData);
}

static inline int32_t QVRServiceClient_GetHeadTrackingData(
    qvrservice_client_helper_t* helper,
    qvrservice_head_tracking_data_t **ppData)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_1) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetHeadTrackingData) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetHeadTrackingData(
        helper->clientHandle, ppData);
}

static inline int32_t QVRServiceClient_GetRingBufferDescriptor(
    qvrservice_client_helper_t* helper, QVRSERVICE_RING_BUFFER_ID id,
    qvrservice_ring_buffer_desc_t *pDesc)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_2) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetRingBufferDescriptor) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetRingBufferDescriptor(
        helper->clientHandle, id, pDesc);
}

static inline int32_t QVRServiceClient_GetHistoricalHeadTrackingData(
    qvrservice_client_helper_t* helper,
    qvrservice_head_tracking_data_t **ppData, int64_t timestampNs)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_2) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetHistoricalHeadTrackingData) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetHistoricalHeadTrackingData(
        helper->clientHandle, ppData, timestampNs);
}

static inline int32_t QVRServiceClient_SetDisplayInterruptCapture(
    qvrservice_client_helper_t* helper, QVRSERVICE_DISP_INTERRUPT_ID id,
    uint32_t mode)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_2) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetDisplayInterruptCapture) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->SetDisplayInterruptCapture(
        helper->clientHandle, id, mode);
}

static inline int32_t QVRServiceClient_GetDisplayInterruptTimestamp(
    qvrservice_client_helper_t* helper, QVRSERVICE_DISP_INTERRUPT_ID id,
    qvrservice_ts_t** ppTs)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_2) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetDisplayInterruptTimestamp) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetDisplayInterruptTimestamp(
        helper->clientHandle, id, ppTs);
}

static inline int32_t QVRServiceClient_RegisterForNotification(
    qvrservice_client_helper_t* helper, QVRSERVICE_CLIENT_NOTIFICATION notification,
    notification_callback_fn cb, void *pCtx)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_3) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->RegisterForNotification) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->RegisterForNotification(
        helper->clientHandle, notification, cb, pCtx);
}

static inline int32_t QVRServiceClient_SetThreadAttributesByType(
    qvrservice_client_helper_t* helper, int tid, QVRSERVICE_THREAD_TYPE thread_type)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_3) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetThreadAttributesByType) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->SetThreadAttributesByType(
        helper->clientHandle, tid, thread_type);
}

static inline int32_t QVRServiceClient_SetOperatingLevel(
    qvrservice_client_helper_t* helper, qvrservice_perf_level_t * perf_levels,
    uint32_t num_perf_levels, char * perf_token, uint32_t * ptoken_len)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_3) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetOperatingLevel) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->SetOperatingLevel(
        helper->clientHandle, perf_levels, num_perf_levels, perf_token, ptoken_len);
}

static inline int32_t QVRServiceClient_GetEyeTrackingMode(
    qvrservice_client_helper_t* helper,
    uint32_t *pCurrentMode, uint32_t *pSupportedModes)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_3) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetEyeTrackingMode) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetEyeTrackingMode(
        helper->clientHandle, pCurrentMode, pSupportedModes);
}

static inline int32_t QVRServiceClient_SetEyeTrackingMode(
    qvrservice_client_helper_t* helper, uint32_t mode)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_3) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetEyeTrackingMode) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->SetEyeTrackingMode(helper->clientHandle, mode);
}

static inline int32_t QVRServiceClient_GetEyeTrackingData(
    qvrservice_client_helper_t      *helper,
    qvrservice_eye_tracking_data_t **ppData,
    int64_t                          timestampNs)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_3) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetEyeTrackingData) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetEyeTrackingData(
        helper->clientHandle, ppData, timestampNs);
}

static inline int32_t QVRServiceClient_GetEyeTrackingDataWithFlags(
    qvrservice_client_helper_t      *helper,
    qvrservice_eye_tracking_data_t **ppData,
    int64_t                          timestampNs,
    qvr_eye_tracking_data_flags_t    flags)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_5) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetEyeTrackingData) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetEyeTrackingDataWithFlags(
        helper->clientHandle, ppData, timestampNs, flags);
}

static inline int32_t QVRServiceClient_ActivatePredictedHeadTrackingPoseElement(
    qvrservice_client_helper_t* helper, int16_t* element_id, int64_t target_prediction_timestamp_ns)
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_4) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->ActivatePredictedHeadTrackingPoseElement) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->ActivatePredictedHeadTrackingPoseElement(
        helper->clientHandle, element_id, target_prediction_timestamp_ns);
}

static inline int32_t QVRServiceClient_SetTransformationMatrix(
    qvrservice_client_helper_t* helper, QVRSERVICE_TRANSFORMATION_MATRIX_TYPE type, float mat4x4[16])
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_4) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->SetTransformationMatrix) return QVR_API_NOT_SUPPORTED;

    return helper->client->ops->SetTransformationMatrix(
        helper->clientHandle, type, mat4x4);
}

static inline int32_t QVRServiceClient_GetHwTransforms(
    qvrservice_client_helper_t* helper,
    uint32_t *pnTransforms, qvrservice_hw_transform_t transforms[])
{
    if(!helper) return QVR_INVALID_PARAM;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_4) return QVR_API_NOT_SUPPORTED;
    if(!helper->client->ops->GetHwTransforms) return QVR_API_NOT_SUPPORTED;
    return helper->client->ops->GetHwTransforms(
        helper->clientHandle, pnTransforms, transforms);
}

// helper function to get a specific hardware transform. Caller is to provide
// a qvrservice_hw_transform_t with the 'from' and 'to' members set to the
// desired values. if successful, the transformation matrix will be filled
// in, otherwise an error is returned: QVR_ERROR indicates out of memory,
// QVR_INVALID_PARAM means the transform was not found.
static inline int32_t QVRServiceClient_GetHwTransform(
    qvrservice_client_helper_t* helper, qvrservice_hw_transform_t *transform)
{
    qvrservice_hw_transform_t *t;
    uint32_t nt;
    int ret;
    if (NULL == transform) return QVR_INVALID_PARAM;
    ret = QVRServiceClient_GetHwTransforms(helper, &nt, NULL);
    if (ret < 0) return ret;
    t = (qvrservice_hw_transform_t*) malloc(nt * sizeof(*t));
    if (NULL == t) return QVR_ERROR;
    if (0 == QVRServiceClient_GetHwTransforms(helper, &nt, t)) {
        qvrservice_hw_transform_t* pt = t;
        for (uint32_t i=0; i<nt; i++, pt++) {
            if (transform->from == pt->from && transform->to == pt->to) {
                memcpy(transform->m, pt->m, sizeof(pt->m));
                free(t);
                return QVR_SUCCESS;
            }
        }
    }
    free(t);
    return QVR_INVALID_PARAM;
}

static inline qvrplugin_data_t* QVRServiceClient_GetPluginDataHandle(
    qvrservice_client_helper_t* helper, const char* pluginName)
{
    if(!helper) return NULL;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_4) return NULL;
    if(!helper->client->ops->GetPluginDataHandle) return NULL;
    return helper->client->ops->GetPluginDataHandle(
        helper->clientHandle, pluginName);
}

static inline void QVRServiceClient_ReleasePluginDataHandle(
    qvrservice_client_helper_t* helper, qvrplugin_data_t* handle)
{
    if(!helper) return;
    if (helper->client->api_version < QVRSERVICECLIENT_API_VERSION_4) return;
    if(!helper->client->ops->ReleasePluginDataHandle) return;
    return helper->client->ops->ReleasePluginDataHandle(
        helper->clientHandle, handle);
}

static inline int32_t QVRServiceClient_GetPointCloud_Exp(
    qvrservice_client_helper_t* me, XrPointCloudQTI** point_cloud)
{
    if (!me) return QVR_INVALID_PARAM;
    if (me->client->api_version < QVRSERVICECLIENT_API_VERSION_5) return QVR_API_NOT_SUPPORTED;
    if (!me->client->ops->GetPointCloud) return QVR_API_NOT_SUPPORTED;
    return me->client->ops->GetPointCloud(me->clientHandle, point_cloud);
}

static inline int32_t QVRServiceClient_ReleasePointCloud_Exp(
    qvrservice_client_helper_t* me, XrPointCloudQTI* point_cloud)
{
    if (!me) return QVR_INVALID_PARAM;
    if (me->client->api_version < QVRSERVICECLIENT_API_VERSION_5) return QVR_API_NOT_SUPPORTED;
    if (!me->client->ops->ReleasePointCloud) return QVR_API_NOT_SUPPORTED;
    return me->client->ops->ReleasePointCloud(me->clientHandle, point_cloud);
}

#ifdef __cplusplus
}
#endif

#endif /* QVRSERVICE_CLIENT_H */
