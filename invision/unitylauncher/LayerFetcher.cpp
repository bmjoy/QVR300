#include "LayerFetcher.h"
#include <sys/un.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

namespace UnityLauncher {
    void updateLayerData(void *context, const layer_data_t *data_t) {
        LayerFetcher *fetcher = (LayerFetcher *) context;
        fetcher->onUpdateLayerData(data_t);
    }

    void updateLayerBuffer(void *context, int layerID, const AHardwareBuffer *hardwareBuffer) {
        LayerFetcher *fetcher = (LayerFetcher *) context;
        fetcher->onUpdateLayerBuffer(layerID, hardwareBuffer);
    }

    void LayerFetcher::onUpdateLayerData(const layer_data_t *data_t) {
        WLOGI("LayerFetcher::onUpdateLayerData %d", data_t->layerId);
        uint32_t layerId = 0;
        layerId = data_t->layerId;

        bool bRemoveLayer = layerId & (1 << 31);
        if (bRemoveLayer) {
            layerId = layerId & ~(1 << 31);
            WLOGI("LayerFetcher::onUpdateLayerData remove layerId=%u", layerId);
        }

        std::unique_lock<std::mutex> autoLock(mDataMutex);
        mLayerId = layerId;
        if (bRemoveLayer) {
            if (mHardwareBufferMap.find(mLayerId) != mHardwareBufferMap.end()) {
                AHardwareBuffer_release(mHardwareBufferMap[mLayerId]);
                mHardwareBufferMap.erase(mLayerId);
            }
            if (mEGLImageMap.find(mLayerId) != mEGLImageMap.end()) {
                eglDestroyImageKHR(mEGLDisplay, mEGLImageMap[layerId]);
                mEGLImageMap.erase(mLayerId);
            }
            if (mLayerDataMap.find(mLayerId) != mLayerDataMap.end()) {
                mLayerDataMap.erase(mLayerId);
            }
            return;
        }

        LayerData layerData;
        layerData.layerId = mLayerId;
        layerData.parentLayerId = data_t->parentLayerId;

        layerData.modelMatrix = glm::make_mat4(data_t->modelMatrix);
        layerData.editFlag = data_t->editFlag;
        layerData.z = data_t->z;
        memcpy(layerData.regionArray, data_t->regionArray, sizeof(int) * 4);
        layerData.alpha = data_t->alpha;
        layerData.bOpaque = data_t->bOpaque;
        layerData.bHasTransparentRegion = data_t->bHasTransparentRegion;
        layerData.textureTransform = data_t->textureTransform;
        layerData.taskId = data_t->taskId;
        mLayerDataMap[mLayerId] = layerData;
        WLOGI("LayerFetcher::receiveLayerData done mLayerId=%d, parentLayerId=%d, editFlag=%u, taskId=%u, z=%d, region=[%d, %d, %d, %d], alpha=%f, bOpaque=%d, bHasTransparentRegion=%d, textureTransform=%u modelMatrix=[%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]",
              mLayerId, layerData.parentLayerId, layerData.editFlag, layerData.taskId, layerData.z,
              layerData.regionArray[0],
              layerData.regionArray[1], layerData.regionArray[2], layerData.regionArray[3],
              layerData.alpha, layerData.bOpaque, layerData.bHasTransparentRegion,
              layerData.textureTransform,
              layerData.modelMatrix[0][0], layerData.modelMatrix[1][0], layerData.modelMatrix[2][0],
              layerData.modelMatrix[3][0],
              layerData.modelMatrix[0][1], layerData.modelMatrix[1][1], layerData.modelMatrix[2][1],
              layerData.modelMatrix[3][1],
              layerData.modelMatrix[0][2], layerData.modelMatrix[1][2], layerData.modelMatrix[2][2],
              layerData.modelMatrix[3][2],
              layerData.modelMatrix[0][3], layerData.modelMatrix[1][3], layerData.modelMatrix[2][3],
              layerData.modelMatrix[3][3]);
    }

    void LayerFetcher::onUpdateLayerBuffer(int layerID, const AHardwareBuffer *buffer) {
        WLOGE("LayerFetcher::onUpdateLayerBuffer layerid = %d", layerID);
        AHardwareBuffer *hardwareBuffer = const_cast<AHardwareBuffer *>(buffer);
        //this must call in this fuction,if AHardwareBuffer_acquire not call, the buffer may be realse in graphicbuffer(see AHardwareBuffer_recvHandleFromUnixSocket)
        AHardwareBuffer_acquire(hardwareBuffer);
        AHardwareBuffer_Desc hardwareBufferDesc;
        AHardwareBuffer_describe(hardwareBuffer, &hardwareBufferDesc);
        EGLClientBuffer cbuf = eglGetNativeClientBufferANDROID(hardwareBuffer);
        mEglImageAttrs[7] = hardwareBufferDesc.width;
        mEglImageAttrs[9] = hardwareBufferDesc.height;

        bool bNeedCreate = true;
        std::unique_lock<std::mutex> autoLock(mDataMutex);
        if (mHardwareBufferMap.find(layerID) == mHardwareBufferMap.end()) {
            mHardwareBufferMap.emplace(layerID, hardwareBuffer);
        } else if (mHardwareBufferMap[layerID] != hardwareBuffer) {
            AHardwareBuffer_release(mHardwareBufferMap[layerID]);
            mHardwareBufferMap[layerID] = hardwareBuffer;
        } else {
            bNeedCreate = false;
        }

        if (bNeedCreate) {
            EGLImageKHR image = eglCreateImageKHR(mEGLDisplay, EGL_NO_CONTEXT,
                                                  EGL_NATIVE_BUFFER_ANDROID,
                                                  cbuf, mEglImageAttrs);
            if (EGL_NO_IMAGE_KHR == image) {
                EGLint error = eglGetError();
                WLOGE("LayerFetcher::receiveLayerHardwareBuffer Failed for creating EGLImage: %#x",
                      error);
            } else {
                if (mEGLImageMap.find(layerID) == mEGLImageMap.end()) {
                    mEGLImageMap.emplace(layerID, image);
                    WLOGI("LayerFetcher::onUpdateLayerBuffer insert image, layerId=%u", layerID);
                } else {
                    if (image != mEGLImageMap[layerID]) {
                        eglDestroyImageKHR(mEGLDisplay, mEGLImageMap[layerID]);
                        mEGLImageMap[layerID] = image;
                        WLOGI("LayerFetcher::onUpdateLayerBuffer update image, layerId=%u",
                              layerID);
                    }
                }
            }
        }
    }

    int LayerFetcher::init() {
        WLOGI("LayerFetcher::init start");

        invisionsf_client_callback_t *callback = (invisionsf_client_callback_t *)
                malloc(sizeof(invisionsf_client_callback_t));

        if (callback) {
            callback->context = this;
            callback->updateLayerData = updateLayerData;
            callback->updateLayerBuffer = updateLayerBuffer;
            helper = InVisionSFClient_Create(callback);
            if (helper) {
                WLOGE("invisionsf_client_helper_t helper create success");
            } else {
                WLOGE("invisionsf_client_helper_t helper create fail");
            }
        }
        mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        mEglImageAttrs[0] = EGL_IMAGE_PRESERVED_KHR;
        mEglImageAttrs[1] = EGL_TRUE;
        mEglImageAttrs[2] = 0x3148; // EGL_IMAGE_CROP_LEFT_ANDROID
        mEglImageAttrs[4] = 0x3149; // EGL_IMAGE_CROP_TOP_ANDROID
        mEglImageAttrs[6] = 0x314A; // EGL_IMAGE_CROP_RIGHT_ANDROID
        mEglImageAttrs[8] = 0x314B; // EGL_IMAGE_CROP_BOTTOM_ANDROID
        mEglImageAttrs[10] = EGL_NONE;

        WLOGI("LayerFetcher::init done successfully");
        return 0;
    }

    std::vector<uint8_t *> *LayerFetcher::getLayerData() {
        if (mErrorCode < 0) {
            return nullptr;
        }
        return &mLayerDataVector;
    }

    std::mutex *LayerFetcher::getDataMutex() {
        return &mDataMutex;
    }

    std::vector<AHardwareBuffer *> *LayerFetcher::getHardwareBufferVector() {
        return &mHardwareBufferVector;
    }

    std::vector<EGLImageKHR> *LayerFetcher::getEGLImageVector() {
        return &mEGLImageVector;
    }

    std::unordered_map<uint32_t, AHardwareBuffer *> *LayerFetcher::getHardwareBufferMap() {
        return &mHardwareBufferMap;
    }

    std::unordered_map<uint32_t, EGLImageKHR> *LayerFetcher::getEGLImageMap() {
        return &mEGLImageMap;
    }

    std::unordered_map<uint32_t, LayerData> *LayerFetcher::getLayerDataMap() {
        return &mLayerDataMap;
    }

    void LayerFetcher::printGlmMat4(const char *matName, glm::mat4 &mat4) {
        WLOGI("%s = [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]", matName,
              mat4[0][0], mat4[1][0], mat4[2][0], mat4[3][0],
              mat4[0][1], mat4[1][1], mat4[2][1], mat4[3][1],
              mat4[0][2], mat4[1][2], mat4[2][2], mat4[3][2],
              mat4[0][3], mat4[1][3], mat4[2][3], mat4[3][3]);
    }

    void LayerFetcher::updateModelMatrix(uint32_t layerId, float *modelMatrixArray) {
        WLOGI("LayerFetcher::updateModelMatrix start layerId=%d modelMatrixArray=%p", layerId,
              modelMatrixArray);
        WLOGI("LayerFetcher::updateModelMatrix modelMatrixArray[0-3]=[%f, %f, %f, %f]",
              modelMatrixArray[0], modelMatrixArray[1], modelMatrixArray[2], modelMatrixArray[3]);
        helper->client->ops->updateModelMatrix(layerId, modelMatrixArray);
        WLOGI("LayerFetcher::updateModelMatrix done");
    }

    void LayerFetcher::sendActionBarCmd(uint32_t layerId, int cmd) {
        WLOGI("LayerFetcher::sendActionBarCmd start layerId=%u, cmd=%d", layerId, cmd);
        helper->client->ops->sendActionBarCmd(layerId, cmd);
        WLOGI("LayerFetcher::sendActionBarCmd done");
    }

    void LayerFetcher::injectMotionEvent(uint32_t layerId, int displayID, int action, long ts, float x, float y) {
        helper->client->ops->injectIvMotionEvent(layerId, displayID, action, ts, x, y);
    }
}