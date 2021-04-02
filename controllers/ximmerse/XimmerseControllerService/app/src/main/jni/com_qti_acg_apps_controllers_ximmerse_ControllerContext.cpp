//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include <jni.h>
#include "RingBuffer.h"
#include <pthread.h>
#include <android/log.h>
#include "svrApi.h"
#include <string.h>

#define JNI_FUNC(x) Java_com_qti_acg_apps_controllers_ximmerse_ControllerContext_##x

class NativeContext {
    public:
        svrControllerState state;
        RingBuffer<svrControllerState>* sharedMemory;
    public:
        NativeContext()
        {
            sharedMemory = 0;
			state.rotation.x = 0;
			state.rotation.y = 0;
			state.rotation.z = 0;
			state.rotation.w = 1;
			
			state.position.x = 0;
			state.position.y = 0;
			state.position.z = 0;
			
			state.accelerometer.x = 0;
			state.accelerometer.y = 0;
			state.accelerometer.z = 0;
			
			state.gyroscope.x = 0;
			state.gyroscope.y = 0;
			state.gyroscope.z = 0;
			
		
			state.connectionState = (svrControllerConnectionState)0;
			state.buttonState = 0;
			state.isTouching = 0;
			state.timestamp = 0;
			for(int i=0;i<4;i++)
			{
				state.analog2D[i].x = 0;
				state.analog2D[i].y = 0;
			}
			
			for(int i=0;i<8;i++)
			{
				state.analog1D[i] = 0;
			}
        }

        ~NativeContext()
        {
            delete sharedMemory;
            sharedMemory = 0;
        }
};
//-----------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL JNI_FUNC(updateNativeConnectionState)(JNIEnv *jniEnv, jobject, jint ptr, jint state)
//-----------------------------------------------------------------------------
{
	NativeContext* entry = (NativeContext*)(ptr);
	entry->state.connectionState = (svrControllerConnectionState)state;
	
    if( entry->sharedMemory != 0 )
    {
        entry->sharedMemory->set(&entry->state);
    }
}

//-----------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL JNI_FUNC(updateNativeState)(JNIEnv *jniEnv, jobject, jint ptr, jint state, jint btns,
                                                                                                    jfloat pos0, jfloat pos1, jfloat pos2,
                                                                                                    jfloat rot0, jfloat rot1, jfloat rot2, jfloat rot3,
                                                                                                    jfloat gyro0, jfloat gyro1, jfloat gyro2,
                                                                                                    jfloat acc0, jfloat acc1, jfloat acc2,
                                                                                                    jint timestamp, jint touchpads, jfloat x, jfloat y)
//-----------------------------------------------------------------------------
{
    NativeContext* entry = (NativeContext*)(ptr);

    entry->state.rotation.x = rot0;
    entry->state.rotation.y = rot1;
    entry->state.rotation.z = -rot2;
    entry->state.rotation.w = -rot3;

    entry->state.position.x = pos0;
    entry->state.position.y = pos1;
    entry->state.position.z = pos2;
	
	entry->state.accelerometer.x = acc0;
	entry->state.accelerometer.y = acc1;
	entry->state.accelerometer.z = acc2;
	
	entry->state.gyroscope.x = gyro0;
	entry->state.gyroscope.y = gyro1;
	entry->state.gyroscope.z = gyro2;
	

    entry->state.connectionState = (svrControllerConnectionState)state;
    entry->state.buttonState = btns;
    entry->state.isTouching = touchpads;
    entry->state.timestamp = timestamp;
    entry->state.analog2D[0].x = x;
    entry->state.analog2D[0].y = y;


    if( entry->sharedMemory != 0 )
    {
        entry->sharedMemory->set(&entry->state);
    }

    //__android_log_print(ANDROID_LOG_ERROR, "XXXXX", "state updated - %f %f %f", p0, p1, p2);
}

//-----------------------------------------------------------------------------
extern "C" JNIEXPORT jint JNICALL JNI_FUNC(createNativeContext)(JNIEnv *jniEnv, jobject, jint fd, jint size)
//-----------------------------------------------------------------------------
{
    NativeContext* nativeContext = new NativeContext();
    nativeContext->sharedMemory = RingBuffer<svrControllerState>::fromFd(fd, size, false);
    return (jint)(nativeContext);
}

//-----------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL JNI_FUNC(freeNativeContext)(JNIEnv *jniEnv, jobject, jint ptr)
//-----------------------------------------------------------------------------
{
    NativeContext* nativeContext = (NativeContext*)(ptr);
    if( nativeContext != 0 )
    {
        delete nativeContext;
    }
}
