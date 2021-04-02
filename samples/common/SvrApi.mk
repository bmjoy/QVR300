# Copyright (C) 2017 The Android Open Source Project
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

include $(CLEAR_VARS)
MY_PATH := $(LOCAL_PATH)

LOCAL_MODULE := libsvrapi
#LOCAL_SRC_FILES := $(MY_PATH)/../app/.svrLibs/$(APP_OPTIM)/jni/$(TARGET_ARCH_ABI)/libsvrapi.so
LOCAL_SRC_FILES := $(SVR_ROOT_PATH)/svrApi/libs/$(APP_OPTIM)/jni/$(TARGET_ARCH_ABI)/libsvrapi.so

#LOCAL_SRC_FILES := ../../../svrapi/libs/armeabi-v7a/libsvrapi.so
include $(PREBUILT_SHARED_LIBRARY)