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

SDK_ROOT_PATH := $(LOCAL_PATH)/../../../

APP_COMMON_UTILS_PATH:= $(SDK_ROOT_PATH)/samples/common/utils/
LOCAL_C_INCLUDES += $(APP_COMMON_UTILS_PATH)
LOCAL_SRC_FILES += $(APP_COMMON_UTILS_PATH)/utils.cpp

LOCAL_ARM_MODE	:= arm
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv3
LOCAL_SHARED_LIBRARIES := libsvrapi

LOCAL_WHOLE_STATIC_LIBRARIES += android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)