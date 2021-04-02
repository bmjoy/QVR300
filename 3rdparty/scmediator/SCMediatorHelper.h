#pragma once

// #ifdef __cplusplus
// extern "C" {
// #endif



#ifdef __cplusplus
extern "C"{
#endif

#include <dlfcn.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>


typedef void *SCMediatorHandler;

typedef struct {
    SCMediatorHandler (*createMediatorFunction)();
    int (*fetchDeflectionValue)(SCMediatorHandler);
    int (*updateDeflectionValue)(SCMediatorHandler, int newValue);
} SCMediatorOps;

typedef struct {
    int version;
    SCMediatorOps *ops;
} SCMediatorOperator;

typedef struct {
    void *libHandle = nullptr;
    SCMediatorOperator *mediatorOperator = nullptr;
    SCMediatorHandler mediatorHandler;
} SCMediatorHelper;

static inline SCMediatorHelper *scCreateMediator() {
    SCMediatorHelper *helper = new SCMediatorHelper();
    helper->libHandle = dlopen("libscmediator.so", RTLD_NOW);
    if (nullptr == helper->libHandle) {
        delete helper;
        return nullptr;
    }

    typedef SCMediatorOperator *(*sc_create_mediator_operator_fn)(void);
    sc_create_mediator_operator_fn createMediatorOperatorFunction;

    createMediatorOperatorFunction =
            (sc_create_mediator_operator_fn)dlsym(helper->libHandle,
                                                      "createMediatorOperator");
    if (!createMediatorOperatorFunction) {
        dlclose(helper->libHandle);
        delete helper;
        return nullptr;
    }
    helper->mediatorOperator = createMediatorOperatorFunction();
    helper->mediatorHandler = helper->mediatorOperator->ops->createMediatorFunction();
    return helper;
}

static inline int scFetchDeflection(SCMediatorHelper *helper, int &outCurrVal) {
    outCurrVal = 0;
    if (!helper) {
        return -1;
    }
    outCurrVal = helper->mediatorOperator->ops->fetchDeflectionValue(helper->mediatorHandler);
    return 0;
}

static inline int scUpdateDeflection(SCMediatorHelper *helper, int newVal) {
    if (!helper) {
        return -1;
    }
    return helper->mediatorOperator->ops->updateDeflectionValue(helper->mediatorHandler, newVal);
}


#ifdef __cplusplus
}
#endif