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
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace UnityLauncher {

#define DEBUG_TAG "unity_launcher"
#define WLOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, __VA_ARGS__))
#define WLOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, DEBUG_TAG, __VA_ARGS__))
#define WLOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, DEBUG_TAG, __VA_ARGS__))
#define ARRAY_SIZE(x)    ((int) (sizeof(x) / sizeof((x)[0])))
    using Nanoseconds = uint64_t;
}

#endif //SC_COMMON_H