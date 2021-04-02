//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
#include <android/native_window_jni.h>
#include "private/JNIHelper.h"
#include "svrUtil.h"
#include "svrApi.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <unistd.h>
#include <sys/syscall.h>

svrBeginParams beginParams;
svrFrameParams frameParams;

enum svrJclass 
{
        JCLASS_BPARAM = 0,
        JCLASS_FPARAM = 1,
        JCLASS_PERFLEV = 2,
        JCLASS_EYELAYER = 3,
        JCLASS_LAYOUTCOORDS = 4,
        JCLASS_VULKANINFO = 5,
        JCLASS_TEXTURETYPE = 6,
        JCLASS_EYEMASK = 7,
        JCLASS_WARPTYPE = 8,
        JCLASS_HEADPOSESTATE = 9,
        JCLASS_HEADPOSE = 10,
        JCLASS_QUAT = 11,
        JCLASS_VEC3 = 12,
        JCLASS_WHICHEYE = 13,
        JCLASS_DVINFO = 14,
        JCLASS_VFRU = 15,
        JCLASS_WARPMESHENUM = 16,
        JCLASS_COLORSPACE = 17
};
enum svrFieldID 
{
        FID_BPARAM_SURFACE = 0,
        FID_BPARAM_CPUPERF = 1,
        FID_BPARAM_GPUPERF = 2,
        FID_BPARAM_OPTIONFLAGS = 3,
        FID_BPARAM_COLORSPACE = 4,
        FID_FPARAM_FRAMEINDEX = 5,
        FID_FPARAM_MINVSYNC = 6,
        FID_FPARAM_EYEBUFTYPE = 7,
        FID_FPARAM_EYELAYER = 8,
        FID_FPARAM_FRAMEOPT = 9,
        FID_FPARAM_WARPTYPE = 10,
        FID_FRARAM_FOV = 11,
        FID_RENDERLAYER_IMAGEHANDLE = 12,
        FID_RENDERLAYER_IMAGECOORDS = 13,
        FID_RENDERLAYER_VULKANINFO = 14,
        FID_RENDERLAYER_IMAGETYPE = 15,
        FID_RENDERLAYER_EYEMASK = 16,
        FID_RENDERLAYER_FLAG = 17
};
enum svrLayoutCoordsFid
{
        FID_LAYOUTCOORDS_LLPOS = 0,
        FID_LAYOUTCOORDS_LRPOS = 1,
        FID_LAYOUTCOORDS_ULPOS = 2,
        FID_LAYOUTCOORDS_URPOS = 3,
        FID_LAYOUTCOORDS_LUV = 4,
        FID_LAYOUTCOORDS_UUV = 5,
        FID_LAYOUTCOORDS_TFMATRIX = 6
};
enum headPoseXFid
{
        HP_FPARAM_HPSTATE = 0,
        HP_POSE = 1,
        HP_ROTATION = 2,
        HP_ROTATION_X = 3,
        HP_ROTATION_Y = 4,
        HP_ROTATION_Z = 5,
        HP_ROTATION_W = 6,
        HP_POSITION = 7,
        HP_POSITION_X = 8,
        HP_POSITION_Y = 9,
        HP_POSITION_Z = 10,
        HP_POSE_STATUS = 11,
        HP_POSE_TS = 12,
        HP_ST_PFTS = 13,
        HP_ST_EDTS = 14 
};
enum deviceInfoFid
{
        DV_WIDTH = 0,
        DV_HIGHT = 1,
        DV_FPS = 2,
        DV_ORI = 3,
        DV_EYE_WIDTH = 4,
        DV_EYE_HIGHT = 5,
        DV_FOV_XRAD = 6,
        DV_FOV_YRAD = 7,
        DV_VFRUM_LEFT = 8,
        DV_VFRUM_RIGHT = 9,
        VF_LEFT = 10,
        VF_RIGHT = 11,
        VF_TOP = 12,
        VF_BOT = 13,
        VF_NEAR = 14,
        VF_FAR = 15,
        DV_OSVERSION = 16,
        DV_EYE_CONV = 17,
        DV_EYE_PITCH = 18
};
enum svrMethodID 
{
        MID_PERFLEV_ORDINAL = 0,
        MID_EYEBUFTYPE_ORDINAL = 1,
        MID_TEXTYPE_ORDINAL = 2,
        MID_EYEMASK_GETVALUE = 3,
        MID_WARPTYPE_ORDINAL = 4,
        MID_SVRAPI_INIT = 5,
        MID_HPSTATE_INIT = 6,
        MID_HP_INIT = 7,
        MID_QUAT_INIT = 8,
        MID_VEC3_INIT = 9,
        MID_WHICHEYE_ORDINAL = 10,
        MID_SETRESULT = 11,
        MID_DVINIT = 12,
        MID_VIEWFRU = 13,
        MID_WARPMESH_ORDINAL = 14,
        MID_COLORSPACE_ORDINAL = 15       
};

static jclass apiClazz[18];
static jfieldID apiFiledID[18];
static jfieldID lCoordsFid[7];
static jfieldID hpXFid[30];
static jfieldID dvFid[19];
static jmethodID apiMethodID[16];


void getQuaternion(JNIEnv * env, jobject quatObj, svrFrameParams& mParams,
jfieldID xFid, jfieldID yFid, jfieldID zFid, jfieldID wFid)
{
	mParams.headPoseState.pose.rotation.x = env->GetFloatField(quatObj, xFid);
	mParams.headPoseState.pose.rotation.y = env->GetFloatField(quatObj, yFid);
	mParams.headPoseState.pose.rotation.z = env->GetFloatField(quatObj, zFid);
	mParams.headPoseState.pose.rotation.w = env->GetFloatField(quatObj, wFid);
}


void getVector3(JNIEnv * env, jobject svrVector3Obj, svrFrameParams& mParams,
jfieldID xFid, jfieldID yFid, jfieldID zFid)
{
	mParams.headPoseState.pose.position.x = env->GetFloatField(svrVector3Obj, xFid);
	mParams.headPoseState.pose.position.y = env->GetFloatField(svrVector3Obj, yFid);
	mParams.headPoseState.pose.position.z = env->GetFloatField(svrVector3Obj, zFid);
}

void getHeadPoseState(JNIEnv * env, jclass rootClass, jobject rootObject, svrFrameParams& mParams,
jclass hpState, jclass headpose, jclass vec3, jclass quat, jfieldID* hpxFid)

{
	jobject headPoseStateObj = env->GetObjectField(rootObject, *(hpxFid+HP_FPARAM_HPSTATE));
	// svrHeadPose Obj
	jobject headPoseObj = env->GetObjectField(headPoseStateObj, *(hpxFid+HP_POSE));
	//get rotation fid in svrHeadPose
	jobject svrQuaternionObj = env->GetObjectField(headPoseObj, *(hpxFid+HP_ROTATION));

	getQuaternion(env, svrQuaternionObj, mParams, *(hpxFid+HP_ROTATION_X),
	*(hpxFid+HP_ROTATION_Y), *(hpxFid+HP_ROTATION_Z), *(hpxFid+HP_ROTATION_W));

	jobject svrVector3Obj = env->GetObjectField(headPoseObj, *(hpxFid+HP_POSITION));

	getVector3(env, svrVector3Obj, mParams, *(hpxFid+HP_POSITION_X),
	*(hpxFid+HP_POSITION_Y), *(hpxFid+HP_POSITION_Z));
	// int    poseStatus;  
	mParams.headPoseState.poseStatus = env->GetIntField(headPoseStateObj, *(hpxFid+HP_POSE_STATUS));

	// uint64_t        poseTimeStampNs;   
	mParams.headPoseState.poseTimeStampNs = env->GetLongField(headPoseStateObj, *(hpxFid+HP_POSE_TS));

    // uint64_t        poseFetchTimeNs
    mParams.headPoseState.poseFetchTimeNs = env->GetLongField(headPoseStateObj, *(hpxFid + HP_ST_PFTS));

    // uint64_t        expectedDisplayTimeNs; 
	mParams.headPoseState.expectedDisplayTimeNs = env->GetLongField(headPoseStateObj, *(hpxFid+HP_ST_EDTS));
}

void getImageCoordsArrayValueByName(JNIEnv * env, jobject parentObj, jfieldID fid, float (&value)[4])
{
	jfloatArray targetArr = (jfloatArray)env->GetObjectField(parentObj, fid);
	jfloat* ptargetArr = env->GetFloatArrayElements(targetArr, NULL);
	jsize len = env->GetArrayLength(targetArr);
	for (int i = 0; i < len; i++)
	{
		value[i] = ptargetArr[i];
	}
	env->ReleaseFloatArrayElements(targetArr, ptargetArr, 0);
}

void getImageCoords(JNIEnv * env, jobject imageCoordsObj, svrLayoutCoords& imageCoords, jfieldID* fid)
{
	getImageCoordsArrayValueByName(env, imageCoordsObj,
	*(fid+FID_LAYOUTCOORDS_LLPOS), imageCoords.LowerLeftPos);
	getImageCoordsArrayValueByName(env, imageCoordsObj,
	*(fid+FID_LAYOUTCOORDS_LRPOS), imageCoords.LowerRightPos);
	getImageCoordsArrayValueByName(env, imageCoordsObj,
	*(fid+FID_LAYOUTCOORDS_ULPOS), imageCoords.UpperLeftPos);
	getImageCoordsArrayValueByName(env, imageCoordsObj,
	*(fid+FID_LAYOUTCOORDS_URPOS), imageCoords.UpperRightPos);
	getImageCoordsArrayValueByName(env, imageCoordsObj,
	*(fid+FID_LAYOUTCOORDS_LUV), imageCoords.LowerUVs);
	getImageCoordsArrayValueByName(env, imageCoordsObj,
	*(fid+FID_LAYOUTCOORDS_UUV), imageCoords.UpperUVs);
	// TransformMatrix
	jfloatArray TransformMatrixArr =
	(jfloatArray)env->GetObjectField(imageCoordsObj, *(fid+FID_LAYOUTCOORDS_TFMATRIX));
	jfloat* pTransformMatrixArr = env->GetFloatArrayElements(TransformMatrixArr, NULL);
	jsize pTransformMatrixArrlen = env->GetArrayLength(TransformMatrixArr);
	for (int i = 0; i < pTransformMatrixArrlen; i++)
	{
		imageCoords.TransformMatrix[i] = pTransformMatrixArr[i];
	}
	env->ReleaseFloatArrayElements(TransformMatrixArr, pTransformMatrixArr, 0);
}

void getVulkanTexInfo(JNIEnv * env, jclass vulkanInfoClass, jobject vulkanInfoObj, svrVulkanTexInfo& vulkanInfo)
{
	// uint32_t            memSize;
	jfieldID memSizeFid = env->GetFieldID(vulkanInfoClass, "memSize", "I");
	vulkanInfo.memSize = env->GetIntField(vulkanInfoObj, memSizeFid);
	// uint32_t            width;
	jfieldID widthFid = env->GetFieldID(vulkanInfoClass, "width", "I");
	vulkanInfo.width = env->GetIntField(vulkanInfoObj, widthFid);
	// uint32_t            height;
	jfieldID heightFid = env->GetFieldID(vulkanInfoClass, "height", "I");
	vulkanInfo.height = env->GetIntField(vulkanInfoObj, heightFid);
	// uint32_t            numMips;
	jfieldID numMipsFid = env->GetFieldID(vulkanInfoClass, "numMips", "I");
	vulkanInfo.numMips = env->GetIntField(vulkanInfoObj, numMipsFid);
	// uint32_t            bytesPerPixel;
	jfieldID bytesPerPixelFid = env->GetFieldID(vulkanInfoClass, "bytesPerPixel", "I");
	vulkanInfo.bytesPerPixel = env->GetIntField(vulkanInfoObj, bytesPerPixelFid);
	// uint32_t            renderSemaphore;
	jfieldID renderSemaphoreFid = env->GetFieldID(vulkanInfoClass, "renderSemaphore", "I");
	vulkanInfo.renderSemaphore = env->GetIntField(vulkanInfoObj, renderSemaphoreFid);
}


void setVec3Object(JNIEnv * env, jobject& targetObj, jfieldID* hpxFid, svrVector3 vec)
{
	env->SetFloatField(targetObj, *(hpxFid+HP_POSITION_X), vec.x);
	env->SetFloatField(targetObj, *(hpxFid+HP_POSITION_Y), vec.y);
	env->SetFloatField(targetObj, *(hpxFid+HP_POSITION_Z), vec.z);
}

void setViewFrustum(JNIEnv * env, jobject& targetObj, jfieldID* vfFid, svrViewFrustum vfum)
{
	// float               left; 
	env->SetFloatField(targetObj, *(vfFid+VF_LEFT), vfum.left);
	// float               right; 
	env->SetFloatField(targetObj, *(vfFid+VF_RIGHT), vfum.right);
	// float               top;  
	env->SetFloatField(targetObj, *(vfFid+VF_TOP), vfum.top);
	// float               bottom; 
	env->SetFloatField(targetObj, *(vfFid+VF_BOT), vfum.bottom);
	// float               near;  
	env->SetFloatField(targetObj, *(vfFid+VF_NEAR), vfum.near);
	// float               far;   
	env->SetFloatField(targetObj, *(vfFid+VF_FAR), vfum.far);
}

void initJclass(JNIEnv * env, jclass* apiClazz)
{
	jclass beginParamsLocal= env->FindClass("com/qualcomm/svrapi/SvrApi$svrBeginParams");
	*(apiClazz+JCLASS_BPARAM) = (jclass)env->NewGlobalRef(beginParamsLocal);

	jclass frameParamsLocal = env->FindClass("com/qualcomm/svrapi/SvrApi$svrFrameParams");
	*(apiClazz+JCLASS_FPARAM) = (jclass)env->NewGlobalRef(frameParamsLocal);

	*(apiClazz+JCLASS_PERFLEV) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrPerfLevel");

	*(apiClazz+JCLASS_EYELAYER) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrRenderLayer");

	jclass layoutCoordsLocal = env->FindClass("com/qualcomm/svrapi/SvrApi$svrLayoutCoords");
	*(apiClazz+JCLASS_LAYOUTCOORDS) = (jclass)env->NewGlobalRef(layoutCoordsLocal);

	jclass vulkanInfoLocal = env->FindClass("com/qualcomm/svrapi/SvrApi$svrVulkanTexInfo");
	*(apiClazz+JCLASS_VULKANINFO) = (jclass)env->NewGlobalRef(vulkanInfoLocal);

	*(apiClazz+JCLASS_TEXTURETYPE) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrTextureType");

	*(apiClazz+JCLASS_EYEMASK) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrEyeMask");

	*(apiClazz+JCLASS_WARPTYPE) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrWarpType");

	jclass hpStateTmp = env->FindClass("com/qualcomm/svrapi/SvrApi$svrHeadPoseState");
	*(apiClazz+JCLASS_HEADPOSESTATE) = (jclass)env->NewGlobalRef(hpStateTmp);

	jclass hposeTmp = env->FindClass("com/qualcomm/svrapi/SvrApi$svrHeadPose");
	*(apiClazz+JCLASS_HEADPOSE) = (jclass)env->NewGlobalRef(hposeTmp);

	jclass quatTemp = env->FindClass("com/qualcomm/svrapi/SvrApi$svrQuaternion");
	*(apiClazz+JCLASS_QUAT) = (jclass)env->NewGlobalRef(quatTemp);

	jclass vec3Temp = env->FindClass("com/qualcomm/svrapi/SvrApi$svrVector3");
	*(apiClazz+JCLASS_VEC3) = (jclass)env->NewGlobalRef(vec3Temp);

	*(apiClazz+JCLASS_WHICHEYE) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrWhichEye");

	jclass dvClassTmp = env->FindClass("com/qualcomm/svrapi/SvrApi$svrDeviceInfo");
	*(apiClazz+JCLASS_DVINFO) = (jclass)env->NewGlobalRef(dvClassTmp);

	jclass viemFruTmp = env->FindClass("com/qualcomm/svrapi/SvrApi$svrViewFrustum");
	*(apiClazz+JCLASS_VFRU) = (jclass)env->NewGlobalRef(viemFruTmp);

	*(apiClazz+JCLASS_WARPMESHENUM) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrWarpMeshEnum");
	*(apiClazz+JCLASS_COLORSPACE) = env->FindClass("com/qualcomm/svrapi/SvrApi$svrColorSpace");
}

void initDeviceInfoFid(JNIEnv * env, jfieldID* dvFid, jclass dvInfo, jclass vfrum)
{
	//public int             displayWidthPixels;
	*(dvFid+DV_WIDTH) = env->GetFieldID(dvInfo, "displayWidthPixels", "I");
	//public int             displayHeightPixels; 
	*(dvFid+DV_HIGHT) = env->GetFieldID(dvInfo, "displayHeightPixels", "I");
	//public float           displayRefreshRateHz; 
	*(dvFid+DV_FPS) = env->GetFieldID(dvInfo, "displayRefreshRateHz", "F");
	//public int             displayOrientation;  
	*(dvFid+DV_ORI) = env->GetFieldID(dvInfo, "displayOrientation", "I");
	//public int             targetEyeWidthPixels;
	*(dvFid+DV_EYE_WIDTH) = env->GetFieldID(dvInfo, "targetEyeWidthPixels", "I");
	//public int             targetEyeHeightPixels; 
	*(dvFid+DV_EYE_HIGHT) = env->GetFieldID(dvInfo, "targetEyeHeightPixels", "I");
	//public float           targetFovXRad; 
	*(dvFid+DV_FOV_XRAD) = env->GetFieldID(dvInfo, "targetFovXRad", "F");
	//public float           targetFovYRad; 
	*(dvFid+DV_FOV_YRAD) = env->GetFieldID(dvInfo, "targetFovYRad", "F");
	//public svrViewFrustum  leftEyeFrustum;
	*(dvFid+DV_VFRUM_LEFT) = env->GetFieldID(dvInfo,
	"leftEyeFrustum", "Lcom/qualcomm/svrapi/SvrApi$svrViewFrustum;");
	//  public svrViewFrustum  rightEyeFrustum; 
	*(dvFid+DV_VFRUM_RIGHT) = env->GetFieldID(dvInfo,
	"rightEyeFrustum", "Lcom/qualcomm/svrapi/SvrApi$svrViewFrustum;");

	// float               left; 
	*(dvFid+VF_LEFT) = env->GetFieldID(vfrum, "left", "F");
	// float               right; 
	*(dvFid+VF_RIGHT) = env->GetFieldID(vfrum, "right", "F");
	// float               top;  
	*(dvFid+VF_TOP) = env->GetFieldID(vfrum, "top", "F");
	// float               bottom; 
	*(dvFid+VF_BOT) = env->GetFieldID(vfrum, "bottom", "F");
	// float               near;  
	*(dvFid+VF_NEAR) = env->GetFieldID(vfrum, "near", "F");
	// float               far;   
	*(dvFid+VF_FAR) = env->GetFieldID(vfrum, "far", "F");

	*(dvFid+DV_OSVERSION) = env->GetFieldID(dvInfo, "deviceOSVersion", "I");
	//public float           targetEyeConvergence; 
	*(dvFid+DV_EYE_CONV) = env->GetFieldID(dvInfo, "targetEyeConvergence", "F");
	//public float           targetEyePitch;    
	*(dvFid+DV_EYE_PITCH) = env->GetFieldID(dvInfo, "targetEyePitch", "F");
}


void initLayoutCoordsFid(JNIEnv * env, jfieldID* lCoordsFid, jclass layoutCoords)
{
	*(lCoordsFid+FID_LAYOUTCOORDS_LLPOS) =
	env->GetFieldID(layoutCoords, "LowerLeftPos", "[F");

	*(lCoordsFid+FID_LAYOUTCOORDS_LRPOS) =
	env->GetFieldID(layoutCoords, "LowerRightPos", "[F");

	*(lCoordsFid+FID_LAYOUTCOORDS_ULPOS) =
	env->GetFieldID(layoutCoords, "UpperLeftPos", "[F");

	*(lCoordsFid+FID_LAYOUTCOORDS_URPOS) =
	env->GetFieldID(layoutCoords, "UpperRightPos", "[F");

	*(lCoordsFid+FID_LAYOUTCOORDS_LUV) =
	env->GetFieldID(layoutCoords, "LowerUVs", "[F");
	*(lCoordsFid+FID_LAYOUTCOORDS_UUV) =
	env->GetFieldID(layoutCoords, "UpperUVs", "[F");

	*(lCoordsFid+FID_LAYOUTCOORDS_TFMATRIX) =
	env->GetFieldID(layoutCoords, "TransformMatrix", "[F");
}

void initApiFid(JNIEnv * env, jfieldID* apiFiledID,
jclass beginParam, jclass frameParam, jclass renderLayer)
{
	//get surface
	*(apiFiledID+FID_BPARAM_SURFACE) =
	env->GetFieldID(beginParam, "surface", "Landroid/view/Surface;");

	*(apiFiledID+FID_BPARAM_GPUPERF) = env->GetFieldID(beginParam,
	"gpuPerfLevel", "Lcom/qualcomm/svrapi/SvrApi$svrPerfLevel;");
	*(apiFiledID+FID_BPARAM_CPUPERF) = env->GetFieldID(beginParam,
	"cpuPerfLevel", "Lcom/qualcomm/svrapi/SvrApi$svrPerfLevel;");
	//optionFlags
	*(apiFiledID+FID_BPARAM_OPTIONFLAGS) = env->GetFieldID(beginParam,
	"optionFlags", "I");
	//color space
	*(apiFiledID+FID_BPARAM_COLORSPACE) = env->GetFieldID(beginParam,
	"colorSpace", "Lcom/qualcomm/svrapi/SvrApi$svrColorSpace;");
	//get frameIndex
	*(apiFiledID+FID_FPARAM_FRAMEINDEX) =
	env->GetFieldID(frameParam, "frameIndex", "I");
	//get minVsyncs
	*(apiFiledID+FID_FPARAM_MINVSYNC) =
	env->GetFieldID(frameParam, "minVsyncs", "I");

	*(apiFiledID+FID_FPARAM_EYELAYER) = env->GetFieldID(frameParam,
	"renderLayers", "[Lcom/qualcomm/svrapi/SvrApi$svrRenderLayer;");
	//public int  frameOptions;
	*(apiFiledID+FID_FPARAM_FRAMEOPT) = env->GetFieldID(frameParam, "frameOptions", "I");

	*(apiFiledID+FID_FPARAM_WARPTYPE) = env->GetFieldID(frameParam,
	"warpType", "Lcom/qualcomm/svrapi/SvrApi$svrWarpType;");
	// public float                fieldOfView;
	*(apiFiledID+FID_FRARAM_FOV) = env->GetFieldID(frameParam, "fieldOfView", "F");
	//get renderLayers imageHandle
	*(apiFiledID+FID_RENDERLAYER_IMAGEHANDLE) =
	env->GetFieldID(renderLayer, "imageHandle", "I");
	//public svrLayoutCoords     imageCoords; 
	*(apiFiledID+FID_RENDERLAYER_IMAGECOORDS) = env->GetFieldID(renderLayer,
	"imageCoords", "Lcom/qualcomm/svrapi/SvrApi$svrLayoutCoords;");
	//public svrVulkanTexInfo    vulkanInfo;
	*(apiFiledID+FID_RENDERLAYER_VULKANINFO) = env->GetFieldID(renderLayer,
	"vulkanInfo", "Lcom/qualcomm/svrapi/SvrApi$svrVulkanTexInfo;");
	//public svrTextureType      imageType; 
	*(apiFiledID+FID_RENDERLAYER_IMAGETYPE) = env->GetFieldID(renderLayer,
	"imageType", "Lcom/qualcomm/svrapi/SvrApi$svrTextureType;");
	// public svrEyeMask          eyeMask;
	*(apiFiledID+FID_RENDERLAYER_EYEMASK) = env->GetFieldID(renderLayer,
	"eyeMask", "Lcom/qualcomm/svrapi/SvrApi$svrEyeMask;");
	//
	*(apiFiledID+FID_RENDERLAYER_FLAG) = env->GetFieldID(renderLayer,
	"layerFlags", "I");
}

void initHeadPoseStateFid(JNIEnv * env, jfieldID* hpXFid, jclass frameParam,
jclass hpState, jclass headPose, jclass quat, jclass vec3)
{
	// public svrHeadPoseState    headPoseState; 
	*(hpXFid+HP_FPARAM_HPSTATE) = env->GetFieldID(frameParam,
	"headPoseState", "Lcom/qualcomm/svrapi/SvrApi$svrHeadPoseState;");
	//svrHeadPose Obj
	*(hpXFid+HP_POSE) = env->GetFieldID(hpState,
	"pose", "Lcom/qualcomm/svrapi/SvrApi$svrHeadPose;");
	//get rotation fid in svrHeadPose
	*(hpXFid+HP_ROTATION) = env->GetFieldID(headPose,
	"rotation", "Lcom/qualcomm/svrapi/SvrApi$svrQuaternion;");
	//rotation x
	*(hpXFid+HP_ROTATION_X) = env->GetFieldID(quat, "x", "F");
	*(hpXFid+HP_ROTATION_Y) = env->GetFieldID(quat, "y", "F");
	*(hpXFid+HP_ROTATION_Z) = env->GetFieldID(quat, "z", "F");
	*(hpXFid+HP_ROTATION_W) = env->GetFieldID(quat, "w", "F");

	*(hpXFid+HP_POSITION) = env->GetFieldID(headPose,
	"position", "Lcom/qualcomm/svrapi/SvrApi$svrVector3;");
	//rotation x
	*(hpXFid+HP_POSITION_X) = env->GetFieldID(vec3, "x", "F");
	*(hpXFid+HP_POSITION_Y) = env->GetFieldID(vec3, "y", "F");
	*(hpXFid+HP_POSITION_Z) = env->GetFieldID(vec3, "z", "F");
	// int                 poseStatus;
	*(hpXFid+HP_POSE_STATUS) = env->GetFieldID(hpState, "poseStatus", "I");

	// uint64_t            poseTimeStampNs;
	*(hpXFid+HP_POSE_TS) = env->GetFieldID(hpState, "poseTimeStampNs", "J");

    // uint64_t        poseFetchTimeNs
    *(hpXFid + HP_ST_PFTS) = env->GetFieldID(hpState, "poseFetchTimeNs", "J");

    // uint64_t            expectedDisplayTimeNs;
    *(hpXFid+HP_ST_EDTS) = env->GetFieldID(hpState, "expectedDisplayTimeNs", "J");
}

void initMethodID(JNIEnv * env, jclass clazz, jmethodID* apiMethodID, jclass* apiClazz)
{
	*(apiMethodID+MID_PERFLEV_ORDINAL) = env->GetMethodID(*(apiClazz+JCLASS_PERFLEV), "ordinal", "()I");
	*(apiMethodID+MID_TEXTYPE_ORDINAL) = env->GetMethodID(*(apiClazz+JCLASS_TEXTURETYPE), "ordinal", "()I");
	*(apiMethodID+MID_EYEMASK_GETVALUE) = env->GetMethodID(*(apiClazz+JCLASS_EYEMASK), "getEyeMask", "()I");
	*(apiMethodID+MID_WARPTYPE_ORDINAL) = env->GetMethodID(*(apiClazz+JCLASS_WARPTYPE), "ordinal", "()I");
	*(apiMethodID+MID_SVRAPI_INIT) = env->GetMethodID(clazz, "<init>", "()V");
	*(apiMethodID+MID_HPSTATE_INIT) = env->GetMethodID(*(apiClazz+JCLASS_HEADPOSESTATE),
	"<init>", "(Lcom/qualcomm/svrapi/SvrApi;)V");
	*(apiMethodID+MID_HP_INIT) = env->GetMethodID(*(apiClazz+JCLASS_HEADPOSE),
	"<init>", "(Lcom/qualcomm/svrapi/SvrApi;)V");
	*(apiMethodID+MID_QUAT_INIT) = env->GetMethodID(*(apiClazz+JCLASS_QUAT),
	"<init>", "(Lcom/qualcomm/svrapi/SvrApi;)V");
	*(apiMethodID+MID_VEC3_INIT) = env->GetMethodID(*(apiClazz+JCLASS_VEC3),
	"<init>", "(Lcom/qualcomm/svrapi/SvrApi;)V");
	*(apiMethodID+MID_WHICHEYE_ORDINAL) = env->GetMethodID(*(apiClazz+JCLASS_WHICHEYE), "ordinal", "()I");
	*(apiMethodID+MID_SETRESULT) = env->GetMethodID(clazz, "setSvrResult", "(I)V");
	//device info
	*(apiMethodID+MID_DVINIT) = env->GetMethodID(*(apiClazz+JCLASS_DVINFO),
	"<init>", "(Lcom/qualcomm/svrapi/SvrApi;)V");
	// public svrViewFrustum  leftEyeFrustum; 
	*(apiMethodID+MID_VIEWFRU) = env->GetMethodID(*(apiClazz+JCLASS_VFRU),
	"<init>", "(Lcom/qualcomm/svrapi/SvrApi;)V");
	*(apiMethodID+MID_WARPMESH_ORDINAL) = env->GetMethodID(*(apiClazz+JCLASS_WARPMESHENUM), "ordinal", "()I");
	*(apiMethodID+MID_COLORSPACE_ORDINAL) = env->GetMethodID(*(apiClazz+JCLASS_COLORSPACE), "ordinal", "()I");
}

void deleteGlobalRef(JNIEnv* env, jclass* apiClazz)
{
	if (*(apiClazz+JCLASS_BPARAM)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_BPARAM));}
	if (*(apiClazz+JCLASS_FPARAM)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_FPARAM));}
	if (*(apiClazz+JCLASS_LAYOUTCOORDS)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_LAYOUTCOORDS));}
	if (*(apiClazz+JCLASS_VULKANINFO)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_VULKANINFO));}
	if (*(apiClazz+JCLASS_HEADPOSESTATE)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_HEADPOSESTATE));}
	if (*(apiClazz+JCLASS_HEADPOSE)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_HEADPOSE));}
	if (*(apiClazz+JCLASS_QUAT)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_QUAT));}
	if (*(apiClazz+JCLASS_VEC3)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_VEC3));}
	if (*(apiClazz+JCLASS_DVINFO)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_DVINFO));}
	if (*(apiClazz+JCLASS_VFRU)){
	env->DeleteGlobalRef(*(apiClazz+JCLASS_VFRU));}
}

extern "C"

{
//-----------------------------------------------------------------------------
void Java_com_qualcomm_svrapi_SvrApi_svrInitialize(JNIEnv * env, jclass clazz, jobject thiz)
//-----------------------------------------------------------------------------
{
	svrInitParams initParams;
	env->GetJavaVM(&initParams.javaVm);
	initParams.javaEnv = env;
	initParams.javaActivityObject = (jobject)env->NewGlobalRef(thiz);//thiz;
	if (svrInitialize(&initParams) != SVR_ERROR_NONE)
	{
		LOGE("svrInitialize failed!");
		env->DeleteGlobalRef(initParams.javaActivityObject);
	}
	else
	{
		memset(&beginParams, 0, sizeof(svrBeginParams));
		memset(&frameParams, 0, sizeof(svrFrameParams));
		initJclass(env, apiClazz);
		initMethodID(env, clazz, apiMethodID, apiClazz);
		//frame params
		initApiFid(env, apiFiledID,
		apiClazz[JCLASS_BPARAM], apiClazz[JCLASS_FPARAM], apiClazz[JCLASS_EYELAYER]);
		//layoutCoords
		initLayoutCoordsFid(env, lCoordsFid, apiClazz[JCLASS_LAYOUTCOORDS]);
		//head pose state 
		initHeadPoseStateFid(env, hpXFid, apiClazz[JCLASS_FPARAM], apiClazz[JCLASS_HEADPOSESTATE],
		apiClazz[JCLASS_HEADPOSE], apiClazz[JCLASS_QUAT], apiClazz[JCLASS_VEC3]);
		//device info
		initDeviceInfoFid(env, dvFid, apiClazz[JCLASS_DVINFO], apiClazz[JCLASS_VFRU]);
	}
}


//-----------------------------------------------------------------------------
void Java_com_qualcomm_svrapi_SvrApi_svrBeginVr(JNIEnv * env, jclass clazz, jobject thiz, jobject mBeginParamsObj)
//-----------------------------------------------------------------------------
{

	//get surface
	jobject surface = env->GetObjectField(mBeginParamsObj, apiFiledID[FID_BPARAM_SURFACE]);
	ANativeWindow * nativeWindow = ANativeWindow_fromSurface(env, surface);
	beginParams.nativeWindow = nativeWindow;

	//get cpu/gpu perf level
	jobject cpuPerfLevelObj = env->GetObjectField(mBeginParamsObj, apiFiledID[FID_BPARAM_CPUPERF]);
	beginParams.cpuPerfLevel = (svrPerfLevel)env->CallIntMethod(cpuPerfLevelObj, apiMethodID[MID_PERFLEV_ORDINAL]);

	jobject gpuPerfLevelObj = env->GetObjectField(mBeginParamsObj, apiFiledID[FID_BPARAM_GPUPERF]);
	beginParams.gpuPerfLevel = (svrPerfLevel)env->CallIntMethod(gpuPerfLevelObj, apiMethodID[MID_PERFLEV_ORDINAL]);

	beginParams.optionFlags = env->GetIntField(mBeginParamsObj, apiFiledID[FID_BPARAM_OPTIONFLAGS]);

	jobject colorSpaceObj = env->GetObjectField(mBeginParamsObj, apiFiledID[FID_BPARAM_COLORSPACE]);
	beginParams.colorSpace = (svrColorSpace)env->CallIntMethod(colorSpaceObj,apiMethodID[MID_COLORSPACE_ORDINAL]);

	beginParams.mainThreadId = gettid();

	if (svrBeginVr(&beginParams) != SVR_ERROR_NONE)
	{
		LOGE("svr begin VR failed!");
		env->DeleteLocalRef(surface);
		deleteGlobalRef(env,apiClazz);
		return;
	}

}

//-----------------------------------------------------------------------------
void Java_com_qualcomm_svrapi_SvrApi_svrSubmitFrame(JNIEnv * env, jclass clazz, jobject thiz, jobject frameParamsObj)
//-----------------------------------------------------------------------------
{
	//get frameIndex
	frameParams.frameIndex =
	env->GetIntField(frameParamsObj, apiFiledID[FID_FPARAM_FRAMEINDEX]);
	//get minVsyncs
	frameParams.minVsyncs =
	env->GetIntField(frameParamsObj, apiFiledID[FID_FPARAM_MINVSYNC]);
	jobjectArray eyeLayersObjArr = (jobjectArray)env->GetObjectField(frameParamsObj, apiFiledID[FID_FPARAM_EYELAYER]);
	jobject eyeLayersObj[SVR_MAX_RENDER_LAYERS];
	jobject imageTypeObj,imageCoordsObj,eyeMaskObj,vulkanInfoObj;

	for (int i=0; i < SVR_MAX_RENDER_LAYERS; i++)
	{
		eyeLayersObj[i] = env->GetObjectArrayElement(eyeLayersObjArr, i);
		frameParams.renderLayers[i].imageHandle =
        env->GetIntField(eyeLayersObj[i], apiFiledID[FID_RENDERLAYER_IMAGEHANDLE]);
		// //public svrTextureType      imageType;
		imageTypeObj = env->GetObjectField(eyeLayersObj[i], apiFiledID[FID_RENDERLAYER_IMAGETYPE]);
		frameParams.renderLayers[i].imageType =
		(svrTextureType)env->CallIntMethod(imageTypeObj, apiMethodID[MID_TEXTYPE_ORDINAL]);
		//imageCoords
		imageCoordsObj = env->GetObjectField(eyeLayersObj[i], apiFiledID[FID_RENDERLAYER_IMAGECOORDS]);
		getImageCoords(env, imageCoordsObj, frameParams.renderLayers[i].imageCoords, lCoordsFid);
		//public svrEyeMask          eyeMask;            //!< Determines which eye[s] receive this render layer
		eyeMaskObj = env->GetObjectField(eyeLayersObj[i], apiFiledID[FID_RENDERLAYER_EYEMASK]);
		frameParams.renderLayers[i].eyeMask = 
		(svrEyeMask)env->CallIntMethod(eyeMaskObj, apiMethodID[MID_EYEMASK_GETVALUE]);
		//layerFlags
		frameParams.renderLayers[i].layerFlags = env->GetIntField(eyeLayersObj[i], apiFiledID[FID_RENDERLAYER_FLAG]);
		//vulkanInfo
		vulkanInfoObj = env->GetObjectField(eyeLayersObj[i], apiFiledID[FID_RENDERLAYER_VULKANINFO]);
		if (vulkanInfoObj != NULL) {
			getVulkanTexInfo(env, apiClazz[JCLASS_VULKANINFO], vulkanInfoObj, frameParams.renderLayers[i].vulkanInfo);
		} else {
			LOGE("vulkanInfoObj NULL");
		}
	}
	//public int  frameOptions; 
	frameParams.frameOptions = env->GetIntField(frameParamsObj, apiFiledID[FID_FPARAM_FRAMEOPT]);
	//public svrHeadPoseState    headPoseState;
	getHeadPoseState(env, apiClazz[JCLASS_FPARAM], frameParamsObj, frameParams, apiClazz[JCLASS_HEADPOSESTATE],
	apiClazz[JCLASS_HEADPOSE], apiClazz[JCLASS_VEC3], apiClazz[JCLASS_QUAT], hpXFid);
	//public  svrWarpType         warpType; 
	jobject warpTypeObj = env->GetObjectField(frameParamsObj, apiFiledID[FID_FPARAM_WARPTYPE]);
	frameParams.warpType =
	(svrWarpType)env->CallIntMethod(warpTypeObj, apiMethodID[MID_WARPTYPE_ORDINAL]);
	//public float                fieldOfView;
	frameParams.fieldOfView = env->GetFloatField(frameParamsObj, apiFiledID[FID_FRARAM_FOV]);
	svrSubmitFrame(&frameParams);
	env->DeleteLocalRef(eyeLayersObjArr);
	env->DeleteLocalRef(warpTypeObj);
	for (int i=0; i < SVR_MAX_RENDER_LAYERS; i++)
	{
		env->DeleteLocalRef(eyeLayersObj[i]);
	}
	env->DeleteLocalRef(imageTypeObj);
	env->DeleteLocalRef(imageCoordsObj);
	env->DeleteLocalRef(eyeMaskObj);
	env->DeleteLocalRef(vulkanInfoObj);
}

void Java_com_qualcomm_svrapi_SvrApi_svrEndVr(JNIEnv * env, jclass clazz)

{
	if (svrEndVr() != SVR_ERROR_NONE)
	{
		LOGE("svr end VR failed!");
    }
}

void Java_com_qualcomm_svrapi_SvrApi_svrShutdown(JNIEnv * env, jclass clazz)
{
	if (svrShutdown() != SVR_ERROR_NONE)
	{
		LOGE("svr end VR failed!");
	}
	deleteGlobalRef(env,apiClazz);
}

JNIEXPORT jstring JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetVersion(JNIEnv * env, jclass clazz)

{
	char buf[256];
	memset(buf, 0, sizeof(buf));
	strcpy(buf, svrGetVersion());
	jstring jstrBuf = env->NewStringUTF(buf);
	return jstrBuf;
}

JNIEXPORT jstring JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetVrServiceVersion(JNIEnv * env, jclass clazz)

{
	char buf[256];
	memset(buf, 0, sizeof(buf));
	svrGetVrServiceVersion(buf, 100);
	jstring jstrBuf = env->NewStringUTF(buf);
	return jstrBuf;
}

JNIEXPORT jstring JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetVrClientVersion(JNIEnv * env, jclass clazz)

{
	char buf[256];
	memset(buf, 0, sizeof(buf));
	svrGetVrClientVersion(buf, 100);
	jstring jstrBuf = env->NewStringUTF(buf);
	return jstrBuf;
}

JNIEXPORT jfloat JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetPredictedDisplayTime(JNIEnv * env, jclass clazz)

{
	jfloat predictedTimeMs = svrGetPredictedDisplayTime();
	LOGI("Predicted Display Time : %0.3f", predictedTimeMs);
	return predictedTimeMs;
}

JNIEXPORT jfloat JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetPredictedDisplayTimePipelined(JNIEnv * env, jclass clazz, jint depth)

{
	jfloat predictedTimeMs = svrGetPredictedDisplayTimePipelined(depth);
	LOGI("Predicted Display Time : %0.3f", predictedTimeMs);
	return predictedTimeMs;
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrGetPredictedHeadPose
 * Signature: (F)Lcom/qualcomm/svrapi/SvrApi/svrHeadPoseState;
 */
JNIEXPORT jobject JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetPredictedHeadPose(JNIEnv * env, jclass clazz, jfloat predictedTimeMs)
{
	svrHeadPoseState poseState = svrGetPredictedHeadPose(predictedTimeMs);

	glm::fquat poseQuat = glm::fquat(poseState.pose.rotation.w,
    poseState.pose.rotation.x, poseState.pose.rotation.y, poseState.pose.rotation.z);
	glm::mat4 poseMat = glm::mat4_cast(poseQuat);
	//svrApi init
	jobject svrApiObj = env->NewObject(clazz, apiMethodID[MID_SVRAPI_INIT]);

	jobject svrHeadPoseStateObj = env->NewObject(apiClazz[JCLASS_HEADPOSESTATE], apiMethodID[MID_HPSTATE_INIT], svrApiObj);
	//poseStatus
	env->SetIntField(svrHeadPoseStateObj, hpXFid[HP_POSE_STATUS], poseState.poseStatus);
	//poseTimeStampNs
	env->SetLongField(svrHeadPoseStateObj, hpXFid[HP_POSE_TS], poseState.poseTimeStampNs);
    //poseFetchTimeNs
    env->SetLongField(svrHeadPoseStateObj, hpXFid[HP_ST_PFTS], poseState.poseFetchTimeNs);
	//expectedDisplayTimeNs
	env->SetLongField(svrHeadPoseStateObj, hpXFid[HP_ST_EDTS], poseState.expectedDisplayTimeNs);
	//handle svrHeadPose
	jobject svrHeadPoseObj = env->NewObject(apiClazz[JCLASS_HEADPOSE], apiMethodID[MID_HP_INIT], svrApiObj);
	//get rotation fid in svrHeadPose
	jobject svrQuaternionObj = env->NewObject(apiClazz[JCLASS_QUAT], apiMethodID[MID_QUAT_INIT], svrApiObj);
	// //get x in svrQuaternion
	env->SetFloatField(svrQuaternionObj, hpXFid[HP_ROTATION_X], poseState.pose.rotation.x );
	//get y in svrQuaternion
	env->SetFloatField(svrQuaternionObj, hpXFid[HP_ROTATION_Y], poseState.pose.rotation.y );
	//get z in svrQuaternion
	env->SetFloatField(svrQuaternionObj, hpXFid[HP_ROTATION_Z], poseState.pose.rotation.z );
	//get w in svrQuaternion
	env->SetFloatField(svrQuaternionObj, hpXFid[HP_ROTATION_W], poseState.pose.rotation.w );
	//return svrQuaternionObj;
	env->SetObjectField(svrHeadPoseObj, hpXFid[HP_ROTATION], svrQuaternionObj);
	//create svrVector3 object
	jobject svrVector3Obj = env->NewObject(apiClazz[JCLASS_VEC3], apiMethodID[MID_VEC3_INIT], svrApiObj);

	setVec3Object(env, svrVector3Obj, hpXFid, poseState.pose.position);

	env->SetObjectField(svrHeadPoseObj, hpXFid[HP_POSITION], svrVector3Obj);
	// pose fid in svrHeadPoseState
	env->SetObjectField(svrHeadPoseStateObj, hpXFid[HP_POSE], svrHeadPoseObj);

	env->DeleteLocalRef(svrApiObj);
	env->DeleteLocalRef(svrHeadPoseObj);
	env->DeleteLocalRef(svrQuaternionObj);
	env->DeleteLocalRef(svrVector3Obj);

	return svrHeadPoseStateObj;
}

void setSvrResultValue(JNIEnv * env, jclass clazz, jint setResult)
{
	jobject svrApiObj = env->NewObject(clazz, apiMethodID[MID_SVRAPI_INIT]);
	env->CallVoidMethod(svrApiObj, apiMethodID[MID_SETRESULT], (jint)setResult);
	env->DeleteLocalRef(svrApiObj);
}

void setSvrWarpMeshType(JNIEnv * env, jclass clazz, jint setWarpMeshType)
{
	jobject svrApiObj = env->NewObject(clazz, apiMethodID[MID_SVRAPI_INIT]);
	// create setWarpMeshTypeMid Obj
	jmethodID setWarpMeshTypeMid = env->GetMethodID(clazz, "setSvrWarpMeshType", "(I)V");
	env->CallVoidMethod(svrApiObj, setWarpMeshTypeMid, (jint)setWarpMeshType);
	env->DeleteLocalRef(svrApiObj);
}
/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrSetPerformanceLevels
 * Signature: (Lcom/qualcomm/svrapi/SvrApi/svrPerfLevel;
 *Lcom/qualcomm/svrapi/SvrApi/svrPerfLevel;)Lcom/qualcomm/svrapi/SvrApi/SvrResult;*/

JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrSetPerformanceLevels(JNIEnv * env, jclass clazz, jobject cpuPerfLevel, jobject gpuPerfLevel)
{
	svrPerfLevel cpuPerfLev = (svrPerfLevel)env->CallIntMethod(cpuPerfLevel, apiMethodID[MID_PERFLEV_ORDINAL]);
	svrPerfLevel gpuPerfLev = (svrPerfLevel)env->CallIntMethod(gpuPerfLevel, apiMethodID[MID_PERFLEV_ORDINAL]);

	SvrResult setPerfResult = svrSetPerformanceLevels(cpuPerfLev, gpuPerfLev);
	setSvrResultValue(env, clazz, setPerfResult);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrGetDeviceInfo
 * Signature: ()Lcom/qualcomm/svrapi/SvrApi/svrDeviceInfo;
 */
JNIEXPORT jobject JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetDeviceInfo(JNIEnv * env, jclass clazz)
{
	svrDeviceInfo deviceInfo = svrGetDeviceInfo();
	//svrApi init
	jobject svrApiObj = env->NewObject(clazz, apiMethodID[MID_SVRAPI_INIT]);
	jobject svrDeviceInfoObj = env->NewObject(apiClazz[JCLASS_DVINFO], apiMethodID[MID_DVINIT], svrApiObj);

	// public int             displayWidthPixels; 
	env->SetIntField(svrDeviceInfoObj, dvFid[DV_WIDTH], deviceInfo.displayWidthPixels);
	// public int             displayHeightPixels; 
	env->SetIntField(svrDeviceInfoObj, dvFid[DV_HIGHT], deviceInfo.displayHeightPixels);
	// public float           displayRefreshRateHz; 
	env->SetFloatField(svrDeviceInfoObj, dvFid[DV_FPS], deviceInfo.displayRefreshRateHz);
	// public int             displayOrientation; 
	env->SetIntField(svrDeviceInfoObj, dvFid[DV_ORI], deviceInfo.displayOrientation);
	// public int             targetEyeWidthPixels;  
	env->SetIntField(svrDeviceInfoObj, dvFid[DV_EYE_WIDTH], deviceInfo.targetEyeWidthPixels);
	// public int             targetEyeHeightPixels; 
	env->SetIntField(svrDeviceInfoObj, dvFid[DV_EYE_HIGHT], deviceInfo.targetEyeHeightPixels);
	// public float           targetFovXRad; 
	env->SetFloatField(svrDeviceInfoObj, dvFid[DV_FOV_XRAD], deviceInfo.targetFovXRad);
	// public float           targetFovYRad; 
	env->SetFloatField(svrDeviceInfoObj, dvFid[DV_FOV_YRAD], deviceInfo.targetFovYRad);

	// public svrViewFrustum  leftEyeFrustum; 
	jobject svrViewFrustumObj = env->NewObject(apiClazz[JCLASS_VFRU], apiMethodID[MID_VIEWFRU], svrApiObj);
	setViewFrustum(env, svrViewFrustumObj, dvFid, deviceInfo.leftEyeFrustum);
	env->SetObjectField(svrDeviceInfoObj, dvFid[DV_VFRUM_LEFT], svrViewFrustumObj);

	// public svrViewFrustum  rightEyeFrustum;  
	setViewFrustum(env, svrViewFrustumObj, dvFid, deviceInfo.rightEyeFrustum);
	env->SetObjectField(svrDeviceInfoObj, dvFid[DV_VFRUM_RIGHT], svrViewFrustumObj);

	// public int             deviceOSVersion; 
	env->SetIntField(svrDeviceInfoObj, dvFid[DV_OSVERSION], deviceInfo.deviceOSVersion);

	// public float           targetEyeConvergence;              
	env->SetFloatField(svrDeviceInfoObj, dvFid[DV_EYE_CONV], deviceInfo.targetEyeConvergence);
	// public float           targetEyePitch; 
	env->SetFloatField(svrDeviceInfoObj, dvFid[DV_EYE_PITCH], deviceInfo.targetEyePitch);

	setSvrWarpMeshType(env, clazz, deviceInfo.warpMeshType);

	env->DeleteLocalRef(svrApiObj);
	env->DeleteLocalRef(svrViewFrustumObj);
	return svrDeviceInfoObj;

}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrRecenterPose
 * Signature: ()Lcom/qualcomm/svrapi/SvrApi/SvrResult;
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrRecenterPose(JNIEnv * env, jclass clazz)
{
	SvrResult svrResultValue = svrRecenterPose();
	setSvrResultValue(env, clazz, svrResultValue);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrRecenterPosition
 * Signature: ()Lcom/qualcomm/svrapi/SvrApi/SvrResult;
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrRecenterPosition(JNIEnv * env, jclass clazz)
{
	SvrResult svrResultValue = svrRecenterPosition();
	setSvrResultValue(env, clazz, svrResultValue);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrRecenterOrientation
 * Signature: (Z)Lcom/qualcomm/svrapi/SvrApi/SvrResult;
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrRecenterOrientation(JNIEnv * env, jclass clazz, jboolean yawOnly)
{
	SvrResult svrResultValue = svrRecenterOrientation(yawOnly);
	setSvrResultValue(env, clazz, svrResultValue);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrGetSupportedTrackingModes
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetSupportedTrackingModes(JNIEnv * env, jclass clazz)
{
	jint supportedTrackingModes =  svrGetSupportedTrackingModes();
	return supportedTrackingModes;
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrSetTrackingMode
 * Signature: (I)Lcom/qualcomm/svrapi/SvrApi/SvrResult;
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrSetTrackingMode(JNIEnv * env, jclass clazz, jint trackingModes)
{
	SvrResult svrResultValue = svrSetTrackingMode(trackingModes);
	setSvrResultValue(env, clazz, svrResultValue);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrGetTrackingMode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetTrackingMode(JNIEnv * env, jclass clazz)
{
	jint trackingMode =  svrGetTrackingMode();
	return trackingMode;
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrBeginEye
 * Signature: (Lcom/qualcomm/svrapi/SvrApi/svrEyeBufferType;
 *Lcom/qualcomm/svrapi/SvrApi/svrWhichEye;)Lcom/qualcomm/svrapi/SvrApi/SvrResult;
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrBeginEye(JNIEnv * env, jclass clazz,jobject whichEye)
{
	svrWhichEye svrWhichEyeValue = (svrWhichEye)env->CallIntMethod(whichEye, apiMethodID[MID_WHICHEYE_ORDINAL] );
	SvrResult svrResultValue = svrBeginEye(svrWhichEyeValue);
	setSvrResultValue(env, clazz, svrResultValue);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrEndEye
 * Signature: (Lcom/qualcomm/svrapi/SvrApi/svrEyeBufferType;
 *Lcom/qualcomm/svrapi/SvrApi/svrWhichEye;)Lcom/qualcomm/svrapi/SvrApi/SvrResult;
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrEndEye(JNIEnv * env, jclass clazz,jobject whichEye)
{
	svrWhichEye svrWhichEyeValue = (svrWhichEye)env->CallIntMethod(whichEye, apiMethodID[MID_WHICHEYE_ORDINAL]);
	SvrResult svrResultValue = svrEndEye(svrWhichEyeValue);
	setSvrResultValue(env, clazz, svrResultValue);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrSetWarpMesh
 * Signature: 
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrSetWarpMesh(JNIEnv * env, jclass clazz, jobject whichMesh,
jfloatArray pVertexData, jint vertexSize, jint nVertices, jintArray pIndices, jint nIndices)
{
	svrWarpMeshEnum warpMeshEnum = 
	(svrWarpMeshEnum)env->CallIntMethod(whichMesh, apiMethodID[MID_WARPMESH_ORDINAL]);

	// jsize vertexDataLen = env->GetArrayLength(pVertexData);
	jfloat* pVertexDataInJava = env->GetFloatArrayElements(pVertexData, NULL);

	// jsize indicesLen = env->GetArrayLength(pIndices);
	jint* pIndicesInJava = env->GetIntArrayElements(pIndices, NULL);

	SvrResult svrResultValue = svrSetWarpMesh(warpMeshEnum, pVertexDataInJava,
	(int)vertexSize, (int)nVertices, (unsigned int*)pIndicesInJava, (int)nIndices);
	setSvrResultValue(env, clazz, svrResultValue);
	env->ReleaseFloatArrayElements(pVertexData, pVertexDataInJava, 0);
	env->ReleaseIntArrayElements(pIndices, pIndicesInJava, 0);
}

/*
 * Class:     com_qualcomm_svrapi_SvrApi
 * Method:    svrGetOcclusionMesh
 * Signature: 
 */
JNIEXPORT void JNICALL
Java_com_qualcomm_svrapi_SvrApi_svrGetOcclusionMesh(JNIEnv * env, jclass clazz, jobject whichEye,
jintArray pTriangleCount, jintArray pVertexStride, jfloatArray pTriangles)
{
	svrWhichEye svrWhichEyeValue = (svrWhichEye)env->CallIntMethod(whichEye, apiMethodID[MID_WHICHEYE_ORDINAL]);

	// jsize pTriangleCountLen = env->GetArrayLength(pTriangleCount);
	jint* pTriangleCountInJava = env->GetIntArrayElements(pTriangleCount, NULL);

	// jsize pVertexStrideLen = env->GetArrayLength(pVertexStride);
	jint* pVertexStrideInJava = env->GetIntArrayElements(pVertexStride, NULL);

	// jsize pTrianglesLen = env->GetArrayLength(pTriangles);
	jfloat* pTrianglesInJava = env->GetFloatArrayElements(pTriangles, NULL);

	SvrResult svrResultValue = svrGetOcclusionMesh(svrWhichEyeValue, pTriangleCountInJava,
	pVertexStrideInJava, pTrianglesInJava);
	setSvrResultValue(env, clazz, svrResultValue);
	
	env->ReleaseIntArrayElements(pTriangleCount, pTriangleCountInJava, 0);
	env->ReleaseIntArrayElements(pVertexStride, pVertexStrideInJava, 0);
	env->ReleaseFloatArrayElements(pTriangles, pTrianglesInJava, 0);
}

}//end of extern C