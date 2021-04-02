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

APP_COMMON_PATH := $(LOCAL_PATH)/../../common

include $(APP_COMMON_PATH)/SvrApi.mk

include $(CLEAR_VARS)
include $(APP_COMMON_PATH)/Framework.mk

LOCAL_MODULE    := controller
LOCAL_SRC_FILES += app.cpp

include $(APP_COMMON_PATH)/App.mk

