//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once

class SharedMemory {
    public:
        static int allocate(int size);
        static int getSize(int fd);
        static void* map(int fd, int size, bool bReadOnly = true);
        static void free(int fd, void* address, int size); 
};
