//
// Created by guowei on 2020/7/21.
//

#ifndef INVISION_CAMERA_HELPER_H
#define INVISION_CAMERA_HELPER_H

#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/log.h>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct ivcamera_helper_ops {
    void (*ivcamera_start)(int64_t offset,std::function<void(void* data, uint64_t ts,u_short gain,u_short exp)> c);
    void (*ivcamera_stop)();
    void (*ivcamera_set_camera_ts_offset)(int type,int64_t);
    void (*ivcamera_set_camera_exp_gain)(int type,int idx,u_short gain,u_short exp);
    void (*ivcamera_set_camera_mode)(int mode);
}ivcamera_helper_ops_t;

#define IVCAMERA_HELPER_LIB "libivcameramanager.so"
typedef struct {
    void *libHandle;
    void *context;
    ivcamera_helper_ops_t *ops;
    int api_version;
} ivcamera_helper_t;


static inline ivcamera_helper_t *IvCameraHelper_Create() {
    ivcamera_helper_t *me = (ivcamera_helper_t *)
            malloc(sizeof(ivcamera_helper_t));
    if (!me) return NULL;

    me->libHandle = dlopen(IVCAMERA_HELPER_LIB, RTLD_NOW);
    if (!me->libHandle) {
        __android_log_print(ANDROID_LOG_ERROR, "IVCameraHelper","IVCameraHelper dlopen error=%s", dlerror());
        free(me);
        return NULL;
    }

    me->ops = (ivcamera_helper_ops_t *) malloc(sizeof(ivcamera_helper_ops_t));
    if(!me->ops){
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }
    me->api_version=1;

    typedef void (*ivcamera_start_fn)(int64_t offset,std::function<void(void* data, uint64_t ts,u_short gain,u_short exp)> c);
    me->ops->ivcamera_start = (ivcamera_start_fn) dlsym(me->libHandle,"IvCamera_Start");

    typedef void (*ivcamera_stop_fn)();
    me->ops->ivcamera_stop = (ivcamera_stop_fn) dlsym(me->libHandle,"IvCamera_Stop");


    typedef void (*ivcamera_set_camera_ts_offset_fn)(int type,int64_t);
    me->ops->ivcamera_set_camera_ts_offset = (ivcamera_set_camera_ts_offset_fn) dlsym(me->libHandle,"IvCamera_SetCameraTsOffset");

    typedef void (*ivcamera_set_camera_exp_gain_fn)(int type,int idx,u_short gain,u_short exp);
    me->ops->ivcamera_set_camera_exp_gain = (ivcamera_set_camera_exp_gain_fn) dlsym(me->libHandle,"IvCamera_SetCamereExpGain");


    typedef void (*ivcamera_set_camera_mode_fn)(int mode);
    me->ops->ivcamera_set_camera_mode = (ivcamera_set_camera_mode_fn) dlsym(me->libHandle,"IvCamera_SetCamereMode");
    return me;
}

static inline void IvCameraHelper_Destroy(ivcamera_helper_t *me) {
    __android_log_print(ANDROID_LOG_ERROR, "IVCameraHelper","ivcameraClient_Destroy");
    if (!me) return;

    if (me->libHandle) {
        dlclose(me->libHandle);
    }
    me->context = NULL;
    free(me);
    me = NULL;
}

static inline void IvCameraHelper_Start(ivcamera_helper_t *me, int64_t offset,std::function<void(void* data, uint64_t ts,u_short gain,u_short exp)> c) {
    __android_log_print(ANDROID_LOG_ERROR, "IVCameraHelper","IvCameraHelper_Start");
    if (!me) return;
    if(me->ops->ivcamera_start){
        me->ops->ivcamera_start(offset,c);
    }
}


static inline void IvCameraHelper_Stop(ivcamera_helper_t *me) {
    __android_log_print(ANDROID_LOG_ERROR, "IVCameraHelper","IvCameraHelper_Stop");
    if (!me) return;
    if(me->ops->ivcamera_stop){
        me->ops->ivcamera_stop();
    }
}

static inline void IvCameraHelper_SetCamereExpGain(ivcamera_helper_t *me,int type,int idx,u_short gain,u_short exp) {
    __android_log_print(ANDROID_LOG_ERROR, "IVCameraHelper","IvCameraHelper_SetCamereExpGain");
    if (!me) return;
    if(me->ops->ivcamera_set_camera_exp_gain){
        me->ops->ivcamera_set_camera_exp_gain(type,idx,gain,exp);
    }
}

//0 for handshake the led will off, 1 for gesture the led will on
static inline void IvCameraHelper_SetCamereMode(ivcamera_helper_t *me,int mode) {
    __android_log_print(ANDROID_LOG_ERROR, "IVCameraHelper","IvCameraHelper_SetCamereMode %d",mode);
    if (!me) return;
    if(me->ops->ivcamera_set_camera_mode){
        me->ops->ivcamera_set_camera_mode(mode);
    }
}


#ifdef __cplusplus
}
#endif
#endif //QCOM_835_INVISIONCAMERAHELPER_H
