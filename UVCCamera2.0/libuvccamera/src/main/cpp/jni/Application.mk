APP_ABI := armeabi-v7a
APP_STL := gnustl_static
APP_PLATFORM = 10
APP_CPPFLAGS += -fexceptions -frtti -std=c++11
APP_CFLAGS += -Wno-error=format-security -flax-vector-conversions -DSTRIP_LOG
APP_OPTIM := release
APP_MODULES := uvc_native