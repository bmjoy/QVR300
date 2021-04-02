/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: D:\\ws\\p4\\depot\\sgs\\svr\\latest\\svrApi\\build\\gradle\\svrApi\\src\\main\\aidl\\com\\qualcomm\\snapdragonvrservice\\ISvrServiceInterface.aidl
 */
package com.qualcomm.snapdragonvrservice;
// Declare any non-default types here with import statements

public interface ISvrServiceInterface extends android.os.IInterface
{
/** Local-side IPC implementation stub class. */
public static abstract class Stub extends android.os.Binder implements com.qualcomm.snapdragonvrservice.ISvrServiceInterface
{
private static final java.lang.String DESCRIPTOR = "com.qualcomm.snapdragonvrservice.ISvrServiceInterface";
/** Construct the stub at attach it to the interface. */
public Stub()
{
this.attachInterface(this, DESCRIPTOR);
}
/**
 * Cast an IBinder object into an com.qualcomm.snapdragonvrservice.ISvrServiceInterface interface,
 * generating a proxy if needed.
 */
public static com.qualcomm.snapdragonvrservice.ISvrServiceInterface asInterface(android.os.IBinder obj)
{
if ((obj==null)) {
return null;
}
android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
if (((iin!=null)&&(iin instanceof com.qualcomm.snapdragonvrservice.ISvrServiceInterface))) {
return ((com.qualcomm.snapdragonvrservice.ISvrServiceInterface)iin);
}
return new com.qualcomm.snapdragonvrservice.ISvrServiceInterface.Stub.Proxy(obj);
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
case TRANSACTION_GetControllerProviderIntent:
{
data.enforceInterface(DESCRIPTOR);
java.lang.String _arg0;
_arg0 = data.readString();
android.content.Intent _result = this.GetControllerProviderIntent(_arg0);
reply.writeNoException();
if ((_result!=null)) {
reply.writeInt(1);
_result.writeToParcel(reply, android.os.Parcelable.PARCELABLE_WRITE_RETURN_VALUE);
}
else {
reply.writeInt(0);
}
return true;
}
case TRANSACTION_GetControllerRingBufferSize:
{
data.enforceInterface(DESCRIPTOR);
int _result = this.GetControllerRingBufferSize();
reply.writeNoException();
reply.writeInt(_result);
return true;
}
}
return super.onTransact(code, data, reply, flags);
}
private static class Proxy implements com.qualcomm.snapdragonvrservice.ISvrServiceInterface
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
@Override public android.content.Intent GetControllerProviderIntent(java.lang.String desc) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
android.content.Intent _result;
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeString(desc);
mRemote.transact(Stub.TRANSACTION_GetControllerProviderIntent, _data, _reply, 0);
_reply.readException();
if ((0!=_reply.readInt())) {
_result = android.content.Intent.CREATOR.createFromParcel(_reply);
}
else {
_result = null;
}
}
finally {
_reply.recycle();
_data.recycle();
}
return _result;
}
@Override public int GetControllerRingBufferSize() throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
int _result;
try {
_data.writeInterfaceToken(DESCRIPTOR);
mRemote.transact(Stub.TRANSACTION_GetControllerRingBufferSize, _data, _reply, 0);
_reply.readException();
_result = _reply.readInt();
}
finally {
_reply.recycle();
_data.recycle();
}
return _result;
}
}
static final int TRANSACTION_GetControllerProviderIntent = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
static final int TRANSACTION_GetControllerRingBufferSize = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
}
public android.content.Intent GetControllerProviderIntent(java.lang.String desc) throws android.os.RemoteException;
public int GetControllerRingBufferSize() throws android.os.RemoteException;
}
