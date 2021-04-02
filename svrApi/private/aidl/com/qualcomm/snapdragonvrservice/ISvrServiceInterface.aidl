// SvrServiceInterface.aidl
package com.qualcomm.snapdragonvrservice;

// Declare any non-default types here with import statements

interface ISvrServiceInterface {
    Intent GetControllerProviderIntent(in String desc);
    int GetControllerRingBufferSize();
}
