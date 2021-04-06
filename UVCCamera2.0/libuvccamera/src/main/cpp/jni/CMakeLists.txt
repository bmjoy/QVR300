# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

set(jnilibs "${CMAKE_SOURCE_DIR}/../jniLibs")

include_directories(
        ${CMAKE_SOURCE_DIR}
        util
        render
        sample
        glm
        inc
        inc/assimp
        model
        opencv_3_0_0
        arcore
        libuvc_android/source/libjpeg/libjpeg-turbo-1.5.0/include
        libuvc_android/source/libjpeg/libjpeg-turbo-1.5.0
        libuvc_android/source/libusb/libusb
        libuvc_android/source/libusb/liusb/libusb
        libuvc_android/source/libuvc/libuvc/include
        libuvc_android/source/libuvc/libuvc/include/libuvc
)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
file(GLOB src-files
        ${CMAKE_SOURCE_DIR}/*.c
        ${CMAKE_SOURCE_DIR}/*.cpp
        ${CMAKE_SOURCE_DIR}/*jni_interface.cpp
        ${CMAKE_SOURCE_DIR}/render/*.cpp
        ${CMAKE_SOURCE_DIR}/util/*.cpp
        ${CMAKE_SOURCE_DIR}/sample/CubeSample.cpp
        ${CMAKE_SOURCE_DIR}/sample/Model3DSample.cpp
        )

add_library( # Sets the name of the library.
            uvc_native

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             ${src-files}
        )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

add_library(assimp SHARED IMPORTED )
set_target_properties(assimp PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libassimp.so)

add_library(opencv_java3 SHARED IMPORTED )
set_target_properties(opencv_java3 PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libopencv_java3.so)

add_library(glesv3 SHARED IMPORTED )
set_target_properties(glesv3 PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libGLESv3.so)

add_library(arcore SHARED IMPORTED )
set_target_properties(arcore PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libarcore.so)


add_library(jpeg STATIC IMPORTED )
set_target_properties(jpeg PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libjpeg-turbo1500_static.a)

add_library(usb100 STATIC IMPORTED )
set_target_properties(usb100 PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libusb100_static.a)

add_library(uvc STATIC IMPORTED )
set_target_properties(uvc PROPERTIES IMPORTED_LOCATION ${jnilibs}/${ANDROID_ABI}/libuvc_static.a)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

#ASAN 需要 NDK20 编译，Android P 以上运行
#target_compile_options(native-render PUBLIC -fsanitize=address -fno-omit-frame-pointer)
#set_target_properties(native-render PROPERTIES LINK_FLAGS -fsanitize=address)

#SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(third-party-libs
        opencv_java3
        jpeg
        usb100
        uvc
        glesv3
        arcore
        android)

target_link_libraries( # Specifies the target library.
                        uvc_native

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
                       ${third-party-libs}
                       EGL
        assimp
                       )