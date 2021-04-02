#ifndef SC_HANDSHANKCLIENT_H
#define SC_HANDSHANKCLIENT_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/log.h>
typedef struct {
    float acc[3];
    float gyro[3];
    float quaternion[4];
    uint64_t time;
} HandShank_IMUDATA;


typedef struct handshank_client_ops {
    void (*HandShank_start)(void **);

    void (*HandShank_stop)(void **);

    void (*HandShank_getimu)(void **, std::vector<HandShank_IMUDATA> &v, int lr);

    void (*HandShank_deinitcontext)(void **);

    void (*HandShank_registerChargingEventCallback)(void **, std::function<void(bool, uint8_t)> c);

    void (*HandShank_registerBatteryEventCallback)(void **, std::function<void(int, uint8_t)> c);

    void (*HandShank_registerConnectEventCallback)(void **, std::function<void(bool, uint8_t)> c);

    void (*HandShank_registerTouchEventCallback)(void **, std::function<void(int, int, uint8_t)> c);

    void (*HandShank_registerHallEventCallback)(void **, std::function<void(int, int, uint8_t)> c);

    void (*HandShank_registerKeyEventCallback)(void **, std::function<void(int, int, uint8_t)> c);

    void (*HandShank_registerKeyTouchEventCallback)(void **, std::function<void(bool, bool, bool, bool, uint8_t)> c);

    uint8_t (*HandShank_getVersion)(void **);

    uint8_t (*HandShank_getBond)(void **);

    uint8_t (*HandShank_getBattery)(void **, uint8_t lr);

    bool (*HandShank_getConnectState)(void **, uint8_t lr);

    void (*HandShank_clearBond)(void **);

    void (*HandShank_ledControl)(void **, uint8_t enable);

    void (*HandShank_vibrateControl)(void **, uint8_t value);

    int (*HandShank_getPose)(void **, float *, double &ts, int lr);
} handshank_client_ops_t;

#define HANDSHANK_CLIENT_LIB "libhandshank_client.so"

typedef struct {
    void *libHandle;
    void *context;
    handshank_client_ops_t *ops;
    int api_version;
} handshank_client_helper_t;

static inline handshank_client_helper_t *HandShankClient_Create() {
    handshank_client_helper_t *me = (handshank_client_helper_t *)
            malloc(sizeof(handshank_client_helper_t));
    if (!me) return NULL;

    me->libHandle = dlopen(HANDSHANK_CLIENT_LIB, RTLD_NOW);
    if (!me->libHandle) {
        free(me);
        return NULL;
    }

    me->ops = (handshank_client_ops_t *) malloc(sizeof(handshank_client_ops_t));
    if (!me->ops) {
        __android_log_print(ANDROID_LOG_ERROR, "HandShankClient", "HandShankClient dlopen error=%s", dlerror());
        dlclose(me->libHandle);
        free(me);
        return NULL;
    }
    me->api_version = 1;

    typedef void (*HandShank_initContext)(void **);
    HandShank_initContext initContext = (HandShank_initContext) dlsym(me->libHandle, "HandShank_InitContext");
    if (!initContext) {
        __android_log_print(ANDROID_LOG_ERROR, "HandShankClient", "HandShankClient initContext error=%s", dlerror());
    }
    initContext(&me->context);

    typedef void (*HandShank_start_fn)(void **);
    me->ops->HandShank_start = (HandShank_start_fn) dlsym(me->libHandle, "HandShank_Start");

    typedef void (*HandShank_stop_fn)(void **);
    me->ops->HandShank_stop = (HandShank_stop_fn) dlsym(me->libHandle, "HandShank_Stop");

    typedef void (*HandShank_getimu_fn)(void **, std::vector<HandShank_IMUDATA> &v, int lr);
    me->ops->HandShank_getimu = (HandShank_getimu_fn) dlsym(me->libHandle, "HandShank_GetImu");

    typedef void (*HandShank_deinit_context_fn)(void **);
    me->ops->HandShank_deinitcontext = (HandShank_deinit_context_fn) dlsym(me->libHandle, "HandShank_DeInitContext");

    typedef void (*HandShank_registerTouchEventCallback_fn)(void **, std::function<void(int, int, uint8_t)> c);
    me->ops->HandShank_registerTouchEventCallback = (HandShank_registerTouchEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterTouchEventCallback");

    typedef void (*HandShank_registerHallEventCallback_fn)(void **, std::function<void(int, int, uint8_t)> c);
    me->ops->HandShank_registerHallEventCallback = (HandShank_registerHallEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterHallEventCallback");

    typedef void (*HandShank_registerKeyEventCallback_fn)(void **, std::function<void(int, int, uint8_t)> c);
    me->ops->HandShank_registerKeyEventCallback = (HandShank_registerKeyEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterKeyEventCallback");

    typedef void (*HandShank_registerKeyTouchEventCallback_fn)(void **, std::function<void(bool, bool, bool, bool, uint8_t)> c);
    me->ops->HandShank_registerKeyTouchEventCallback = (HandShank_registerKeyTouchEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterKeyTouchEventCallback");

    typedef void (*HandShank_registerChargingEventCallback_fn)(void **, std::function<void(bool, uint8_t)> c);
    me->ops->HandShank_registerChargingEventCallback = (HandShank_registerChargingEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterChargingEventCallback");

    typedef void (*HandShank_registerBatteryEventCallback_fn)(void **, std::function<void(int, uint8_t)> c);
    me->ops->HandShank_registerBatteryEventCallback = (HandShank_registerBatteryEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterBatteryEventCallback");

    typedef void (*HandShank_registerConnectEventCallback_fn)(void **, std::function<void(bool, uint8_t)> c);
    me->ops->HandShank_registerConnectEventCallback = (HandShank_registerConnectEventCallback_fn) dlsym(me->libHandle, "HandShank_RegisterConnectEventCallback");

    typedef uint8_t (*HandShank_getVersion_fn)(void **);
    me->ops->HandShank_getVersion = (HandShank_getVersion_fn) dlsym(me->libHandle, "HandShank_GetVersion");

    typedef uint8_t (*HandShank_getBond_fn)(void **);
    me->ops->HandShank_getBond = (HandShank_getBond_fn) dlsym(me->libHandle, "HandShank_Getbond");

    typedef uint8_t (*HandShank_getBattery_fn)(void **, uint8_t lr);
    me->ops->HandShank_getBattery = (HandShank_getBattery_fn) dlsym(me->libHandle, "HandShank_GetBattery");

    typedef bool (*HandShank_getConnectState_fn)(void **, uint8_t lr);
    me->ops->HandShank_getConnectState = (HandShank_getConnectState_fn) dlsym(me->libHandle, "HandShank_GetConnectState");

    typedef void (*HandShank_clearBond_fn)(void **);
    me->ops->HandShank_clearBond = (HandShank_clearBond_fn) dlsym(me->libHandle, "HandShank_Clearbond");

    typedef void (*HandShank_ledControl_fn)(void **, uint8_t enable);
    me->ops->HandShank_ledControl = (HandShank_ledControl_fn) dlsym(me->libHandle, "HandShank_LedControl");

    typedef void (*HandShank_vibrateControl_fn)(void **, uint8_t value);
    me->ops->HandShank_vibrateControl = (HandShank_vibrateControl_fn) dlsym(me->libHandle, "HandShank_VibrateControl");

    typedef int (*HandShank_getPose_fn)(void **, float *, double &ts, int lr);
    me->ops->HandShank_getPose = (HandShank_getPose_fn) dlsym(me->libHandle, "HandShank_GetPose");
    return me;
}

static inline void HandShankClient_Destroy(handshank_client_helper_t *me) {
    __android_log_print(ANDROID_LOG_ERROR, "HandShankClient", "HandShankClient_Destroy");
    if (!me) return;
    if (me->ops->HandShank_deinitcontext) {
        me->ops->HandShank_deinitcontext(&me->context);
    }

    if (me->libHandle) {
        dlclose(me->libHandle);
    }
    me->context = nullptr;
    free(me);
    me = nullptr;
}

static inline void HandShankClient_Start(handshank_client_helper_t *me) {
    if (!me) return;
    if (me->ops->HandShank_start) {
        me->ops->HandShank_start(&me->context);
    }
}

static inline void HandShankClient_Stop(handshank_client_helper_t *me) {
    if (!me) return;
    if (me->ops->HandShank_stop) {
        me->ops->HandShank_stop(&me->context);
    }
}

static inline void HandShankClient_GetImu(handshank_client_helper_t *me, std::vector<HandShank_IMUDATA> &v, int lr) {
    if (!me) return;
    if (me->ops->HandShank_getimu) {
        me->ops->HandShank_getimu(&me->context, v, lr);
    }
}

static inline void HandShankClient_RegisterTouchEventCB(handshank_client_helper_t *me, std::function<void(int, int, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerTouchEventCallback) {
        me->ops->HandShank_registerTouchEventCallback(&me->context, c);
    }
}

static inline void HandShankClient_RegisterHallEventCB(handshank_client_helper_t *me, std::function<void(int, int, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerHallEventCallback) {
        me->ops->HandShank_registerHallEventCallback(&me->context, c);
    }
}

static inline void HandShankClient_RegisterKeyEventCB(handshank_client_helper_t *me, std::function<void(int, int, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerKeyEventCallback) {
        me->ops->HandShank_registerKeyEventCallback(&me->context, c);
    }
}

static inline void HandShankClient_RegisterKeyTouchEventCB(handshank_client_helper_t *me, std::function<void(bool, bool, bool, bool, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerKeyTouchEventCallback) {
        me->ops->HandShank_registerKeyTouchEventCallback(&me->context, c);
    }
}

static inline void HandShankClient_RegisterChargingEventCallback(handshank_client_helper_t *me, std::function<void(bool, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerChargingEventCallback) {
        me->ops->HandShank_registerChargingEventCallback(&me->context, c);
    }
}

static inline void HandShankClient_RegisterBatteryEventCallback(handshank_client_helper_t *me, std::function<void(int, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerBatteryEventCallback) {
        me->ops->HandShank_registerBatteryEventCallback(&me->context, c);
    }
}

static inline void HandShankClient_RegisterConnectEventCallback(handshank_client_helper_t *me, std::function<void(bool, uint8_t)> c) {
    if (!me) return;
    if (me->ops->HandShank_registerConnectEventCallback) {
        me->ops->HandShank_registerConnectEventCallback(&me->context, c);
    }
}

static inline uint8_t HandShankClient_GetBattery(handshank_client_helper_t *me, uint8_t lr) {
    if (!me) return -2;
    if (me->ops->HandShank_getBattery) {
        return me->ops->HandShank_getBattery(&me->context, lr);
    }

    return -2;
}

static inline bool HandShankClient_GetConnectState(handshank_client_helper_t *me, uint8_t lr) {
    if (!me) return false;
    if (me->ops->HandShank_getConnectState) {
        return me->ops->HandShank_getConnectState(&me->context, lr);
    }

    return false;
}

static inline uint8_t HandShankClient_GetVersion(handshank_client_helper_t *me) {
    if (!me) return -2;
    if (me->ops->HandShank_getVersion) {
        return me->ops->HandShank_getVersion(&me->context);
    }

    return -2;
}

static inline uint8_t HandShankClient_Getbond(handshank_client_helper_t *me) {
    if (!me) return -2;
    if (me->ops->HandShank_getBond) {
        return me->ops->HandShank_getBond(&me->context);
    }

    return -2;
}

static inline void HandShankClient_Clearbond(handshank_client_helper_t *me) {
    if (!me) return;
    if (me->ops->HandShank_clearBond) {
        me->ops->HandShank_clearBond(&me->context);
    }
}

static inline void HandShankClient_LedControl(handshank_client_helper_t *me, uint8_t enable) {
    if (!me) return;
    if (me->ops->HandShank_ledControl) {
        me->ops->HandShank_ledControl(&me->context, enable);
    }
}

static inline void HandShankClient_VibrateControl(handshank_client_helper_t *me, uint8_t value) {
    if (!me) return;
    if (me->ops->HandShank_vibrateControl) {
        me->ops->HandShank_vibrateControl(&me->context, value);
    }
}

static inline int HandShankClient_GetPose(handshank_client_helper_t *me, float *data, double &ts, int lr) {
    if (!me) return -2;
    if (me->ops->HandShank_getPose) {
        return me->ops->HandShank_getPose(&me->context, data, ts, lr);
    }
    return -2;
}

#ifdef __cplusplus
}
#endif
#endif //SC_HANDSHANKCLIENT_H
