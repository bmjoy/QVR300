//
// Created by zengchuiguo on 7/22/2020.
//

#ifndef INVISIONGESTURECLIENT_H
#define INVISIONGESTURECLIENT_H

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/log.h>
#include "svrUtil.h"

typedef struct ivgesture_client_ops {
    void (*ivgesture_start)(void **,LOW_POWER_WARNING_CALLBACK callback);
    void (*ivgesture_stop)(void **);
    int (*ivgesture_getgesture)(void**,uint64_t *index,float *model, float *pose);
    int (*ivgesture_setgestureData)(void**,float *model, float *pose);
    void (*ivgesture_deinitcontext)(void **);
    void (*ivgesture_setGestureChangeCb)(void **,GESTURE_CHANGE_CALLBACK callback);
    void (*ivgesture_setGestureModelDataChangeCb)(void **,GESTURE_MODEL_DATA_CHANGE_CALLBACK callback);
    void (*ivgesture_setLowPowerWarningCb)(void **,LOW_POWER_WARNING_CALLBACK callback);
}ivgesture_client_ops_t;

#define INVISION_GESTURE_LIB "libivgesture_client.so"
typedef struct {
    void *libHandle;
    void *context;
    ivgesture_client_ops_t *ops;
    int api_version;
} ivgesture_client_helper_t;


static inline ivgesture_client_helper_t *IvGestureClient_Create() {
    ivgesture_client_helper_t *me = (ivgesture_client_helper_t *)
            malloc(sizeof(ivgesture_client_helper_t));
    if (!me) {
        LOGE("%s, malloc for the gesture helper fail!",__FUNCTION__);
        return nullptr;
    }

    me->libHandle = dlopen(INVISION_GESTURE_LIB, RTLD_NOW);
    if (!me->libHandle) {
        LOGE("%s, open the gesture lib fail!",__FUNCTION__);
        free(me);
        return nullptr;
    }

    me->ops = (ivgesture_client_ops_t *) malloc(sizeof(ivgesture_client_ops_t));
    if(!me->ops){
        LOGE("%s, malloc for ops fail!",__FUNCTION__);
        dlclose(me->libHandle);
        free(me);
        return nullptr;
    }
    me->api_version=1;

    typedef void (*ivgesture_initContext)(void **);
    ivgesture_initContext initContext =  (ivgesture_initContext) dlsym(me->libHandle,"InvisionGesture_InitContext");
    if(!initContext){
        LOGE("%s, dlopen error=%s",__FUNCTION__,dlerror() );
        dlclose(me->libHandle);
        free(me);
        return nullptr;
    }
    initContext(&me->context);

    typedef void (*ivgesture_start_fn)(void**, LOW_POWER_WARNING_CALLBACK);
    me->ops->ivgesture_start = (ivgesture_start_fn) dlsym(me->libHandle,"InvisionGesture_Start");

    typedef void (*ivgesture_stop_fn)(void**);
    me->ops->ivgesture_stop = (ivgesture_stop_fn) dlsym(me->libHandle,"InvisionGesture_Stop");

    typedef int (*ivgesture_getgesture_fn)(void**,uint64_t *,float*,float*);
    me->ops->ivgesture_getgesture = (ivgesture_getgesture_fn) dlsym(me->libHandle,"InvisionGesture_GetGesture");

    typedef int (*ivgesture_setgestureData_fn)(void**,float*,float*);
    me->ops->ivgesture_setgestureData = (ivgesture_setgestureData_fn) dlsym(me->libHandle,"InvisionGesture_SetGestureData");

    typedef void (*ivgesture_deinit_context_fn)(void**);
    me->ops->ivgesture_deinitcontext = (ivgesture_deinit_context_fn) dlsym(me->libHandle,"InvisionGesture_DeInitContext");

    typedef void (*ivgesture_setGestureChangeCb_fn)(void**,GESTURE_CHANGE_CALLBACK);
    me->ops->ivgesture_setGestureChangeCb = (ivgesture_setGestureChangeCb_fn) dlsym(me->libHandle,"InvisionGesture_SetGestureChangeCallback");

    typedef void (*ivgesture_setGestureModelDataChangeCb_fn)(void**,GESTURE_MODEL_DATA_CHANGE_CALLBACK);
    me->ops->ivgesture_setGestureModelDataChangeCb = (ivgesture_setGestureModelDataChangeCb_fn) dlsym(me->libHandle,"InvisionGesture_SetGestureModelDataChangeCallback");

    typedef void (*ivgesture_setLowPowerWarningCb_fn)(void**,LOW_POWER_WARNING_CALLBACK);
    me->ops->ivgesture_setLowPowerWarningCb = (ivgesture_setLowPowerWarningCb_fn) dlsym(me->libHandle,"InvisionGesture_SetLowPowerWarningCallback");

    return me;
}

static inline void IvGestureClient_Destroy(ivgesture_client_helper_t *me) {
    if (!me) return;
    LOGE("%s",__FUNCTION__ );
    if(me->ops->ivgesture_deinitcontext){
        me->ops->ivgesture_deinitcontext(&me->context);
    }

    if (me->libHandle) {
        dlclose(me->libHandle);
    }
    me->context = nullptr;
    free(me);
    me = nullptr;
}

static inline void IvGestureClient_Start(ivgesture_client_helper_t *me,LOW_POWER_WARNING_CALLBACK callback){
    if(!me) return;
    LOGE("%s",__FUNCTION__ );
    if(me->ops->ivgesture_start){
        me->ops->ivgesture_start(&me->context,callback);
    }
}

static inline void IvGestureClient_Stop(ivgesture_client_helper_t *me){
    if(!me) {
        LOGE("%s, the helper was invalid",__FUNCTION__ );
        return;
    }
    LOGE("%s",__FUNCTION__ );
    if(me->ops->ivgesture_stop){
        me->ops->ivgesture_stop(&me->context);
    }
}

static inline void IvGestureClient_GetGesture(ivgesture_client_helper_t *me,uint64_t *index,float * model,float * pose){
    if(!me) return;
    if(me->ops->ivgesture_getgesture){
        me->ops->ivgesture_getgesture(&me->context,index,model,pose);
    }
}

static inline void IvGestureClient_SetGestureData(ivgesture_client_helper_t *me,float * model,float * pose){
    if(!me) return;
    if(me->ops->ivgesture_setgestureData){
        me->ops->ivgesture_setgestureData(&me->context,model,pose);
    }
}


static inline void IvGestureClient_SetGestureChangeCallback(ivgesture_client_helper_t *me,GESTURE_CHANGE_CALLBACK callback){
    if(!me) return;
    if(me->ops->ivgesture_setGestureChangeCb){
        me->ops->ivgesture_setGestureChangeCb(&me->context,callback);
    }
}

static inline void IvGestureClient_SetGestureModelDataChangeCallback(ivgesture_client_helper_t *me,GESTURE_MODEL_DATA_CHANGE_CALLBACK callback){
    if(!me) return;
    if(me->ops->ivgesture_setGestureModelDataChangeCb){
        me->ops->ivgesture_setGestureModelDataChangeCb(&me->context,callback);
    }
}

static inline void IvGestureClient_SetLowPowerWarningCallback(ivgesture_client_helper_t *me,LOW_POWER_WARNING_CALLBACK callback){
    if(!me) return;
    if(me->ops->ivgesture_setLowPowerWarningCb){
        me->ops->ivgesture_setLowPowerWarningCb(&me->context,callback);
    }
}

#ifdef __cplusplus
}
#endif

#endif //INVISIONGESTURECLIENT_H
