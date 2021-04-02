// IControllerInterface.aidl
package com.qualcomm.svrapi.controllers;
import com.qualcomm.svrapi.controllers.IControllerInterfaceCallback;

// Declare any non-default types here with import statements

interface IControllerInterface {
    /**
     * Registers the callback for receiving state changes from Controller Provider.
     * @param callback The ControllerInterfaceCallback
     */
    void registerCallback(IControllerInterfaceCallback callback);
    
    /**
     * Unregister the existing callback.
     */
    void unregisterCallback();
    
    /**
     * <p>
     * Start tracking a controller. Handle is the identifier maintained by the SDK to identify this
     * controller instance with. This should be used when sending back state change messages to the SDK.
     * </p>
     * <p>
     * SVR SDK allocates the shared memory and passes the file descriptor along for the Controller
     * Provider to write to. With this call, the provider can start updating the controller data
     * into the provided Ring Buffer.<br>
     * Use the jni/SvrControllerUtil/RingBuffer.h to map and write into the Ring Buffer.
     * </p>
     *
     * @param handle Identifier maintained by the SDK for this controller.
     * @param desc Additional parameters can be passed along to the provider. (Optional)
     * @param pfd FileDescriptor for the Ring Buffer Shared Memory.
     * @param fdSize FileDescriptor's Size
     * @param qvrFd FileDescriptor for QVR's Head Pose data.
     * @param qvrFdSize FileDescriptor's Size
     */
    void Start(in int handle, in String desc, in ParcelFileDescriptor pfd, in int fdSize, in ParcelFileDescriptor qvrFd, int qvrFdSize);
    
    /**
     * Stop tracking the controller identified by the Handle.
     * @param handle Identifier for the controller to stop.
     */
    void Stop(in int handle);
    
    /**
     * Sends a message to the Controller Provider.
     * Currently Supported Messages (defined in SvrControllerApi.java):
     *      <table border='1' cellpadding='10'>
     *      <tr><th>Message</th><th>Argument 1</th><th>Argument 2</th></tr>
     *      <tr><td><b>Recenter</b></td><td>Ignored</td><td>Ignored</td></tr>
     *      <tr><td><b>Vibrate</b></td><td>Amplitude</td><td>Duration</td></tr>
     *      </table>
     * @param handle Identifier for the controller to signal
     * @param type Message Type
     * @param arg1 Argument 1
     * @param arg2 Argument 2
     */
    void SendMessage(in int handle, in int type, in int arg1, in int arg2);
    
    /**
     * Query information from the Controller Provider - Integer Data.
     * Currently Supported: (defined in SvrControllerApi.java)
     *      <ul>
     *          <li><b>BatteryRemaining</b></li>
     *          <li><b>ControllerCapability</b></li>
     *          <li><b>ActiveButtons</b></li>
     *          <li><b>Active2DAnalogs</b></li>
     *          <li><b>Active1DAnalogs</b></li>
     *          <li><b>ActiveTouchButtons</b></li>
     *          <li><b>NumControllers</b></li>
     *      </ul>
     * @param handle Identifier for the controller to Query.
     * @param type Query Type
     * @return Queried Data
     */
    int  QueryInt(in int handle, in int type);

    /**
     * Query information from the Controller Provider - String Data.
     * Currently Supported: (defined in SvrControllerApi.java)
     *      <ul>
     *          <li><b>DeviceManufacturer</b></li>
     *          <li><b>DeviceIdentifier</b></li>
     *      </ul>
     * @param handle Identifier for the controller to Query.
     * @param type Query Type
     * @return Queried Data
     */
    String  QueryString(in int handle, in int type);
}
