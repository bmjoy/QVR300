##############################################################################
# Copyright (c) 2015 Huajie IMI Technology Co., Ltd.
# All rights reserved.
# @File Name     : Application.mk
# @Author        : YangFeiYi
# @Date          : 2015-06-24
# @Description   : Described application and the compile modules
# @Version       : 0.1
# @History       :
##############################################################################
APP_ABI := armeabi armeabi-v7a
APP_STL := stlport_static
APP_PLATFORM = 10
APP_CPPFLAGS += -fexceptions -frtti
APP_CFLAGS += -Wno-error=format-security
#APP_OPTIM := debug
APP_OPTIM := release
#The compile modules
APP_MODULES := libuvc_static
