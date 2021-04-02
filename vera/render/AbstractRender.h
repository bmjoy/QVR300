//
// Created by xwkol on 2020/3/29.
//

#pragma once

#include <Common.h>
#include <GLES3/gl31.h>

namespace InVision {
    enum class RenderingStatus {
        NONE,
        INITIALIZED,
        RENDERING,
        RELEASED,
    };

    class AbstractRender {
    public:
        AbstractRender() = default;

        virtual ~AbstractRender() = default;

        virtual int init(int width, int height);

        virtual int updateData() = 0;

        virtual int render(float *projectionMatrix, float *viewMatrix) = 0;

        virtual int release() = 0;

    protected:
        uint createShader(GLenum type, const char *shaderSource);

    protected:
        int mScreenWidth;
        int mScreenHeight;
        RenderingStatus mRenderingStatus = RenderingStatus::NONE;

    private:
        const std::string IMG_PATH_PREFIX = "/sdcard/InVisionFBODemo_";
        const std::string IMG_PATH_SUFFIX = ".jpg";
    };
}

