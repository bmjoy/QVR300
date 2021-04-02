//
// Created by willie on 2019-12-9
//

#ifndef SC_DEMO_RENDER_H
#define SC_DEMO_RENDER_H
#include "utils.h"
#include "AbstractRender.h"

namespace InVision {
    class PlaneRender final : public AbstractRender {
    public:
        PlaneRender() = default;

        virtual ~PlaneRender() = default;

        virtual int init(int width, int height) override;

        virtual int updateData() override;

        virtual int render(float *projectionMatrix, float *viewMatrix) override;

        virtual int release() override;

        void setPlanInfo(float A,float B,float C,float X,float Y,float Z);

        void generateQuads2(float *outVerticesArray, float *normalCenterArray, float quadLen);

    private:
        int mProgramId = 0;
        int mUniformPLoc = 0;
        int mUniformVLoc = 0;
        int mUniformMLoc = 0;
        uint mVao;
        uint mVbo;

        glm::mat4 mBaseModelMatrix;
        glm::mat4 mModelMatrix;
        float mNormalCenterArray[6]={0,0,1,0,0,0};
    };
}


#endif //SC_DEMO_RENDER_H