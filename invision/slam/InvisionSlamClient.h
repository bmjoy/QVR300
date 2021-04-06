//
// Created by guowei on 20-4-16.
//

#ifndef SLAMDEMO_INVISIONSLAMCLIENT_H
#define SLAMDEMO_INVISIONSLAMCLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/log.h>
typedef enum {
    IV_RENDER = 0,
    IV_WARP,
    IV_CONTROLLER,
    IV_NORMAL,
} IVSLAM_THREAD_TYPE;
typedef struct ivslam_client_ops {
    void (*ivslam_start)(void **);

    void (*ivslam_stop)(void **);

    int (*ivslam_getpose)(void **, float *pose, double predictTimeDiffNano, double &pose_ts);

    void (*ivslam_deinitcontext)(void **);

    void (*ivslam_setthreadattributesbytype)(void **, int, int);

    void (*ivslam_setthreadpriority)(void **, int, int);

    void (*ivslam_resavemap)(void **, const char *path);

    int (*ivslam_getofflinemaprelocstate)(void **);

    void (*ivslam_getpanel)(void **, int &panelsize, float *panelinfo);

    void (*ivslam_setAppExitCB)(void **, APP_EXIT_CALLBACK callback);

    void (*ivslam_getgnss)(void **, double &dt, float *gnss);
} ivslam_client_ops_t;

#define INVISION_SLAM_LIB "libuvc_native.so"
typedef struct {
    void *libHandle;
    void *context;
    ivslam_client_ops_t *ops;
    int api_version;
} ivslam_client_helper_t;


static inline ivslam_client_helper_t *IvSlamClient_Create() {
    ivslam_client_helper_t *me = (ivslam_client_helper_t *)
            malloc(sizeof(ivslam_client_helper_t));
    if (!me) return NULL;

    me->libHandle = dlopen(INVISION_SLAM_LIB, RTLD_NOW);
    if (!me->libHandle) {
        free(me);
        return NULL;
    }

    me->ops = (ivslam_client_ops_t *) malloc(sizeof(ivslam_client_ops_t));
    if (!me->ops) {
        __android_log_print(ANDROID_LOG_ERROR, "InvisionSlamClient", "InvisionSlamClient dlopen error=%s", dlerror());
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }
    me->api_version = 1;

//    typedef void (*ivslam_initContext)(void **);
//    ivslam_initContext initContext =  (ivslam_initContext) dlsym(me->libHandle,"InvisionSlam_InitContext");
//    if(!initContext){
//        __android_log_print(ANDROID_LOG_ERROR, "InvisionSlamClient","InvisionSlamClient initContext error=%s", dlerror());
//    }
//    initContext(&me->context);

    typedef void (*ivslam_start_fn)(void **);
    me->ops->ivslam_start = (ivslam_start_fn) dlsym(me->libHandle, "InvisionSlam_Start");

    typedef void (*ivslam_stop_fn)(void **);
    me->ops->ivslam_stop = (ivslam_stop_fn) dlsym(me->libHandle, "InvisionSlam_Stop");

    typedef int (*ivslam_getpose_fn)(void **, float *, double, double &);
    me->ops->ivslam_getpose = (ivslam_getpose_fn) dlsym(me->libHandle,"My_GetPose");

    typedef void (*ivslam_deinit_context_fn)(void **);
    me->ops->ivslam_deinitcontext = (ivslam_deinit_context_fn) dlsym(me->libHandle, "InvisionSlam_DeInitContext");

    typedef void (*ivslam_setthreadattributesbytype_fn)(void **, int, int);
    me->ops->ivslam_setthreadattributesbytype = (ivslam_setthreadattributesbytype_fn) dlsym(me->libHandle, "InvisionSlam_SetThreadAttributesByType");
    typedef void (*ivslam_setthreadpriority_fn)(void **, int, int);
    me->ops->ivslam_setthreadpriority = (ivslam_setthreadpriority_fn) dlsym(me->libHandle, "InvisionSlam_SetThreadPriority");
    typedef void (*ivslam_resavemap_fn)(void **, const char *path);
    me->ops->ivslam_resavemap = (ivslam_resavemap_fn) dlsym(me->libHandle, "InvisionSlam_ReSaveMap");
    typedef int(*ivslam_getofflinemaprelocstate_fn)(void **);
    me->ops->ivslam_getofflinemaprelocstate = (ivslam_getofflinemaprelocstate_fn) dlsym(me->libHandle, "InvisionSlam_GetOfflineMapRelocState");
    typedef void(*ivslam_getpanel_fn)(void **, int &panelsize, float *panelinfo);
    me->ops->ivslam_getpanel = (ivslam_getpanel_fn) dlsym(me->libHandle, "InvisionSlam_GetPanel");

    typedef void(*ivslam_setAppExitCB_fn)(void **, APP_EXIT_CALLBACK callback);
    me->ops->ivslam_setAppExitCB = (ivslam_setAppExitCB_fn) dlsym(me->libHandle, "InvisionSlam_SetAppExitCallback");

    typedef void(*ivslam_getgnss_fn)(void **, double &dt, float *gnss);
    me->ops->ivslam_getgnss = (ivslam_getgnss_fn) dlsym(me->libHandle, "InvisionSlam_GetGnss");
    return me;
}

static inline void IvSlamClient_Destroy(ivslam_client_helper_t *me) {
    __android_log_print(ANDROID_LOG_ERROR, "InvisionSlamClient", "IvSlamClient_Destroy");
    if (!me) return;
    if (me->ops->ivslam_deinitcontext) {
        me->ops->ivslam_deinitcontext(&me->context);
    }

    if (me->libHandle) {
        dlclose(me->libHandle);
    }
    me->context = nullptr;
    free(me);
    me = nullptr;
}

static inline void IvSlamClient_Start(ivslam_client_helper_t *me) {
    if (!me) return;
    if (me->ops->ivslam_start) {
        me->ops->ivslam_start(&me->context);
    }
}

static inline void IvSlamClient_Stop(ivslam_client_helper_t *me) {
    if (!me) return;
    if (me->ops->ivslam_stop) {
        me->ops->ivslam_stop(&me->context);
    }
}

static inline void IvSlamClient_GetPose(ivslam_client_helper_t *me, float *pose, double predict_timenano, double &pose_ts) {
    if (!me) return;
    if (me->ops->ivslam_getpose) {
        me->ops->ivslam_getpose(&me->context, pose, predict_timenano, pose_ts);
    }
}

static inline void IvSlamClient_SetThreadAttributesByType(ivslam_client_helper_t *me, int tid, int type) {
    if (!me) return;
    if (me->ops->ivslam_setthreadattributesbytype) {
        me->ops->ivslam_setthreadattributesbytype(&me->context, tid, type);
    }
}
static inline void IvSlamClient_SetThreadPriority(ivslam_client_helper_t *me, int tid, IVSLAM_THREAD_TYPE priority) {
    if (!me) return;
    if (me->ops->ivslam_setthreadpriority) {
        me->ops->ivslam_setthreadpriority(&me->context, tid, priority);
    }
}
static inline void IvSlamClient_ReSaveMapMap(ivslam_client_helper_t *me, const char *path) {
    if (!me) return;
    if (me->ops->ivslam_resavemap) {
        me->ops->ivslam_resavemap(&me->context, path);
    }
}
static inline int IvSlamClient_GetOfflineMapRelocState(ivslam_client_helper_t *me) {
    if (!me) return 1;
    if (me->ops->ivslam_getofflinemaprelocstate) {
        return me->ops->ivslam_getofflinemaprelocstate(&me->context);
    }
    return 1;
}
static inline void IvSlamClient_GetPanel(ivslam_client_helper_t *me, int &panelsize, float *panelinfo) {
    if (!me) return;
    if (me->ops->ivslam_getpanel) {
        me->ops->ivslam_getpanel(&me->context, panelsize, panelinfo);
    }
    return;
}

static inline void IvSlamClient_SetAppExitCallback(ivslam_client_helper_t *me, APP_EXIT_CALLBACK callback) {
    if (!me) return;
    if (me->ops->ivslam_setAppExitCB) {
        me->ops->ivslam_setAppExitCB(&me->context, callback);
    }
}

static inline void IvSlamClient_GetGnss(ivslam_client_helper_t *me, double &dt, float *gnss) {
    if (!me) return;
    if (me->ops->ivslam_getgnss) {
        me->ops->ivslam_getgnss(&me->context, dt, gnss);
    }
    return;
}

#ifdef __cplusplus
}
#endif
#endif //SLAMDEMO_INVISIONSLAMCLIENT_H
