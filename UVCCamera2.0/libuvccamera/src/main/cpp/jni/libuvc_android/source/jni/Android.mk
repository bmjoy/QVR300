#include $(call all-subdir-makefiles)
PROJ_PATH	:= $(call my-dir)
include $(CLEAR_VARS)
include $(PROJ_PATH)/../libjpeg/libjpeg-turbo-1.5.0/Android.mk
include $(PROJ_PATH)/../libusb/libusb/android/jni/Android.mk
include $(PROJ_PATH)/../libuvc/libuvc/android/jni/Android.mk