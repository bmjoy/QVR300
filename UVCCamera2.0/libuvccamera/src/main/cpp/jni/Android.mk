LOCAL_PATH	:= $(call my-dir)
include $(CLEAR_VARS)
#CFLAGS := -Werror

# set path to source
MY_PREFIX := $(LOCAL_PATH)

# set path to source
MY_SRC_FILES := \
             $(LOCAL_PATH)/*.cpp \
             $(LOCAL_PATH)/*.c \
             $(LOCAL_PATH)/render/*.cpp \
             $(LOCAL_PATH)/sample/*.cpp \
			 
# expand the wildcards
MY_SRC_FILE_EXPANDED := $(wildcard $(MY_SRC_FILES))

# make those paths relative to here
LOCAL_SRC_FILES := $(MY_SRC_FILE_EXPANDED:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := \
		$(MY_PREFIX)/ \
		$(MY_PREFIX)/libuvc_android/source/libjpeg/libjpeg-turbo-1.5.0/include/ \
		$(MY_PREFIX)/libuvc_android/source/libjpeg/libjpeg-turbo-1.5.0/ \
		$(MY_PREFIX)/libuvc_android/source/libusb/libusb/ \
		$(MY_PREFIX)/libuvc_android/source/libusb/liusb/libusb/ \
		$(MY_PREFIX)/libuvc_android/source/libuvc/libuvc/include/ \
		$(MY_PREFIX)/libuvc_android/source/libuvc/libuvc/include/libuvc \
		$(MY_PREFIX)/glm \
		$(MY_PREFIX)/inc \
		$(MY_PREFIX)/model \
		$(MY_PREFIX)/opencv_3_0_0 \
		$(MY_PREFIX)/render \
		$(MY_PREFIX)/util \
		$(MY_PREFIX)/sample \

LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_CFLAGS += -DANDROID_NDK
LOCAL_CFLAGS += -DLOG_NDEBUG
LOCAL_CFLAGS += -DACCESS_RAW_DESCRIPTORS
LOCAL_CFLAGS += -O3 -fstrict-aliasing -fprefetch-loop-arrays

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)

else
	LOCAL_CFLAGS += -mfloat-abi=softfp -mfpu=neon -march=armv7-a -mtune=cortex-a8
endif


LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -ldl -lGLESv2 \
                -llog -lm \
				$(LOCAL_PATH)/libuvc_android/lib/$(TARGET_ARCH_ABI)/libuvc_static.a \
				$(LOCAL_PATH)/libuvc_android/lib/$(TARGET_ARCH_ABI)/libjpeg-turbo1500_static.a \
				$(LOCAL_PATH)/libuvc_android/lib/$(TARGET_ARCH_ABI)/libusb100_static.a \


LOCAL_ARM_MODE?:= arm
LOCAL_ARM_NEON?:= true

LOCAL_CPPFLAGS += -fPIC

LOCAL_MODULE    := uvc_native
include $(BUILD_SHARED_LIBRARY)
