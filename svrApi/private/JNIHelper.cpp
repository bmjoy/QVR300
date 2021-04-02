//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include<jni.h>

namespace JNIHelper {
    //-----------------------------------------------------------------------------
    JNIEnv* GetJNIEnv(JavaVM* javaVM)
    //-----------------------------------------------------------------------------
    {
        JNIEnv* jniEnv = 0;
        javaVM->AttachCurrentThread(&jniEnv, 0);
        return jniEnv;
    }
    
    //-----------------------------------------------------------------------------
    jclass LoadClass(JavaVM* javaVM, jobject context, const char* className)
    //-----------------------------------------------------------------------------
    {
        //Get JNIEnv
        JNIEnv* jniEnv = GetJNIEnv(javaVM);
        
        //Load Classloader
        jclass clz_Activity = jniEnv->GetObjectClass(context);
        jmethodID getClassLoaderMethodId = jniEnv->GetMethodID(clz_Activity, "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject classLoaderObj = jniEnv->CallObjectMethod(context, getClassLoaderMethodId);

        jclass clz_ClassLoader = jniEnv->GetObjectClass(classLoaderObj);
        jmethodID findClassMethodId = jniEnv->GetMethodID(clz_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

        //loadClass with the passed in className
        jstring strClassName = jniEnv->NewStringUTF(className);
        jclass tmpjavaClass = (jclass)jniEnv->CallObjectMethod(classLoaderObj, findClassMethodId, strClassName);
        
        //Return a GlobalRef
        jclass clazz = (jclass)jniEnv->NewGlobalRef(tmpjavaClass);
        
        return clazz;
    }
}