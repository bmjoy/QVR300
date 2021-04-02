
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

SVR_FRAMEWORK_PATH   := $(SDK_ROOT_PATH)/framework
GLM_ROOT_PATH        := $(SDK_ROOT_PATH)/3rdparty/glm-0.9.7.0
IMGUI_ROOT_PATH      := $(SDK_ROOT_PATH)/3rdparty/imgui
TINYOBJ_ROOT_PATH    := $(SDK_ROOT_PATH)/3rdparty/tinyobj
SVR_API_ROOT_PATH    := $(SDK_ROOT_PATH)/svrApi/public
TELEMETRY_ROOT_PATH  := $(SDK_ROOT_PATH)/3rdparty/telemetry

LOCAL_C_INCLUDES := $(GLM_ROOT_PATH) $(IMGUI_ROOT_PATH) $(SVR_FRAMEWORK_PATH) $(TINYOBJ_ROOT_PATH) $(SVR_API_ROOT_PATH) $(TELEMETRY_ROOT_PATH)/include 

LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrApplication.cpp 
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrAndroidMain.cpp 
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrInput.cpp
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrGeometry.cpp 
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrShader.cpp 
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrUtil.cpp 
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrRenderTarget.cpp
LOCAL_SRC_FILES += $(SVR_FRAMEWORK_PATH)/svrCpuTimer.cpp
LOCAL_SRC_FILES += $(IMGUI_ROOT_PATH)/imgui.cpp
LOCAL_SRC_FILES += $(TINYOBJ_ROOT_PATH)/tiny_obj_loader.cc

