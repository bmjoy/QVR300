#ifndef INVISIONSFCLIENT_H
#define INVISIONSFCLIENT_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/hardware_buffer.h>
#include <android/log.h>
typedef struct {
    int layerId;
    int parentLayerId;
    float modelMatrix[16];
    uint32_t editFlag;
    int z;
    int regionArray[4];
    float alpha;
    bool bOpaque;
    bool bHasTransparentRegion;
    uint32_t textureTransform;
    uint32_t taskId;
} layer_data_t;

#define INVISIONSF_CLIENT_LIB "libinvisionsfclient.so"

typedef struct invisionsf_client_ops {
    void (*updateViewMatrix)(float *m);

    void (*updateModelMatrix)(int layerID, float *m);

    void (*sendActionBarCmd)(int layerId, int cmd);

    bool (*queryBTControlStatus)();

    void (*connectBTControlService)();

    void (*disconnectBTControlService)();

    int (*fetchBTControlOrientation)(float *outOrientationArray);

    void (*updateHandShankRayOrigin)(float *rayOriginArray);

    void (*injectIvKeyEvent)(int layerID,int displayID,int keycode,int action,long ts);

    void (*injectIvMotionEvent)(int layerID,int displayID,int action,long ts,float x,float y);
} invisionsf_client_ops_t;

typedef struct invisionsf_client_callback {
    void *context;

    void (*updateLayerData)(void *context, const layer_data_t *data);

    void (*updateLayerBuffer)(void *context, int layerID, const AHardwareBuffer *hardwareBuffer);

    void (*updateBTControlConnectStatus)(void *context, bool bConnect);
} invisionsf_client_callback_t;

typedef struct invisionsf_client {
    int api_version;
    invisionsf_client_ops *ops;
} invisionsf_client_t;

typedef struct {
    void *libHandle;
    invisionsf_client_t *client;

} invisionsf_client_helper_t;

static inline invisionsf_client_helper_t *InVisionSFClient_Create(invisionsf_client_callback_t *callback) {
    invisionsf_client_helper_t *me = (invisionsf_client_helper_t *)
            malloc(sizeof(invisionsf_client_helper_t));
    if (!me) return NULL;

    me->libHandle = dlopen(INVISIONSF_CLIENT_LIB, RTLD_NOW);
    if (!me->libHandle) {
        __android_log_print(ANDROID_LOG_ERROR, "INVISION_SF_CLIENT", "dlopen libinvisionsfclient.so failed %s", dlerror());
        free(me);
        return NULL;
    }

    typedef invisionsf_client_t *(*invisionsf_client_wrapper_fn)(invisionsf_client_callback_t *);
    invisionsf_client_wrapper_fn invisionsfClientCreate;

    invisionsfClientCreate = (invisionsf_client_wrapper_fn) dlsym(me->libHandle,
                                                                  "createInVisionSFClient");

    if (!invisionsfClientCreate) {
        __android_log_print(ANDROID_LOG_ERROR, "INVISION_SF_CLIENT", "method createInVisionSFClient not found in libinvisionsfclient.so");
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }
    me->client = invisionsfClientCreate(callback);
    return me;
}

static inline void InVisionSFClient_Destroy(invisionsf_client_helper_t *me) {
    if (!me) return;

    typedef void(*invisionsf_client_wrapper_fn)(invisionsf_client_t *);
    invisionsf_client_wrapper_fn invisionsfClientDestroy;

    invisionsfClientDestroy = (invisionsf_client_wrapper_fn) dlsym(me->libHandle,
                                                                   "destoryInVisionSFClient");

    if (!invisionsfClientDestroy) {
        invisionsfClientDestroy(me->client);
    }

    if (me->libHandle) {
        dlclose(me->libHandle);
    }
    free(me);
    me = NULL;
}

static inline void InVisionSFClient_UpdateViewMatrix(invisionsf_client_helper_t *me, float *m) {
    if (!me) return;
    me->client->ops->updateViewMatrix(m);
}

static inline void InVisionSFClient_UpdateModelMatrix(invisionsf_client_helper_t *me, int layerID, float *m) {
    if (!me) return;
    me->client->ops->updateModelMatrix(layerID, m);
}

static inline void InVisionSFClient_sendActionBarCmd(invisionsf_client_helper_t *me, int layerID, int cmd) {
    if (!me) return;
    me->client->ops->sendActionBarCmd(layerID, cmd);
}

static inline void InVisionSFClient_InjectKeyEvent(invisionsf_client_helper_t *me, int layerID, int displayID,int keycode,int action,long ts) {
    if (!me) return;
    if(me->client->ops->injectIvKeyEvent){
        me->client->ops->injectIvKeyEvent(layerID, displayID,keycode,action,ts);
    }
}

static inline void InVisionSFClient_InjectMotionEvent(invisionsf_client_helper_t *me, int layerID, int displayID,int action,long ts,float x,float y) {
    if (!me) return;
    if(me->client->ops->injectIvMotionEvent){
        me->client->ops->injectIvMotionEvent(layerID,displayID,action,ts,x,y);
    }
}

#ifdef __cplusplus
}
#endif

#endif
