#ifndef SC_COMMON_H
#define SC_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <math.h>
#include <inttypes.h>
#include <pthread.h>
#include <android/log.h>
#include <string>
#include <functional>
#include <assert.h>
#include <memory>
#include <string.h>
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#include <thread>
#include <chrono>
#include <future>
#include <mutex>
//#define GLM_FORCE_MESSAGES
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ShadowCreator {

#define DEBUG_TAG "willie_native_log"
#define WLOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, __VA_ARGS__))
#define WLOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, DEBUG_TAG, __VA_ARGS__))
#define WLOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, DEBUG_TAG, __VA_ARGS__))
#define ARRAY_SIZE(x)    ((int) (sizeof(x) / sizeof((x)[0])))
    using Nanoseconds = uint64_t;

    #define SENSOR_ORIENTATION_CORRECT_X 0
    #define SENSOR_ORIENTATION_CORRECT_Y 11.5f
    #define SENSOR_ORIENTATION_CORRECT_Z 0
    #define SENSOR_HEAD_OFFSET_X 0.04f
    #define SENSOR_HEAD_OFFSET_Y -0.035f
    #define SENSOR_HEAD_OFFSET_Z 0
}

#endif //SC_COMMON_H