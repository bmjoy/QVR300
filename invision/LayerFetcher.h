//
// Created by willie on 2019/12/30.
//

#ifndef SC_LAYER_FETCHER_H
#define SC_LAYER_FETCHER_H

#include <Common.h>
#include <vector>
#include <unordered_map>
#include "InVisionMutex.h"
#include <android/hardware_buffer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <svrRenderExt.h>
#include "invisionsfclient.h"

using namespace Svr;

namespace ShadowCreator {
    struct LayerData {
        int layerId;
        int parentLayerId;
        glm::mat4 modelMatrix;
        uint32_t editFlag;
        int z;
        int regionArray[4];
        float alpha;
        bool bOpaque;
        bool bHasTransparentRegion;
        uint32_t textureTransform;
        uint32_t taskId;
    };

    struct LayerDataComparator {
        bool operator()(const std::pair<uint32_t, LayerData> &lhs,
                        const std::pair<uint32_t, LayerData> &rhs) const {
            return lhs.second.z < rhs.second.z;
        }
    };

    class LayerFetcher final {
    public:
        LayerFetcher() = default;

        ~LayerFetcher() = default;

        int init();

        int fetchingLayer();

        int releaseSocket();

        int sendConnectionSocket(float *viewMatrixArray);

        std::vector<uint8_t *> *getLayerData();

//        std::vector<int> *getLayerDataSize();

        InVisionMutex *getDataMutex();

        std::vector<AHardwareBuffer *> *getHardwareBufferVector();

        std::vector<EGLImageKHR> *getEGLImageVector();

        std::unordered_map<uint32_t, AHardwareBuffer *> *getHardwareBufferMap();

        std::unordered_map<uint32_t, EGLImageKHR> *getEGLImageMap();

//        std::unordered_map<uint32_t, glm::mat4> *getModelMatrixMap();

        std::unordered_map<uint32_t, LayerData> *getLayerDataMap();

        int setSocketBlockingMode(int socketFd, bool bBlocking);

        invisionsf_client_helper_t *helper;

        void onUpdateLayerData(const layer_data_t *data);

        void onUpdateLayerBuffer(int layerID, const AHardwareBuffer *hardwareBuffer);

        void updateModelMatrix(uint32_t layerId, float *modelMatrixArray);

        void sendActionBarCmd(uint32_t layerId, int cmd);

        void injectMotionEvent(uint32_t layerId, int displayID, int action, long ts, float x, float y);

        void printGlmMat4(const char *matName, glm::mat4 &mat4);

    private:
        int saveHardwareBuffer(AHardwareBuffer *hardwareBuffer, uint32_t layerId);

        int receiveLayerId();

        int receiveLayerData();

        void receiveLayerHardwareBuffer();

    private:
        int mErrorCode = 0;
        int mSocketFd = 0;
        std::vector<uint8_t *> mLayerDataVector;
//        std::vector<int> mLayerDataSizeVector;
        InVisionMutex mDataMutex;

        AHardwareBuffer *mHardwareBuffer = nullptr;
        uint64_t mFetchCount = 0;
        std::vector<AHardwareBuffer *> mHardwareBufferVector;
        std::vector<EGLImageKHR> mEGLImageVector;
        std::unordered_map<uint32_t, AHardwareBuffer *> mHardwareBufferMap;
        std::unordered_map<uint32_t, EGLImageKHR> mEGLImageMap;
//        std::unordered_map<uint32_t, glm::mat4> mModelMatrixMap;
        std::unordered_map<uint32_t, LayerData> mLayerDataMap;
        EGLDisplay mEGLDisplay;
        bool mBConnected = false;
        EGLint mEglImageAttrs[11] = {0};
        uint32_t mLayerId = 0;
    };
}

#endif //SC_LAYER_FETCHER_H
