//
// Created by xwkol on 2020/3/29.
//

#include "AbstractRender.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <sstream>

namespace InVision {
    int AbstractRender::init(int width, int height) {
        mScreenWidth = width;
        mScreenHeight = height;
        WLOGI("AbstractRender::init done mScreenWidth=%d, mScreenHeight=%d", mScreenWidth,
              mScreenHeight);
        return 0;
    }

    uint AbstractRender::createShader(GLenum type, const char *shaderSource) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &shaderSource, NULL);
        glCompileShader(shader);
        GLint res;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
        if (GL_FALSE == res) {
            GLchar msg[4096];
            GLsizei length;
            glGetShaderInfoLog(shader, sizeof(msg), &length, msg);
            WLOGE("AbstractRender::createShader Failed for %s", msg);
            return 0;
        }
        return shader;
    }

}