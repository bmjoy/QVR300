LOCAL_PATH := $(call my-dir)
MY_PATH := $(LOCAL_PATH)

#-Wundef does not work with GCC as of 4.9
MY_GNU_FLAGS := -std=c++14 -W -Wall -Wextra -Werror -Wpedantic\
    -Wdouble-promotion -Wmissing-include-dirs -Wswitch-enum\
    -Wunused-parameter -Wuninitialized -Wfloat-equal -Wshadow\
    -Wcast-qual -Wcast-align -Wconversion -Wzero-as-null-pointer-constant\
    -Wuseless-cast -Wlogical-op -Wmissing-declarations\
    -Wpacked -Wpadded -Wredundant-decls -Winline -Winvalid-pch -Wvarargs\
    -Wvla -Wdisabled-optimization
MY_CLANG_FLAGS := -std=c++14 -Werror -Weverything -Wno-c++98-compat \
    -Wno-c++98-compat-pedantic -Wno-reserved-id-macro
MY_OPTIM_DEBUG := -g -O0
MY_OPTIM_RELEASE := -g0 -O3 -DNDEBUG

MY_GNU_FLAGS := -std=c++14
MY_CLANG_FLAGS := -std=c++14

# -----------------------------------------------------------------------------
# sharedMemory
# -----------------------------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_PATH := $(MY_PATH)
LOCAL_MODULE := sharedmem
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/SvrControllerUtil/inc/
LOCAL_C_INCLUDES += $(SVR_HOME)/svrApi/public/

ifeq ($(findstring clang, $(NDK_TOOLCHAIN_VERSION)), clang)
    LOCAL_CFLAGS := $(MY_CLANG_FLAGS)
else
    LOCAL_CFLAGS := $(MY_GNU_FLAGS)
endif

ifeq ($(APP_OPTIM),debug)
    LOCAL_CFLAGS += $(MY_OPTIM_DEBUG)
else
    LOCAL_CFLAGS += $(MY_OPTIM_RELEASE)
endif

LOCAL_LDLIBS := -landroid -llog
LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.cpp))
LOCAL_SRC_FILES += ./SvrControllerUtil/src/SharedMemory.cpp
include $(BUILD_SHARED_LIBRARY)
# -----------------------------------------------------------------------------
