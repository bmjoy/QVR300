//=============================================================================
// FILE: svrApiVersion.cpp
//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#include <stdlib.h>
#include <stdio.h>

#include "svrApiCore.h"
#include "Invision/inc/InvisionVersion.h"

//-----------------------------------------------------------------------------
const char* svrGetVersion()
//-----------------------------------------------------------------------------
{
    static char versionBuffer[256];
    snprintf(versionBuffer, 256, "willie ar_vr %d.%d.%d, %s - %s", SVR_MAJOR_VERSION,
             SVR_MINOR_VERSION,
             SVR_REVISION_VERSION, __DATE__, __TIME__);
    return &versionBuffer[0];
}

const char* getInvisionVersion()
//-----------------------------------------------------------------------------
{
    static char versionBuffer[128];
    snprintf(versionBuffer, 128, "%s", INVISION_SVR_VERSION);
    return &versionBuffer[0];
}
