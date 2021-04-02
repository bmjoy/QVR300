/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: D:\\ws\\p4\\depot\\sgs\\svr\\latest\\svrApi\\build\\gradle\\svrApi\\src\\main\\aidl\\com\\qualcomm\\svrapi\\controllers\\IControllerInterfaceCallback.aidl
 */
package com.qualcomm.svrapi.controllers;
public interface IControllerInterfaceCallback extends android.os.IInterface
{
/** Local-side IPC implementation stub class. */
public static abstract class Stub extends android.os.Binder implements com.qualcomm.svrapi.controllers.IControllerInterfaceCallback
{
private static final java.lang.String DESCRIPTOR = "com.qualcomm.svrapi.controllers.IControllerInterfaceCallback";
/** Construct the stub at attach it to the interface. */
public Stub()
{
this.attachInterface(this, DESCRIPTOR);
}
/**
 * Cast an IBinder object into an com.qualcomm.svrapi.controllers.IControllerInterfaceCallback interface,
 * generating a proxy if needed.
 */
public static com.qualcomm.svrapi.controllers.IControllerInterfaceCallback asInterface(android.os.IBinder obj)
{
if ((obj==null)) {
return null;
}
android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
if (((iin!=null)&&(iin instanceof com.qualcomm.svrapi.controllers.IControllerInterfaceCallback))) {
return ((com.qualcomm.svrapi.controllers.IControllerInterfaceCallback)iin);
}
return new com.qualcomm.svrapi.controllers.IControllerInterfaceCallback.Stub.Proxy(obj);
}
@Override public android.os.IBinder asBinder()
{
return this;
}
@Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
{
switch (code)
{
case INTERFACE_TRANSACTION:
{
reply.writeString(DESCRIPTOR);
return true;
}
case TRANSACTION_onStateChanged:
{
data.enforceInterface(DESCRIPTOR);
int _arg0;
_arg0 = data.readInt();
int _arg1;
_arg1 = data.readInt();
int _arg2;
_arg2 = data.readInt();
int _arg3;
_arg3 = data.readInt();
this.onStateChanged(_arg0, _arg1, _arg2, _arg3);
return true;
}
}
return super.onTransact(code, data, reply, flags);
}
private static class Proxy implements com.qualcomm.svrapi.controllers.IControllerInterfaceCallback
{
private android.os.IBinder mRemote;
Proxy(android.os.IBinder remote)
{
mRemote = remote;
}
@Override public android.os.IBinder asBinder()
{
return mRemote;
}
public java.lang.String getInterfaceDescriptor()
{
return DESCRIPTOR;
}
@Override public void onStateChanged(int handle, int what, int arg1, int arg2) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeInt(handle);
_data.writeInt(what);
_data.writeInt(arg1);
_data.writeInt(arg2);
mRemote.transact(Stub.TRANSACTION_onStateChanged, _data, null, android.os.IBinder.FLAG_ONEWAY);
}
finally {
_data.recycle();
}
}
}
static final int TRANSACTION_onStateChanged = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
}
public void onStateChanged(int handle, int what, int arg1, int arg2) throws android.os.RemoteException;
}
