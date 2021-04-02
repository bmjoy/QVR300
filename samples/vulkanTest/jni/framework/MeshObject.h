//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
#pragma once

#include <assert.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <android/asset_manager.h>
#include <sstream>
#include <tiny_obj_loader.h>

#include "BufferObject.h"

#if !defined(nullptr)
#define nullptr NULL
#endif

// Defines a simple object for creating and holding Vulkan state corresponding to simple meshes.

class VkSampleFramework;

class MeshObject
{
public:
    MeshObject();
    ~MeshObject();
    
    static bool LoadObj(VkSampleFramework* sample, const char* objFilename,
                        uint32_t binding,
                        MeshObject* meshObject);

    static bool LoadTriangle(VkSampleFramework* sample,  uint32_t binding, MeshObject* meshObject);
    virtual bool Destroy();

    struct vertex_layout
    {
        float pos[3];
        float color[4];
        float uv[2];
        float normal[3];
        float binormal[3];
        float tangent[4];
    };
    
    VertexBufferObject& Buffer()
    {
        return mVertexBuffer;
    }
    
    uint32_t GetNumVertices()
    {
        return mNumVertices;
    }
    
protected:
    uint32_t mNumVertices;
    VertexBufferObject mVertexBuffer;
    VkSampleFramework* mSample;
};
