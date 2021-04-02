// IControllerInterfaceCallback.aidl
package com.qualcomm.svrapi.controllers;

interface IControllerInterfaceCallback{
    /**
     * Callback for receiving state change messages. The state constants are defined in
     * SvrControllerApi.java
     * @param handle Identifier for controller.
     * @param what New state.
     * @param arg1 Additional Information.
     * @param arg2 Additional Information.
     */
    oneway void onStateChanged(int handle, int what, int arg1, int arg2);
}

