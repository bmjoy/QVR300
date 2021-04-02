#=============================================================================
#                  Copyright (c) 2016 QUALCOMM Technologies Inc.
#                              All Rights Reserved.
#
#=============================================================================
LOCAL_PATH := $(call my-dir)
MY_PATH := $(LOCAL_PATH)
  
include $(CLEAR_VARS)
LOCAL_MODULE := libsvrapi

LOCAL_SRC_FILES := $(MY_PATH)/../.svrLibs/$(APP_OPTIM)/jni/$(TARGET_ARCH_ABI)/libsvrapi.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

SDK_ROOT_PATH := $(LOCAL_PATH)/../../..

SVR_SVRAPI_PATH = : $(SDK_ROOT_PATH)/svrApi
VULKAN_PATH := $(SDK_ROOT_PATH)/3rdparty
GLM_ROOT_PATH := $(SDK_ROOT_PATH)/3rdparty/glm-0.9.7.0
# TINYOBJ_ROOT_PATH := $(SDK_ROOT_PATH)/3rdparty/tinyobj
SVR_API_ROOT_PATH := $(SDK_ROOT_PATH)/svrApi/public
SVR_FRAMEWORK_PATH = : $(SDK_ROOT_PATH)/framework
VULKAN_FRAMEWORK_PATH = : $(LOCAL_PATH)/framework

LOCAL_MODULE    := vulkantest
LOCAL_ARM_MODE	:= arm

#LOCAL_C_INCLUDES  := $(VULKAN_PATH) $(GLM_ROOT_PATH) $(SVR_SVRAPI_PATH) $(TINYOBJ_ROOT_PATH) $(SVR_API_ROOT_PATH) $(FRAMEWORK_PATH) 
LOCAL_C_INCLUDES  := $(VULKAN_PATH) $(GLM_ROOT_PATH) $(SVR_SVRAPI_PATH) $(SVR_API_ROOT_PATH) $(VULKAN_FRAMEWORK_PATH) $(SVR_FRAMEWORK_PATH)

LOCAL_SRC_FILES += ./framework/BufferObject.cpp
LOCAL_SRC_FILES += ./framework/MemoryAllocator.cpp
LOCAL_SRC_FILES += ./framework/MeshObject.cpp
LOCAL_SRC_FILES += ./framework/TextureObject.cpp
LOCAL_SRC_FILES += ./framework/VkSampleFramework.cpp
LOCAL_SRC_FILES += sample.cpp

LOCAL_LDLIBS    := -llog -landroid -lvulkan
LOCAL_SHARED_LIBRARIES := libsvrapi

LOCAL_WHOLE_STATIC_LIBRARIES += android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
