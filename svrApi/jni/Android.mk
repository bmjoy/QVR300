#=============================================================================
#                  Copyright (c) 2016 QUALCOMM Technologies Inc.
#                              All Rights Reserved.
#
#=============================================================================

LOCAL_PATH := $(call my-dir)

# Use this to enable/disable Motion To Photon Test
ENABLE_MOTION_PHOTON_TEST := true

# Use this to enable/disable Telemetry profiling support
ENABLE_TELEMETRY := false

# Use this to enable/disable ATrace profiling support
ENABLE_ATRACE := true

# Use this to enable/disable Motion Vectors
ENABLE_MOTION_VECTORS := true

# Use this to enable/disable debug with property
ALLOW_DEBUG_WITH_PROPERTY := false

ifeq ($(ENABLE_TELEMETRY),true)
include $(CLEAR_VARS)
LOCAL_MODULE    := libTelemetry
LOCAL_SRC_FILES := ../../3rdparty/telemetry/lib/rad_tm_android_arm.a
include $(PREBUILT_STATIC_LIBRARY)
endif

ifeq ($(ENABLE_MOTION_VECTORS),true)
include $(CLEAR_VARS)
LOCAL_MODULE    := libStaticMotionVector
LOCAL_SRC_FILES := ../../3rdparty/motionengine/lib/$(TARGET_ARCH_ABI)/libMotionEngine.a
include $(PREBUILT_STATIC_LIBRARY)
endif

SDK_ROOT_PATH := $(LOCAL_PATH)/../..

SVR_FRAMEWORK_PATH		:= $(SDK_ROOT_PATH)/framework
GLM_ROOT_PATH			:= $(SDK_ROOT_PATH)/3rdparty/glm-0.9.7.0
TINYOBJ_ROOT_PATH		:= $(SDK_ROOT_PATH)/3rdparty/tinyobj
CJSON_ROOT_PATH			:= $(SDK_ROOT_PATH)/3rdparty/cJSON
SVR_API_PUBLIC_PATH		:= $(SDK_ROOT_PATH)/svrApi/public
SVR_API_PRIVATE_PATH	:= $(SDK_ROOT_PATH)/svrApi
SVR_API_SVRCONTROLLER_PATH := $(SVR_API_PRIVATE_PATH)/private/ControllerManager/inc/
SVR_API_SVRCONTROLLER_UTIL_PATH := $(SVR_API_PRIVATE_PATH)/private/SvrControllerUtil/jni/inc/
SVR_API_SVRSERVICECLIENT_PATH := $(SVR_API_PRIVATE_PATH)/private/SvrServiceClient/inc/
TELEMETRY_ROOT_PATH		:= $(SDK_ROOT_PATH)/3rdparty/telemetry
QCOM_ADSP_ROOT_PATH		:= $(SDK_ROOT_PATH)/3rdparty/qcom_adsp
QCOM_QVR_SERVICE_PATH	:= $(SDK_ROOT_PATH)/3rdparty/qvr

ifeq ($(ENABLE_MOTION_VECTORS),true)
MOTION_ENGINE_PATH	    := $(SDK_ROOT_PATH)/3rdparty/motionengine
endif

# willie change start 2020-8-20
MY_SC_MEDIATOR_DIR    := $(SDK_ROOT_PATH)/3rdparty/scmediator
# willie change end 2020-8-20


include $(CLEAR_VARS)

ifeq ($(ENABLE_MOTION_PHOTON_TEST),true)
	LOCAL_CFLAGS += -DMOTION_TO_PHOTON_TEST
endif

ifeq ($(ENABLE_TELEMETRY),true)
	LOCAL_CFLAGS += -DSVR_PROFILING_ENABLED
	LOCAL_CFLAGS += -DSVR_PROFILE_TELEMETRY
endif 

ifeq ($(ENABLE_ATRACE),true)
	LOCAL_CFLAGS += -DSVR_PROFILING_ENABLED
	LOCAL_CFLAGS += -DSVR_PROFILE_ATRACE
endif 

ifeq ($(ENABLE_MOTION_VECTORS),true)
	LOCAL_CFLAGS += -DENABLE_MOTION_VECTORS
endif

ifeq ($(ALLOW_DEBUG_WITH_PROPERTY),true)
	LOCAL_CFLAGS += -DDEBUG_WITH_PROPERTY
endif

LOCAL_MODULE := svrapi
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(GLM_ROOT_PATH) $(TINYOBJ_ROOT_PATH) $(CJSON_ROOT_PATH) $(SVR_FRAMEWORK_PATH) $(SVR_API_PUBLIC_PATH) $(SVR_API_PRIVATE_PATH) $(TELEMETRY_ROOT_PATH)/include $(QCOM_ADSP_ROOT_PATH)/include $(QCOM_QVR_SERVICE_PATH)/inc $(SVR_API_SVRCONTROLLER_UTIL_PATH) $(SVR_API_SVRCONTROLLER_PATH) $(SVR_API_SVRSERVICECLIENT_PATH)

# willie change start 2020-8-20
LOCAL_C_INCLUDES += $(MY_SC_MEDIATOR_DIR)
# willie change end 2020-8-20

ifeq ($(ENABLE_MOTION_VECTORS),true)
LOCAL_C_INCLUDES += $(MOTION_ENGINE_PATH)/inc
endif

#Make sure svrApiVersion is compiled every time
FILE_LIST := $(wildcard $(LOCAL_PATH)/../private/*.cpp)

FILE_LIST += ../../framework/svrConfig.cpp ../../framework/svrContainers.cpp
FILE_LIST += ../../framework/svrGeometry.cpp ../../framework/svrShader.cpp ../../framework/svrUtil.cpp ../../framework/svrRenderTarget.cpp
FILE_LIST += ../../framework/svrKtxLoader.cpp ../../framework/svrProfile.cpp
FILE_LIST += ../../framework/svrCpuTimer.cpp ../../framework/svrGpuTimer.cpp
FILE_LIST += ../../framework/svrRenderExt.cpp

FILE_LIST += ../../3rdparty/tinyobj/tiny_obj_loader.cc
FILE_LIST += ../../3rdparty/cJSON/cJSON.c

FILE_LIST += $(wildcard $(LOCAL_PATH)/../private/SvrControllerUtil/jni/src/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/../private/ControllerManager/src/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/../private/SvrServiceClient/src/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/../private/Invision/src/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/../private/utils/*.cpp)

LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

FILE_LIST               := $(wildcard $(LOCAL_PATH)/../../invision/*.cpp)
FILE_LIST               += $(wildcard $(LOCAL_PATH)/../../invision/slam/*.cpp)
FILE_LIST               += $(wildcard $(LOCAL_PATH)/../../invision/unitylauncher/*.cpp)
LOCAL_SRC_FILES         += $(FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../../invision
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../../invision/slam
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../../invision/gesture
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../../invision/unitylauncher
LOCAL_C_INCLUDES        += $(wildcard $(LOCAL_PATH)/../private/Invision/inc/*.h)

LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../../3rdparty/rapidxml

#$(warning $(LOCAL_SRC_FILES))
.PHONY: $(LOCAL_PATH)/../private/svrApiVersion.cpp

LOCAL_CPPFLAGS += -Wall -fno-strict-aliasing -Wno-unused-but-set-variable

LOCAL_STATIC_LIBRARIES := Telemetry cpufeatures qvrservice_client

ifeq ($(ENABLE_MOTION_VECTORS),true)
LOCAL_STATIC_LIBRARIES += libStaticMotionVector
endif

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3

APP_ALLOW_MISSING_DEPS=true

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)







