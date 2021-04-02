//=============================================================================
// FILE: svrGeometry.h
//
//                  Copyright (c) 2015 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================
#pragma once

#define MAX_ATTRIBUTES 8

namespace Svr
{
    struct SvrProgramAttribute
    {
        unsigned int    index;
        int             size;
        unsigned int    type;
        bool            normalized;
        int             stride;
		int				offset;
    };

    class SvrGeometry
    {
    public:
        SvrGeometry();

        void Initialize(SvrProgramAttribute* pAttribs, int nAttribs,
                        unsigned int* pIndices, int nIndices,
                        const void* pVertexData, int bufferSize, int nVertices);

        void Update(const void* pVertexData, int bufferSize, int nVertices);

        void Destroy();
        void Submit();
        void Submit(SvrProgramAttribute* pAttribs, int nAttribs);

        static void CreateFromObjFile(const char* pObjFilePath, SvrGeometry** pOutGeometry, int& outNumGeometry);

        unsigned int GetVbId() { return mVbId; }
        unsigned int GetIbId() { return mIbId; }
        unsigned int GetVaoId() { return mVaoId; }
        int GetVertexCount() { return mVertexCount; }
        int GetIndexCount() { return mIndexCount; }

    private:
        unsigned int    mVbId;
        unsigned int    mIbId;
        unsigned int    mVaoId;
        int             mVertexCount;
        int             mIndexCount;
    };

}
