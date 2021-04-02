# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)
SVR_ROOT_PATH := $(LOCAL_PATH)/../../..
SVR_FRAMEWORK_SRC_PATH := $(SVR_ROOT_PATH)/samples/SimpleVR/jni

APP_COMMON_PATH := $(LOCAL_PATH)/../../common

include $(APP_COMMON_PATH)/SvrApi.mk

include $(CLEAR_VARS)
include $(APP_COMMON_PATH)/Framework.mk

LOCAL_MODULE    := simple
LOCAL_SRC_FILES += $(SVR_ROOT_PATH)/samples/SimpleVR/jni/app.cpp

MY_VERA_RENDER_DIR          := $(SVR_ROOT_PATH)/vera/render
MY_VERA_UTILS_DIR           := $(SVR_ROOT_PATH)/vera/utils
MY_VERA_INCLUDE_DIR         := $(SVR_ROOT_PATH)/vera/include
FILE_LIST                   := $(wildcard $(MY_VERA_RENDER_DIR)/*.cpp)
LOCAL_SRC_FILES             += $(FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_C_INCLUDES            += $(MY_VERA_INCLUDE_DIR)

include $(APP_COMMON_PATH)/App.mk

