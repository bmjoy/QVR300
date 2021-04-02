//
// Created by 56873 on 2021/3/26.
//

#ifndef SIMPLEVR_CUBERENDER_H
#define SIMPLEVR_CUBERENDER_H
#include "utils.h"
#include "AbstractRender.h"

namespace InVision {
    class CubeRender final : public AbstractRender {
    public:
        CubeRender() = default;

        virtual ~CubeRender() = default;

        virtual int init(int width, int height) override;

        virtual int updateData() override;

        virtual int render(float *projectionMatrix, float *viewMatrix) override;

        virtual int release() override;

        void updateCubePosition(float x,float y ,float z);

        void setFindPlan(bool find);
    private:
        int mProgramId = 0;
        int mUniformPLoc = 0;
        int mUniformVLoc = 0;
        int mUniformMLoc = 0;
        uint mVao;
        uint mVbo;

        //glm::mat4 mBaseModelMatrix;
        glm::mat4 mModelMatrix;
        bool mFindPlan = false;
        float positionX = 0.0f;
        float positionY = -0.2f;
        float positionZ = -1.0f;
    };
}

#endif //SIMPLEVR_CUBERENDER_H
