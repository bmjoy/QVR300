//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

#pragma once
#include "SharedMemory.h"
#include <android/log.h>
#include <string.h>

#define RINGBUFFER_VERSION 0x01
/*
 * Ring Buffer used to share Memory between svr service 
 * and clients and controller modules
 *
 */
template<class T>
class RingBuffer {
    private:
	class Header {
        public:
            int version;
            int itemSize;
            int itemCount;
            int index;
            int baseItemOffset;
	};
    private:
        void* bufferAddress;        
        int bufferSize;
        int fileDescriptor;
    private:
        //-----------------------------------------------------------------------------
        int getBufferSize(int itemCount)
        {
            return sizeof(T)*itemCount + sizeof(Header);
        }
    public:
        //-----------------------------------------------------------------------------
        ~RingBuffer()
        {
            free();
        }
        
        //-----------------------------------------------------------------------------
        int getFd()
        {
            return fileDescriptor;
        }
        
        //-----------------------------------------------------------------------------
        int getBufferSize()
        {
            return bufferSize;
        }
        //-----------------------------------------------------------------------------
        // Used for Reading the data
        //static RingBuffer<T>* fromFd(int fd, bool bReadOnly = true)
        //{
        //    RingBuffer* ringBuffer = 0;
        //    void* mappedAddr =  SharedMemory::map(fd, bReadOnly);
        //    if( mappedAddr != false )
        //    {
        //        ringBuffer = new RingBuffer();
        //        ringBuffer->bufferSize = SharedMemory::getSize(fd);
        //        ringBuffer->fileDescriptor = fd;
        //        ringBuffer->bufferAddress = mappedAddr;
        //    }
        //    
        //    return ringBuffer;
        //}
        //-----------------------------------------------------------------------------
        // Used for Reading the data
        static RingBuffer<T>* fromFd(int fd, int fdSize, bool bReadOnly = true)
        {
            RingBuffer* ringBuffer = 0;
            void* mappedAddr =  SharedMemory::map(fd, fdSize, bReadOnly);
            if( mappedAddr != false )
            {
                ringBuffer = new RingBuffer();
                ringBuffer->bufferSize = fdSize;
                ringBuffer->fileDescriptor = fd;
                ringBuffer->bufferAddress = mappedAddr;
            }
            
            return ringBuffer;
        }
	public:
        //-----------------------------------------------------------------------------
        // Used for Writing the data
        void allocate(int count)
        {
            this->bufferSize = getBufferSize(count);
            this->fileDescriptor = SharedMemory::allocate(this->bufferSize);
            this->bufferAddress = SharedMemory::map(fileDescriptor, this->bufferSize, false);
            {
                Header* header = (Header*)(this->bufferAddress);
                header->version = RINGBUFFER_VERSION;
                header->itemSize = sizeof(T);
                header->itemCount = count;
                header->index = -1;
                header->baseItemOffset = sizeof(Header);
            }
            
        }
        
        //-----------------------------------------------------------------------------
        void free()
        {
            SharedMemory::free(fileDescriptor, bufferAddress, bufferSize);
            fileDescriptor = 0;
            bufferAddress = 0;
            bufferSize = 0;
        }
    public:
        //-----------------------------------------------------------------------------
        void set(T* state)
        {
            Header* header = (Header*)(bufferAddress);
            if( header != 0x00 )
            {
                int indexToWrite = (header->index + 1)%header->itemCount;
                char* baseAddress = (char*)(((char*)bufferAddress)+header->baseItemOffset);
                void* address = (void*)(baseAddress + indexToWrite*header->itemSize);
                memcpy(address, state, header->itemSize);
                header->index = indexToWrite;
            }
        }

        //-----------------------------------------------------------------------------
        T* get()
        {
            Header* header = (Header*)(bufferAddress);
            void* address = 0;
            if( (header != 0x00) && (header->index >= 0) )
            {
                char* baseAddress = (char*)(((char*)bufferAddress)+header->baseItemOffset);
                address = (void*)(((char*)baseAddress + (header->index * header->itemSize)));
            }
            return (T*)address;
        }
};