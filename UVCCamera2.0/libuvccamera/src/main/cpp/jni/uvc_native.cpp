#include <jni.h>
#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <cmath>

#include "libuvc.h"
#include <android/log.h>
#include <MyGLRenderContext.h>
#include <android/asset_manager_jni.h>

#include "hidapi.h"

#include "LogUtil.h"

#include "ARCORE_API.h"

static uvc_device_handle_t *  mDeviceHandle;
static char*                  mUsbFs;
static uvc_context_t*         mContext;
static uvc_device_t*          mDevice;
static int                    mFd;

#define DEBUG_LOG 1
#define DEBUG_FILE 0


//FILE *fp_uvc = NULL;
extern "C" JNIEXPORT jstring JNICALL
Java_com_serenegiant_helper_Native_stringFromJNI(
        JNIEnv* env,
        jclass /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

static char tmp_data[1280*480];
 void uvcFrameCallback(uvc_frame_t* pFrame, void *pUserData)
{
    if (pFrame) {

        memcpy(tmp_data, pFrame->data, sizeof(tmp_data));
        char *metadata = (char*)pFrame->data + 1280*480;
        unsigned long long timestamp = *(unsigned long long*)(metadata);
        double feedTimeStamp = (double )timestamp * 1e-9;
        LOGCATD("---------> @@uvc frame timestamp %f", feedTimeStamp);
        ARCORE_feed_images(tmp_data, feedTimeStamp);
    }
}



extern "C" JNIEXPORT jint JNICALL
Java_com_serenegiant_helper_Native_openUVCCamera(
        JNIEnv* env,
        jclass /* this */,
        jint venderId, jint productId, jint fd, jint busNum, jint devAddr,jstring usbfs) {

    const char *usbfs_path = env->GetStringUTFChars(usbfs, 0);

    uvc_error_t result = UVC_ERROR_OTHER;

    if (usbfs_path) {
        if (!mDeviceHandle && fd) {
            if (mUsbFs)
                free(mUsbFs);
            if (UNLIKELY(!mContext)) {

                result = uvc_init2(&mContext, NULL, mUsbFs);
                LOGCATD("init libuvc result = %d", result);
                if (UNLIKELY(result < 0)) {
                    LOGCATD("failed to init libuvc");
                    return result;
                }
            }

            fd = dup(fd);
            result = uvc_get_device_with_fd(mContext, &mDevice, venderId, productId, NULL, fd, busNum, devAddr);

            LOGCATD("uvc_get_device_with_fd result = %d", result);

            //fp_uvc = fopen("/sdcard/data_dump/uvc.csv", "wt");

            if (LIKELY(!result)) {
                result = uvc_open(mDevice, &mDeviceHandle);

                LOGCATD("uvc_open:err=%d", result);

                if (LIKELY(!result)) {
                    mFd = fd;
                } else {
                    LOGCATD("could not open camera:err=%d", result);
                    uvc_unref_device(mDevice);
                    //				SAFE_DELETE(mDevice);
                    mDevice = NULL;
                    mDeviceHandle = NULL;
                    close(fd);
                }
            } else {
                LOGCATD("could not find camera:err=%d", result);
                close(fd);
            }
        } else {
            LOGCATD("native-lib","camera is already opened. you should release first");
        }

        if(NULL != mDeviceHandle){
            uvc_stream_ctrl_t ctrl;
            //result = uvc_get_stream_ctrl_format_size_fps(mDeviceHandle, &ctrl,
            //                                             UVC_COLOR_FORMAT_MJPEG,
            //                                             1920, 1080, 1, 30);
			
			result = uvc_get_stream_ctrl_format_size(
								  mDeviceHandle, &ctrl, /* result stored in ctrl */
								  UVC_FRAME_FORMAT_ANY, /* YUV 422, aka YUV 4:2:2. try _COMPRESSED */
								  640, 481, 30  /* width, height, fps */
								  //1280, 800, 25
							  );
            LOGCATD("uvc_get_stream_ctrl_format_size result = %d",result );
            if(0 == result)	{
                result = uvc_start_streaming_bandwidth(mDeviceHandle, &ctrl, uvcFrameCallback,
                                                       NULL,  1.0f, 0);

                LOGCATD("uvc_start_streaming_bandwidth ret = %d" , result);
            }
        }

        std::string config_file_path = "/sdcard/model/arconfig/config.yaml";
        std::string vocab_file_path = "/sdcard/model/arconfig/database.bin";

        int ar_init_result = ARCORE_init(config_file_path, vocab_file_path);

        LOGCATD("ARCORE_init result : %d" , ar_init_result);
    }

    return result;
}



#pragma pack(push)
#pragma pack(2)
typedef struct OV580IMUPacket
{
	unsigned short imuID;
	unsigned short sampleID;
	unsigned short temperature;

	unsigned long long gyroTimestamp;
	unsigned int gyroNumerator;
	unsigned int gyroDenominator;
	int gyroX;
	int gyroY;
	int gyroZ;


	unsigned long long accelTimestamp;
	unsigned int accelNumerator;
	unsigned int accelDenominator;
	int accelX;
	int accelY;
	int accelZ;

	unsigned long long magTimestamp;
	unsigned int magNumerator;
	unsigned int magDenominator;
	int magX;
	int magY;
	int magZ;

}IMUPacket;
#pragma pack(pop)


hid_device *hid_handle;

FILE *fp_imu = NULL;

extern "C" JNIEXPORT jint JNICALL
Java_com_serenegiant_helper_Native_openHid(
        JNIEnv* env,
        jclass /* this */,
        jint venderId, jint productId, jint fd, jint busNum, jint devAddr,jstring usbfs) {

    int res;
	unsigned char buf[256];
	#define MAX_STR 255
	wchar_t wstr[MAX_STR];
	int i;
	struct hid_device_info *devs, *cur_dev;
    if (hid_init()) {
        LOGCATD("hit_init falied");
        return -1;
    }

    memset(buf,0x00,sizeof(buf));
    buf[0] = 0x01;
    buf[1] = 0x81;

    hid_handle = hid_open(0x05a9, 0x0f87, NULL, fd, busNum, devAddr);
    	//handle = hid_open(0x046d, 0xc077, NULL);
    if (!hid_handle) {
        LOGCATD("unable to open device\n");
        return 1;
    }
    wstr[0] = 0x0000;
	res = hid_get_manufacturer_string(hid_handle, wstr, MAX_STR);
	if (res < 0)
        LOGCATD("Unable to read manufacturer string\n");
    LOGCATD("Manufacturer String: %ls\n", wstr);

    wstr[0] = 0x0000;
    res = hid_get_product_string(hid_handle, wstr, MAX_STR);
    if (res < 0)
        LOGCATD("native-lib","Unable to read product string\n");
    LOGCATD("native-lib","Product String: %ls\n", wstr);

    // Read the Serial Number String
    wstr[0] = 0x0000;
    res = hid_get_serial_number_string(hid_handle, wstr, MAX_STR);
    if (res < 0)
        LOGCATD("native-lib","Unable to read serial number string\n");
    LOGCATD("native-lib","Serial Number String: (%d) %ls", wstr[0], wstr);

    // Read Indexed String 1
    wstr[0] = 0x0000;
    res = hid_get_indexed_string(hid_handle, 1, wstr, MAX_STR);
    if (res < 0)
        LOGCATD("Unable to read indexed string 1\n");
    LOGCATD("Indexed String 1: %ls\n", wstr);

    res = hid_get_indexed_string(hid_handle, 5, wstr, MAX_STR);
    if (res < 0)
        LOGCATD("Unable to read indexed string 5\n");
    LOGCATD("Indexed String 5: %ls\n", wstr);

    // Set the hid_read() function to be non-blocking.
    hid_set_nonblocking(hid_handle, 0);


//    fp_imu = fopen("/sdcard/data_dump/imu.csv", "wt");
    return -1;
}

IMUPacket revPacket;

void parseImuPacket(IMUPacket* packet, void* buffer, int size)
{
	unsigned char* temp = (unsigned char*)buffer;
	if (size < sizeof(IMUPacket))
	{
#if DEBUG_LOG
        LOGCATD("Not enough buffer:size(%d) < sizeof(IMUPacket)(%d)\n", size, sizeof(IMUPacket));
        LOGCATD("uint_64:(%d) ,uint_16:(%d), uint_32:%d, int_32:%d \n"
        , sizeof(unsigned long), sizeof(unsigned short), sizeof(unsigned int), sizeof(int));
#endif
		return;
	}

	packet->imuID = (*(unsigned short*)temp);
	temp += 2;
	packet->sampleID = (*(unsigned short*)temp);
	temp += 2;
	packet->temperature = (*(unsigned short*)temp);
	temp += 2;

	packet->gyroTimestamp = ((*(unsigned long long*)temp));
	temp += 8;
	packet->gyroNumerator = (*(unsigned int*)temp);
	temp += 4;
	packet->gyroDenominator = (*(unsigned int*)temp);
	temp += 4;
	packet->gyroX = (*(int*)temp);
	temp += 4;
	packet->gyroY = (*(int*)temp);
	temp += 4;
	packet->gyroZ = (*(int*)temp);
	temp += 4;

	packet->accelTimestamp = (*(unsigned long long*)temp);
	temp += 8;
	packet->accelNumerator = (*(unsigned int*)temp);
	temp += 4;
	packet->accelDenominator = (*(unsigned int*)temp);
	temp += 4;
	packet->accelX = (*(int*)temp);
	temp += 4;
	packet->accelY = (*(int*)temp);
	temp += 4;
	packet->accelZ = (*(int*)temp);
	temp += 4;

	packet->magTimestamp = (*(unsigned long long*)temp);
	temp += 8;
	packet->magNumerator = (*(unsigned int*)temp);
	temp += 4;
	packet->magDenominator = (*(unsigned int*)temp);
	temp += 4;
	packet->magX = (*(int*)temp);
	temp += 4;
	packet->magY = (*(int*)temp);
	temp += 4;
	packet->magZ = (*(int*)temp);
	temp += 4;
}


void PrintImuPacket(IMUPacket* packet)
{
    double acc_x = packet->accelX / 2048.f * 9.81;
    double acc_y = packet->accelY / 2048.f * 9.81;
    double acc_z = packet->accelZ / 2048.f * 9.81;

    double gyro_x = packet->gyroX / 16.4 / 180.f * 3.1415926;
    double gyro_y = packet->gyroY / 16.4 / 180.f * 3.1415926;
    double gyro_z = packet->gyroZ / 16.4 / 180.f * 3.1415926;

    std::vector<float> imu;
    imu.push_back(acc_x);
    imu.push_back(acc_y);
    imu.push_back(acc_z);
    imu.push_back(gyro_x);
    imu.push_back(gyro_y);
    imu.push_back(gyro_z);


    double timeStamp = (double )(packet->gyroTimestamp) * pow(10, -9);
    ARCORE_feed_imu(imu, timeStamp);

    LOGCATD("---------> imu timestamp %f %f %f %f %f %f %f", acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, timeStamp);
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_readHid(
        JNIEnv* env,
        jclass /* this */) {
    unsigned char buf[256];

  //while (res >= 0) {
    memset(buf, 0, sizeof(buf));
    int res = hid_read(hid_handle, buf, 128);
    if (res == 0)
    {
         //__android_log_print(ANDROID_LOG_DEBUG ,"native-lib","waiting...\n");
         return;
    }
    if (res < 0)
    {
         return;
    }

    parseImuPacket(&revPacket, &buf[38], 90);
    PrintImuPacket(&revPacket);
//    usleep(700);
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_closeUVCCamera(
        JNIEnv* env,
        jclass /* this */) {

    ARCORE_release();

    if (LIKELY(mDeviceHandle)) {
        uvc_stop_streaming(mDeviceHandle);
        //SAFE_DELETE(mPreview);
        uvc_close(mDeviceHandle);
        mDeviceHandle = NULL;

        LOGCATD("uvc_stop_streaming");
    }
    if (LIKELY(mDevice)) {
        uvc_unref_device(mDevice);
        mDevice = NULL;
    }
    if (mUsbFs) {
        close(mFd);
        mFd = 0;
        free(mUsbFs);
        mUsbFs = NULL;
    }

    if(hid_handle) {
        hid_close(hid_handle);
        hid_exit();
    }

    LOGCATD("closeUVCCamera");
}


#include <opencv2/opencv.hpp>
float id_matrix[16] = { 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
cv::Mat invPose(4, 4, CV_32F, id_matrix);

float qc_matrix[16] = { 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
cv::Mat D_QC(4,4,CV_32F,qc_matrix);

extern "C" JNIEXPORT void My_GetPose(void ** context,float * pose, double predictTimeDiffNano,double &ts){
    ARCORE_get_pose(pose, 0.043f);
    LOGCATD("My_getPose before{%f %f %f}",pose[12],pose[13],pose[14]);
    cv::Mat p_cw(4,4,CV_32F,pose);
    cv::Mat p_qw = D_QC * p_cw * D_QC.t();
    memcpy(pose,p_qw.data,16*sizeof(float));
    ts = 1.0;
    LOGCATD("My_getPose{%f %f %f}",pose[12],pose[13],pose[14]);
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_getPose(
        JNIEnv* env,
        jclass /* this */,jfloatArray array) {

    static float SlamMatrix[16];
    memset(SlamMatrix, 0, sizeof(SlamMatrix));
    ARCORE_get_pose(SlamMatrix, 0.043f);

    LOGCATD("Native_getPose{%f %f %f}",SlamMatrix[12],SlamMatrix[13],SlamMatrix[14]);

    //new pose ex
    static float q[4], p[3];
    ARCORE_get_opengl_pose(q, p, 0.043f);

//    SlamMatrix[12] *= 30;
//    SlamMatrix[13] *= -30;

//    invPose.at<float>(0, 0) = SlamMatrix[0];
//    invPose.at<float>(0, 1) = SlamMatrix[1];
//    invPose.at<float>(0, 2) = SlamMatrix[2];
//    invPose.at<float>(0, 3) = SlamMatrix[3];
//    invPose.at<float>(1, 0) = SlamMatrix[4];
//    invPose.at<float>(1, 1) = SlamMatrix[5];
//    invPose.at<float>(1, 2) = SlamMatrix[6];
//    invPose.at<float>(1, 3) = SlamMatrix[7];
//    invPose.at<float>(2, 0) = SlamMatrix[8];
//    invPose.at<float>(2, 1) = SlamMatrix[9];
//    invPose.at<float>(2, 2) = SlamMatrix[10];
//    invPose.at<float>(2, 3) = SlamMatrix[11];
//    invPose.at<float>(3, 0) = -SlamMatrix[12]*10;
//    invPose.at<float>(3, 1) = -SlamMatrix[13]*10;
//    invPose.at<float>(3, 2) = -SlamMatrix[14]*10;
//    invPose.at<float>(3, 3) = SlamMatrix[15];
//
//    invPose = invPose.inv();
//    SlamMatrix[0] = invPose.at<float>(0, 0);
//    SlamMatrix[1] = invPose.at<float>(0, 1);
//    SlamMatrix[2] = invPose.at<float>(0, 2);
//    SlamMatrix[3] = invPose.at<float>(0, 3);
//
//    SlamMatrix[4] = invPose.at<float>(1, 0);
//    SlamMatrix[5] = invPose.at<float>(1, 1);
//    SlamMatrix[6] = invPose.at<float>(1, 2);
//    SlamMatrix[7] = invPose.at<float>(1, 3);
//
//    SlamMatrix[8] = invPose.at<float>(2, 0);
//    SlamMatrix[9] = invPose.at<float>(2, 1);
//    SlamMatrix[10] = invPose.at<float>(2, 2);
//    SlamMatrix[11] = invPose.at<float>(2, 3);
//
//    SlamMatrix[12] = invPose.at<float>(3, 0);
//    SlamMatrix[13] = invPose.at<float>(3, 1);
//    SlamMatrix[14] = invPose.at<float>(3, 2);
//    SlamMatrix[15] = invPose.at<float>(3, 3);

    env->SetFloatArrayRegion(array, 0, 16, SlamMatrix);
}



extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_renderInit(
        JNIEnv* env,
        jclass /* this */) {
    MyGLRenderContext::GetInstance();
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_renderUnInit(
        JNIEnv* env,
        jclass /* this */) {
    MyGLRenderContext::GetInstance()->DestroyInstance();
}


extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_renderUpdateTransformMatrix(
        JNIEnv* env,
        jclass /* this */,jfloat rotateX, jfloat rotateY, jfloat scaleX, jfloat scaleY) {
    MyGLRenderContext::GetInstance()->UpdateTransformMatrix(rotateX, rotateY, scaleX, scaleY);
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_renderOnSurfaceCreated(
        JNIEnv* env,
        jclass /* this */) {

    MyGLRenderContext::GetInstance()->OnSurfaceCreated();
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_renderOnSurfaceChanged(
        JNIEnv* env,
        jclass /* this */, jint width, jint height) {

    MyGLRenderContext::GetInstance()->OnSurfaceChanged(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_serenegiant_helper_Native_renderOnDrawFrame(
        JNIEnv* env,
        jclass /* this */) {

//    LOGCATD("---> %f:%f:%f", pose[0], pose[1], pose[2]);

    MyGLRenderContext::GetInstance()->OnDrawFrame();
}

