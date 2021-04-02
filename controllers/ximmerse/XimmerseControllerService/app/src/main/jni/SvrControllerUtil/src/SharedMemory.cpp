//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#include "SharedMemory.h"
#include <sys/mman.h>
#include <linux/ashmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <android/log.h>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SharedMemory", __VA_ARGS__)

//-----------------------------------------------------------------------------
int SharedMemory::getSize(int fd)
{
    struct stat buf;
    fstat(fd, &buf);
    return buf.st_size;
}

//-----------------------------------------------------------------------------
void SharedMemory::free(int fd, void* address, int fdSize)
{
    LOGE("free fd(%d) fdSize(%d)", fd, fdSize);
    munmap(address, fdSize);
    close(fd);
}

//-----------------------------------------------------------------------------
void* SharedMemory::map(int fd, int fdSize, bool bReadOnly)
{
    int prot = PROT_READ;
    if( bReadOnly == false )
    {
        prot = prot | PROT_WRITE;
    }
    
    void* address = (mmap(NULL, fdSize, prot, MAP_SHARED, fd, 0));
    
    LOGE("map fd(%d) fdSize(%d) getSize(%d) address(0x%08x)", fd, fdSize, getSize(fd), (unsigned int)(address));
    
    return address;
}

//-----------------------------------------------------------------------------
int SharedMemory::allocate(int size)
{    
    int fd = open("/dev/ashmem", O_RDWR);
    ioctl(fd, ASHMEM_SET_NAME, "my_mem");
    ioctl(fd, ASHMEM_SET_SIZE, size);
    int* address = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if( address == MAP_FAILED )
    {
        close(fd);
        fd = -1;
    }
    else {
        munmap(address, size);
    }
    
    LOGE("allocate fd(%d) fdSize(%d)", fd, size);
    
    return fd;
}
