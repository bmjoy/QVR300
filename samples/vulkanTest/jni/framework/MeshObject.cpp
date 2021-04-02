//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "MeshObject.h"
#include "VkSampleFramework.h"

///////////////////////////////////////////////////////////////////////////////

MeshObject::MeshObject()
{
    mNumVertices = 0;
    mSample = NULL;
}

///////////////////////////////////////////////////////////////////////////////

MeshObject::~MeshObject()
{
    mNumVertices = 0;
    mSample = NULL;
}

///////////////////////////////////////////////////////////////////////////////

bool MeshObject::LoadTriangle(VkSampleFramework* sample,  uint32_t binding, MeshObject* meshObject)
{
    // Make sure a sample is passed in
    if (sample == nullptr)
    {
        return false;
    }

    // Destroy object before re-creating
    if (meshObject->mSample)
    {
        meshObject->Destroy();
    }

    std::vector<vertex_layout> objVertices;

    vertex_layout vertex;
    vertex.normal[0]    =  0.0f;
    vertex.normal[1]    =  0.0f;
    vertex.normal[2]    =  1.0f;
    vertex.color[0]     =  1.0f;
    vertex.color[1]     =  1.0f;
    vertex.color[2]     =  1.0f;
    vertex.color[3]     =  1.0f;

    /// vertice 0
    vertex.pos[0]       = -0.5f;
    vertex.pos[1]       = -0.5f;
    vertex.pos[2]       =  0.0f;
    vertex.uv[0]        =  0.0f;
    vertex.uv[1]        =  0.0f;
    objVertices.push_back(vertex);

    // vertice 1
    vertex.pos[0]       =  0.5f;
    vertex.pos[1]       = -0.5f;
    vertex.pos[2]       =  0.0f;
    vertex.uv[0]        =  1.0f;
    vertex.uv[1]        =  0.0f;
    objVertices.push_back(vertex);

    // vertice 2
    vertex.pos[0]       =  0.0f;
    vertex.pos[1]       =  0.5f;
    vertex.pos[2]       =  0.0f;
    vertex.uv[0]        =  0.5f;
    vertex.uv[1]        =  1.0f;
    objVertices.push_back(vertex);

    meshObject->mNumVertices = objVertices.size();
    meshObject->mVertexBuffer.InitBuffer(sample, objVertices.size() * sizeof(vertex_layout), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, (void*)&objVertices[0]);
    meshObject->mVertexBuffer.AddBinding(binding, sizeof(vertex_layout), VK_VERTEX_INPUT_RATE_VERTEX);
    meshObject->mVertexBuffer.AddAttribute(binding, 0,                  0, VK_FORMAT_R32G32B32_SFLOAT);    // float3 position
    meshObject->mVertexBuffer.AddAttribute(binding, 1, sizeof(float) *  3, VK_FORMAT_R32G32B32A32_SFLOAT); // float4 color
    meshObject->mVertexBuffer.AddAttribute(binding, 2, sizeof(float) *  7, VK_FORMAT_R32G32_SFLOAT);       // float2 uv
    meshObject->mVertexBuffer.AddAttribute(binding, 3, sizeof(float) *  9, VK_FORMAT_R32G32B32_SFLOAT);    // float3 normal
    meshObject->mVertexBuffer.AddAttribute(binding, 4, sizeof(float) * 12, VK_FORMAT_R32G32B32_SFLOAT);    // float3 binormal
    meshObject->mVertexBuffer.AddAttribute(binding, 5, sizeof(float) * 15, VK_FORMAT_R32G32B32_SFLOAT);    // float3 tangent

    meshObject->mSample = sample;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

// Loads a .obj and .mtl file and builds a single Vertex Buffer containing relevant vertex
// positions, normals and threadColors. Vertices are held in TRIANGLE_LIST format.
//
// Vertex layout corresponds to MeshObject::vertex_layout structure.
bool MeshObject::LoadObj(VkSampleFramework* sample, const char* objFilename,
                         uint32_t binding,
                         MeshObject* meshObject)
{

    // make sure a sample is passed in
    if (sample == nullptr)
    {
        return false;
    }

    // Destroy object before re-creating
    if (meshObject->mSample)
    {
        // meshObject->Destroy();
    }

    class MaterialStringStreamReader: public tinyobj::MaterialReader
    {
    public:
        MaterialStringStreamReader(const std::string& matSStream): m_matSStream(matSStream) {}
        virtual ~MaterialStringStreamReader() {}
        virtual bool operator() (
                const std::string& matId,
                std::vector<tinyobj::material_t>& materials,
                std::map<std::string, int>& matMap,
                std::string& err)
        {
            (void)matId;
            (void)err;
            LoadMtl(matMap, materials, m_matSStream);
            return true;
        }

    private:
        std::stringstream m_matSStream;
    };
    
    AAsset* objectFile = AAssetManager_open(sample->GetAssetManager(), objFilename, AASSET_MODE_BUFFER);
    assert(objectFile);
    size_t size = AAsset_getLength(objectFile);

    const char* objectFileData = (const char*)AAsset_getBuffer(objectFile);
    std::stringstream objectFileStream(objectFileData);

    // Stub for the material stream
    MaterialStringStreamReader materialFileStream("");

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    bool ret = tinyobj::LoadObj(shapes, materials, err, objectFileStream, materialFileStream, true);
    AAsset_close(objectFile);

    if (!err.empty())
    {
        LOGE("tinyobj error: %s\n", err.c_str());
        return false;
    }

    if (!ret)
    {
        LOGE("tinyobj error");
        return false;
    }

    std::vector<vertex_layout> objVertices;

    for (size_t i = 0; i < shapes.size(); i++)
    {
        assert((shapes[i].mesh.indices.size() % 3) == 0);
        for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++)
        {
            uint32_t mat_id = shapes[i].mesh.material_ids[f];
            for (uint32_t idx = 0; idx < 3; idx++)
            {
                uint32_t v = shapes[i].mesh.indices[3 * f + idx];
                vertex_layout vertex;
                vertex.pos[0] =  (shapes[i].mesh.positions[3 * v    ]);
                vertex.pos[1] =  (shapes[i].mesh.positions[3 * v + 1]);
                vertex.pos[2] =  (shapes[i].mesh.positions[3 * v + 2]);
                if (shapes[i].mesh.normals.size() > (3 * v + 2))
                {
                    vertex.normal[0] = shapes[i].mesh.normals[3 * v];
                    vertex.normal[1] = shapes[i].mesh.normals[3 * v + 1];
                    vertex.normal[2] = shapes[i].mesh.normals[3 * v + 2];
                }
                else
                {
                    vertex.normal[0] = 0.0f;
                    vertex.normal[1] = 0.0f;
                    vertex.normal[2] = 1.0f;
                }
                uint32_t uvsize = shapes[i].mesh.texcoords.size();
                if (uvsize > (2 * v + 1))
                {
                    vertex.uv[0] = shapes[i].mesh.texcoords[2 * v];
                    vertex.uv[1] = shapes[i].mesh.texcoords[2 * v + 1];
                }
                else
                {
                    vertex.uv[0] = 0.0f;
                    vertex.uv[1] = 0.0f;
                }

                if (materials.size() > mat_id)
                {
                    vertex.color[0] = materials[mat_id].diffuse[0];
                    vertex.color[1] = materials[mat_id].diffuse[1];
                    vertex.color[2] = materials[mat_id].diffuse[2];
                    vertex.color[3] = 1.0f;
                }
                objVertices.push_back(vertex);
            }
        }
    }

    meshObject->mNumVertices = objVertices.size();
    meshObject->mVertexBuffer.InitBuffer(sample, objVertices.size() * sizeof(vertex_layout), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, (void*)&objVertices[0]);
    meshObject->mVertexBuffer.AddBinding(binding, sizeof(vertex_layout), VK_VERTEX_INPUT_RATE_VERTEX);
    meshObject->mVertexBuffer.AddAttribute(binding, 0,                  0, VK_FORMAT_R32G32B32_SFLOAT);    // float3 position
    meshObject->mVertexBuffer.AddAttribute(binding, 1, sizeof(float) *  3, VK_FORMAT_R32G32B32A32_SFLOAT); // float4 color
    meshObject->mVertexBuffer.AddAttribute(binding, 2, sizeof(float) *  7, VK_FORMAT_R32G32_SFLOAT);       // float2 uv
    meshObject->mVertexBuffer.AddAttribute(binding, 3, sizeof(float) *  9, VK_FORMAT_R32G32B32_SFLOAT);    // float3 normal
    meshObject->mVertexBuffer.AddAttribute(binding, 4, sizeof(float) * 12, VK_FORMAT_R32G32B32_SFLOAT);    // float3 binormal
    meshObject->mVertexBuffer.AddAttribute(binding, 5, sizeof(float) * 15, VK_FORMAT_R32G32B32_SFLOAT);    // float3 tangent
    
    meshObject->mSample = sample;
    
    return true;
}

///////////////////////////////////////////////////////////////////////////////
    
bool MeshObject::Destroy()
{
    mNumVertices = 0;
    mVertexBuffer.Destroy();
    mSample = nullptr;
}
