//
// Created by guowei on 20-4-16.
//

#ifndef SLAMDEMO_A11GDEVICECLIENT_H
#define SLAMDEMO_A11GDEVICECLIENT_H
#include <vector>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/log.h>

typedef enum a11g_device_type_t{
    TYPE_OTHERS,
    TYPE_HAND_SHAKE,
    TYPE_HAND_GESTURE
}a11g_device_type;

typedef struct{
    float acc[3];
    float gyro[3];
    float mag[3];
    uint64_t acc_dt;
    uint64_t gyro_dt;
    uint64_t mag_dt;
}A11GDevice_IMUDATA;

typedef struct{
    int x;
    int y;
    uint64_t dt;
}A11GDevice_TPDATA;

typedef struct a11gdevice_client_ops {
    void (*a11gdevice_start)(void **, a11g_device_type device_type);
    void (*a11gdevice_stop)(void **, a11g_device_type device_type);
    void (*a11gdevice_getimu)(void**,std::vector<A11GDevice_IMUDATA> &v);
    int64_t (*a11gdevice_getlastvsync)(void**);
    void (*a11gdevice_deinitcontext)(void **);
    void (*a11gdevice_registercameracb)(void **,std::function<void(void* data, uint64_t ts,u_short gain,u_short exp)> c);
    void (*a11gdevice_registertpcb)(void **,std::function<void(std::vector<A11GDevice_TPDATA> &)> c);
}a11gdevice_client_ops_t;

#define A11GDEVICE_CLIENT_LIB "liba11gdevice_client.so"
typedef struct {
    void *libHandle;
    void *context;
    a11gdevice_client_ops_t *ops;
    int api_version;
} a11gdevice_client_helper_t;


static inline a11gdevice_client_helper_t *A11GDeviceClient_Create() {
    a11gdevice_client_helper_t *me = (a11gdevice_client_helper_t *)
            malloc(sizeof(a11gdevice_client_helper_t));
    if (!me) return NULL;

    me->libHandle = dlopen(A11GDEVICE_CLIENT_LIB, RTLD_NOW);
    if (!me->libHandle) {
        free(me);
        return NULL;
    }

    me->ops = (a11gdevice_client_ops_t *) malloc(sizeof(a11gdevice_client_ops_t));
    if(!me->ops){
        __android_log_print(ANDROID_LOG_ERROR, "A11GDeviceClient","A11GDeviceClient dlopen error=%s", dlerror());
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }
    me->api_version=1;

    typedef void (*a11gdevice_initContext)(void **);
    a11gdevice_initContext initContext =  (a11gdevice_initContext) dlsym(me->libHandle,"A11GDevice_InitContext");
    if(!initContext){
        __android_log_print(ANDROID_LOG_ERROR, "A11GDeviceClient","A11GDeviceClient initContext error=%s", dlerror());
    }
    initContext(&me->context);

    typedef void (*a11gdevice_start_fn)(void**,a11g_device_type device_type);
    me->ops->a11gdevice_start = (a11gdevice_start_fn) dlsym(me->libHandle,"A11GDevice_Start");

    typedef void (*a11gdevice_stop_fn)(void**,a11g_device_type device_type);
    me->ops->a11gdevice_stop = (a11gdevice_stop_fn) dlsym(me->libHandle,"A11GDevice_Stop");

    typedef void (*a11gdevice_getimu_fn)(void**,std::vector<A11GDevice_IMUDATA> &v);
    me->ops->a11gdevice_getimu = (a11gdevice_getimu_fn) dlsym(me->libHandle,"A11GDevice_GetImu");

    typedef int64_t (*a11gdevice_getlastvsync_fn)(void**);
    me->ops->a11gdevice_getlastvsync = (a11gdevice_getlastvsync_fn)dlsym(me->libHandle,"A11GDevice_GetLastVsync");

    typedef void (*a11gdevice_deinit_context_fn)(void**);
    me->ops->a11gdevice_deinitcontext = (a11gdevice_deinit_context_fn) dlsym(me->libHandle,"A11GDevice_DeInitContext");


    typedef void (*a11gdevice_registercameracb_fn)(void **,std::function<void(void* data, uint64_t ts,u_short gain,u_short exp)> c);
    me->ops->a11gdevice_registercameracb = (a11gdevice_registercameracb_fn) dlsym(me->libHandle,"A11GDevice_RegisterCameraCB");

    typedef void (*a11gdevice_registertpcb_fn)(void **,std::function<void(std::vector<A11GDevice_TPDATA> &)> c);
    me->ops->a11gdevice_registertpcb = (a11gdevice_registertpcb_fn) dlsym(me->libHandle,"A11GDevice_RegisterTpCB");

    return me;
}

static inline void A11GDeviceClient_Destroy(a11gdevice_client_helper_t *me) {
    __android_log_print(ANDROID_LOG_ERROR, "A11GDeviceClient","A11GDeviceClient_Destroy");
    if (!me) return;
    if(me->ops->a11gdevice_deinitcontext){
        __android_log_print(ANDROID_LOG_ERROR, "A11GDeviceClient","A11GDeviceClient_DeInit");
        me->ops->a11gdevice_deinitcontext(&me->context);
    }

    if (me->libHandle) {
        dlclose(me->libHandle);
    }
    me->context = nullptr;
    free(me);
    me = nullptr;
}

static inline void A11GDeviceClient_Start(a11gdevice_client_helper_t *me,a11g_device_type device_type){
    if(!me) return;
    if(me->ops->a11gdevice_start){
        me->ops->a11gdevice_start(&me->context,device_type);
    }
}

static inline void A11GDeviceClient_Stop(a11gdevice_client_helper_t *me, a11g_device_type device_type){
    if(!me) return;
    if(me->ops->a11gdevice_stop){
        me->ops->a11gdevice_stop(&me->context, device_type);
    }
}

static inline void A11GDeviceClient_GetImu(a11gdevice_client_helper_t *me,std::vector<A11GDevice_IMUDATA> &v){
    if(!me) return;
    if(me->ops->a11gdevice_getimu){
        me->ops->a11gdevice_getimu(&me->context,v);
    }
}

static inline void A11GDeviceClient_RegisterCameraCB(a11gdevice_client_helper_t *me,std::function<void(void* data, uint64_t ts,u_short gain,u_short exp)> c){
    if(!me) return;
    if(me->ops->a11gdevice_registercameracb){
        me->ops->a11gdevice_registercameracb(&me->context,c);
    }
}

static inline void A11GDeviceClient_RegisterTpCB(a11gdevice_client_helper_t *me,std::function<void(std::vector<A11GDevice_TPDATA> &)> c){
    if(!me) return;
    if(me->ops->a11gdevice_registertpcb){
        me->ops->a11gdevice_registertpcb(&me->context,c);
    }
}

static inline int64_t A11GDeviceClient_GetLastVsync(a11gdevice_client_helper_t *me){
    if(!me) return -1;
    if(me->ops->a11gdevice_getlastvsync){
        return me->ops->a11gdevice_getlastvsync(&me->context);
    }
    return -1;
}

#ifdef __cplusplus
}
#endif
#endif //SLAMDEMO_A11GDEVICECLIENT_H
