/******************************************************************************/
/*! \file  QVRTypes.h */
/*
* Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
* All Rights Reserved
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/
#ifndef QVRTYPES_H
#define QVRTYPES_H

/**************************************************************************//**
* Error codes
******************************************************************************/
#define QVR_SUCCESS                    0
#define QVR_ERROR                      -1
#define QVR_CALLBACK_NOT_SUPPORTED     -2
#define QVR_API_NOT_SUPPORTED          -3
#define QVR_INVALID_PARAM              -4

/**************************************************************************//**
* qvrservice_head_tracking_data_t
* -----------------------------------------------------------------------------
* This structure contains a quaternion (x,y,z,w) representing rotational pose
* and position vector representing translational pose in the Android Portrait
* coordinate system. ts is the timestamp of
* the corresponding sensor value originating from the sensor stack.
* The cofficients may be used for computing a forward-predicted pose.
* tracking_state contains the state of 6DOF tracking.
*   bit 0     : RELOCATION_IN_PROGRESS
*   bits 1-15 : reserved
* tracking_warning_flags contains the 6DOF tracking warnings
*   bit 0     : LOW_FEATURE_COUNT_ERROR
*   bit 1     : LOW_LIGHT_ERROR
*   bit 2     : BRIGHT_LIGHT_ERROR
*   bit 3     : STEREO_CAMERA_CALIBRATION_ERROR
*   bits 4-15 : reserved
* flags_3dof_mag_used bit will be set only when TRACKING_MODE_ROTATIONAL_MAG
* mode is used. flags_3dof_mag_calibrated will be set only when mag data
* is calibrated. If the flag is not set, user needs to be notified to move
* the device for calibrating mag sensor (i.e. figure 8 movement)
* pose_quality, sensor_quality and camera_quality fields are filled only
* for TRACKING_MODE_POSITIONAL mode. They are scaled from 0.0 (bad) to 1.0
* (good).
******************************************************************************/
typedef struct {
    float rotation[4];
    float translation[3];
    uint32_t reserved;
    uint64_t ts;
    uint64_t reserved1;
    float prediction_coff_s[3];
    uint32_t reserved2;
    float prediction_coff_b[3];
    uint32_t reserved3;
    float prediction_coff_bdt[3];
    uint32_t reserved4;
    float prediction_coff_bdt2[3];
    uint16_t tracking_state;
    uint16_t tracking_warning_flags;
    uint32_t flags_3dof_mag_used : 1;
    uint32_t flags_3dof_mag_calibrated : 1;
    float pose_quality;
    float sensor_quality;
    float camera_quality;
    float prediction_coff_ts[3];
    uint32_t reserved6;
    float prediction_coff_tb[3];
    uint32_t reserved7;
    uint8_t reserved8[32];
} qvrservice_head_tracking_data_t;


/**************************************************************************//**
* QVRSERVICE_LATE_LATCHING_SLOT_STATUS
* -----------------------------------------------------------------------------
* QVRSERVICE_LATE_LATCHING_SLOT_UNUSED
*     Indicates the slot is unused and not being tracked
* QVRSERVICE_LATE_LATCHING_SLOT_INITIATED
*     Indicates the slot has been alocated, but not being tracked
* QVRSERVICE_LATE_LATCHING_SLOT_TRACKED
*     Indicates the slot is being tracked by tracker
******************************************************************************/
typedef enum{
    QVRSERVICE_LATE_LATCHING_SLOT_UNUSED = 0,
    QVRSERVICE_LATE_LATCHING_SLOT_INITIATED,
    QVRSERVICE_LATE_LATCHING_SLOT_TRACKED,
} QVRSERVICE_LATE_LATCHING_SLOT_STATUS;


/**************************************************************************//**
* qvr_eye_tracking_data_flags_t
* -----------------------------------------------------------------------------
* QVR_EYE_TRACKING_DATA_ENABLE_SYNC
*     When set, timing-sync will measure GetEyeTrackingDataWithFlags() call
*     frequency and adjust the timing of eye pose generation to minimize
*     latency. Calls must be made at regular intervals (i.e. ~30/45/60 hz).
******************************************************************************/
typedef uint64_t qvr_eye_tracking_data_flags_t;

static const qvr_eye_tracking_data_flags_t QVR_EYE_TRACKING_DATA_ENABLE_SYNC  = 0x00000001;

/**************************************************************************//**
* qvrservice_predicted_head_tracking_data_t
* -----------------------------------------------------------------------------
* This structure contains matrices (4x4 floats) representing predictive 6DOF
* head pos. prediction_target_ns is specified by
* ActivatePredictedHeadTrackingPoseElement() API and used to compute
* forward_pred_delay_ms (i.e. prediction_target_ns - now).
* last_update_ts_ns represents timestamp in NS resolution (QTIMER base) when
* the struct is updated. slot_status represents one of the values from
* QVRSERVICE_LATE_LATCHING_SLOT_STATUS and indicates whether this pose is being
* updated or not by the tracker. originalPredictedPoseMat44F is the initial
* predictive pose when an element is activated by calling
* ActivatePredictedHeadTrackingPoseElement() API.  curPredictedPoseMat44F is the
* subsequent updated predictive head pose over time and the applied forward
* prediction delay is specified in forward_pred_delay_ms.
* start and end values can be utilized to ensure atomicity of the struct.
* For instance, consumer should make a deep copy of the content and check
* if start and end value are equal or not. If not equal, then it should be
* discarded and pose should be copied again from shared memory.
******************************************************************************/
typedef struct {

    uint64_t start;
    uint64_t prediction_target_ns;
    uint64_t last_update_ts_ns;
    uint32_t forward_pred_delay_ms;
    uint32_t slot_status;
    float    originalPredictedPoseMat44F[16];
    float    curPredictedPoseMat44F[16];
    uint8_t  reserved[24];
    uint64_t end;
} qvrservice_predicted_head_tracking_data_t;

/**************************************************************************//**
* QVRSERVICE_RING_BUFFER_ID
* -----------------------------------------------------------------------------
*   RING_BUFFER_POSE
*      ID to indicate the ring buffer for the pose data in shared memory.
*      This ring buffer contains qvrservice_head_tracking_data_t elements.
*   RING_BUFFER_EYE_POSE
*      ID to indicate the ring buffer for the eye pose data in shared memory.
*      This ring buffer contains qvrservice_eye_tracking_data_t elements.
*   RING_BUFFER_PREDICTED_HEAD_POSE
*      ID to indicate the ring buffer for the predicted pose data in shared
*      memory. This ring buffer contains
*      qvrservice_predicted_head_tracking_data_t elements.
*      These elements are activated by calling
*      ActivatePredictedHeadTrackingPoseElement API.
******************************************************************************/
typedef enum {
    RING_BUFFER_POSE=1,
    RING_BUFFER_EYE_POSE,
    RING_BUFFER_PREDICTED_HEAD_POSE,
    RING_BUFFER_MAX
} QVRSERVICE_RING_BUFFER_ID;

/**************************************************************************//**
* qvrservice_ring_buffer_desc_t
* -----------------------------------------------------------------------------
* This structure describes the ring buffer attributes for the provided ring
* buffer ID (see QVRSERVICE_RING_BUFFER_ID).
*   fd:            File descriptor of shared memory block
*   size:          Size in bytes of shared memory block
*   index_offset:  Offset in bytes to the ring index. The index is a 4-byte
*                  integer.
*   ring_offset:   Offset in bytes to the first element of the ring buffer.
*   element_size:  Size in bytes of each element in the ring buffer. This value
*                  should match the structure size for a given ring buffer ID.
*   num_elements:  Total number of elements in the ring buffer
*   reserved:      Reserved for future use
******************************************************************************/
typedef struct {
    int32_t fd;
    uint32_t size;
    uint32_t index_offset;
    uint32_t ring_offset;
    uint32_t element_size;
    uint32_t num_elements;
    uint32_t reserved[2];
} qvrservice_ring_buffer_desc_t;

/**************************************************************************//**
* QVRSERVICE_HW_COMP_ID
* -----------------------------------------------------------------------------
*   QVRSERVICE_HW_COMP_ID_HMD
*      Virtual HMD reference point
*   QVRSERVICE_HW_COMP_ID_IMU
*      IMU
*   QVRSERVICE_HW_COMP_ID_EYE_TRACKING_CAM_L
*      Left eye tracking camera
*   QVRSERVICE_HW_COMP_ID_EYE_TRACKING_CAM_R
*      Right eye tracking camera
******************************************************************************/
typedef enum {
    QVRSERVICE_HW_COMP_ID_INVALID,
    QVRSERVICE_HW_COMP_ID_HMD,
    QVRSERVICE_HW_COMP_ID_IMU,
    QVRSERVICE_HW_COMP_ID_EYE_TRACKING_CAM_L,
    QVRSERVICE_HW_COMP_ID_EYE_TRACKING_CAM_R,
    QVRSERVICE_HW_COMP_ID_MAX
} QVRSERVICE_HW_COMP_ID;

/**************************************************************************//**
* qvrservice_hw_transform_t
* -----------------------------------------------------------------------------
* Description
*   Used to get/set hardware transforms.
*   .m is a 4x4 row major float matrix that represents the spatial transform
*   between hardware components represented by the .from and .to members.
*   .from and .to members must be set to a valid QVRSERVICE_HW_COMP_ID
*   value.
******************************************************************************/
typedef struct _qvrservice_hw_transform_t {
    int from;
    int to;
    float m[16];
} qvrservice_hw_transform_t;


/**************************************************************************//**
* qvr eye-tracking data section
******************************************************************************/
#define QVR_MAX_VIEWPORTS 6

/**************************************************************************//**
* qvrservice_eye_id
* -----------------------------------------------------------------------------
*   QVR_EYE_LEFT
*      Left eye index
*   QVR_EYE_RIGHT
*      Right eye index
******************************************************************************/
typedef enum {
    QVR_EYE_LEFT,
    QVR_EYE_RIGHT,
    QVR_EYE_MAX,
} qvrservice_eye_id;

/**************************************************************************//**
* qvr_gaze_flags_t
* -----------------------------------------------------------------------------
* Description
*   type representing a bitmask used by the plugin to convey
*   the availability of the combined eye gaze data.
*   A valid value for a variable of type qvr_gaze_flags_t is
*   either zero or a bitwise OR of the individual valid flags shown below:
*   - QVR_GAZE_ORIGIN_COMBINED_VALID
*   - QVR_GAZE_DIRECTION_COMBINED_VALID
*   - QVR_GAZE_CONVERGENCE_DISTANCE_VALID
******************************************************************************/
typedef uint64_t qvr_gaze_flags_t;

// Flag bits for qvr_gaze_flags_t
static const qvr_gaze_flags_t QVR_GAZE_ORIGIN_COMBINED_VALID = 0x00000001;
static const qvr_gaze_flags_t QVR_GAZE_DIRECTION_COMBINED_VALID = 0x00000002;
static const qvr_gaze_flags_t QVR_GAZE_CONVERGENCE_DISTANCE_VALID = 0x00000004;

/**************************************************************************//**
* qvr_gaze_per_eye_flags_t
* -----------------------------------------------------------------------------
* Description
*   type representing a bitmask used by the plugin to convey
*   the availability of the per-eye gaze data.
*   A valid value for a variable of type qvr_gaze_per_eye_flags_t is
*   either zero or a bitwise OR of the individual valid flags shown below:
*   - QVR_GAZE_PER_EYE_GAZE_ORIGIN_VALID
*   - QVR_GAZE_PER_EYE_GAZE_DIRECTION_VALID
*   - QVR_GAZE_PER_EYE_GAZE_POINT_VALID
*   - QVR_GAZE_PER_EYE_EYE_OPENNESS_VALID
*   - QVR_GAZE_PER_EYE_PUPIL_DILATION_VALID
*   - QVR_GAZE_PER_EYE_POSITION_GUIDE_VALID
******************************************************************************/
typedef uint64_t qvr_gaze_per_eye_flags_t;

// Flag bits for qvr_gaze_per_eye_flags_t
static const qvr_gaze_per_eye_flags_t QVR_GAZE_PER_EYE_GAZE_ORIGIN_VALID = 0x00000001;
static const qvr_gaze_per_eye_flags_t QVR_GAZE_PER_EYE_GAZE_DIRECTION_VALID = 0x00000002;
static const qvr_gaze_per_eye_flags_t QVR_GAZE_PER_EYE_GAZE_POINT_VALID = 0x00000004;
static const qvr_gaze_per_eye_flags_t QVR_GAZE_PER_EYE_EYE_OPENNESS_VALID = 0x00000008;
static const qvr_gaze_per_eye_flags_t QVR_GAZE_PER_EYE_PUPIL_DILATION_VALID = 0x00000010;
static const qvr_gaze_per_eye_flags_t QVR_GAZE_PER_EYE_POSITION_GUIDE_VALID = 0x00000020;

/**************************************************************************//**
* qvrservice_gaze_per_eye_t
* -----------------------------------------------------------------------------
* Description
*   used to report the per-eye eye-tracking information
*   .flags qvr_gaze_per_eye_flags_t validity bitmask for the per-eye
*      eye gaze attributes
*   .gazeOrigin[3] contains the origin (x, y, z) of the eye
*      gaze vector in meters from the HMD center-eye coordinate system's origin.
*   .gazeDirection[3] contains the unit vector of the eye
*      gaze direction in the HMD center-eye coordinate system.
*   .viewport2DGazePoint[QVR_MAX_VIEWPORTS][2] for each supported viewport,
*      an unsigned normalized 2D gaze point where origin is top left
*      of the view port.
*   .eyeOpenness a value between 0.0 and 1.0 where 1.0 means fully open
*      and 0.0 closed.
*   .pupilDilation a value in millimeter indicating the pupil dilation.
*   .positionGuide[3] contains the position of the inner corner of the eye
*      in meters from the HMD center-eye coordinate system's origin.
******************************************************************************/
typedef struct {
    qvr_gaze_per_eye_flags_t flags;
    uint32_t                 reserved1[2];
    float                    gazeOrigin[3];
    uint32_t                 reserved2;
    float                    gazeDirection[3];
    uint32_t                 reserved3;
    float                    viewport2DGazePoint[QVR_MAX_VIEWPORTS][2];
    float                    eyeOpenness;
    float                    pupilDilation;
    uint32_t                 reserved4[2];
    float                    positionGuide[3];
    uint32_t                 reserved5;
    uint8_t                  reserved[256-23*sizeof(float)-sizeof(uint64_t)
                                      -7*sizeof(uint32_t)];
} qvrservice_gaze_per_eye_t;

/**************************************************************************//**
* qvr_timing_flags_t
* -----------------------------------------------------------------------------
* Description
*   type representing a bitmask used by the plugin to convey
*   infomation about the frame data used
*   either zero or a bitwise OR of the individual valid flags shown below:
*   - QVR_TIMING_CAMERA_DATA_VALID
******************************************************************************/
typedef uint64_t qvr_timing_flags_t;

// Flag bits for qvr_gaze_per_eye_flags_t
static const qvr_timing_flags_t QVR_TIMING_CAMERA_DATA_VALID = 0x00000001;

/**************************************************************************//**
* qvrservice_timing_t
* -----------------------------------------------------------------------------
* Description
*   used to report eye-tracking data source
******************************************************************************/
typedef union {
    struct {
        uint32_t frameNumber;
        uint32_t exposureNs;
        uint64_t startOfExposureNs; // in ns using Android BOOTTIME clock
    } camera;
} qvrservice_timing_t;

/**************************************************************************//**
* qvrservice_eye_tracking_data_t
* -----------------------------------------------------------------------------
* Description
*   used to report the combined eye-tracking information
*   .timestamp in nanoseconds using Android BOOTTIME clock
*   .flags qvr_gaze_flags_t validity bitmask for the combined
*      eye gaze attributes
*   .eye[2] array of 2 qvrservice_gaze_per_eye_t structures,
*      representing the left eye then right eye, per-eye gaze information
*   .gazeOriginCombined[3] contains the origin (x, y, z) of the combined
*      gaze vector in meters from the HMD center-eye coordinate system's origin.
*   .gazeDirectionCombined[3] contains the unit vector of the combined
*      gaze direction in the HMD center-eye coordinate system.
*   .gazeConvergenceDistance the distance in meters from gazeOriginComined
*      where the vectors converge.
*   .timingFlags indicates if timing info is valid and the source of the info
*   .timing data source timing information (ex: camera exposure and timestamp)
******************************************************************************/
typedef struct {
    int64_t                   timestamp;  // in ns using Android BOOTTIME clock
    qvr_gaze_flags_t          flags;
    qvrservice_gaze_per_eye_t eye[QVR_EYE_MAX];
    float                     gazeOriginCombined[3];
    float                     reserved1;
    float                     gazeDirectionCombined[3];
    float                     gazeConvergenceDistance;
    qvr_timing_flags_t        timingFlags;
    qvrservice_timing_t       timing;
    // NOTE: struct is 64-bit aligned, add fields accordingly.
    uint8_t                   reserved[ 640
                                       -sizeof(uint64_t)
                                       -sizeof(qvr_gaze_flags_t)
                                       -sizeof(qvrservice_gaze_per_eye_t) * QVR_EYE_MAX
                                       -sizeof(float) * 8
                                       -sizeof(qvr_timing_flags_t)
                                       -sizeof(qvrservice_timing_t)];
} qvrservice_eye_tracking_data_t;

/**************************************************************************//**
* qvr_plugin_info_t
* -----------------------------------------------------------------------------
* This structure contains plugin information:
* .pluginVendor is a NUL-terminated string identifying the plugin vendor
* .pluginVersion is an integer identifying the plugin version
* .pluginDataVersion is an integer identifying the pluginData version, most
*   useful for the plugin to convey to its clients any evolution of the data
*   format or control/payload protocol.
******************************************************************************/
typedef struct {
    char               pluginVendor[256];
    uint32_t           pluginVersion;
    uint32_t           pluginDataVersion;
} qvr_plugin_info_t;

/**************************************************************************//**
* qvr_plugin_param_t
* -----------------------------------------------------------------------------
* Description
*   Used to set parameters.
******************************************************************************/
typedef struct _qvr_plugin_param {
    const char* name;
    const char* val;
} qvr_plugin_param_t;

#endif /* QVRTYPES_H */
