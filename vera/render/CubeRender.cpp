//
// Created by 56873 on 2021/3/26.
//

#include "CubeRender.h"

namespace InVision {
    int CubeRender::init(int width, int height) {
        WLOGI("CubeRender::init start width=%d, height=%d", width, height);
        AbstractRender::init(width, height);
        int status = 0;
        const float sideLen = 0.1f;
        float widthRatio = 1;
        GLfloat vertices[] = {
                -sideLen * widthRatio, sideLen, sideLen, 0, 1, 1,
                -sideLen * widthRatio, -sideLen, sideLen, 0, 0, 1,
                sideLen * widthRatio, sideLen, sideLen, 1, 1, 1,
                sideLen * widthRatio, -sideLen, sideLen, 1, 0, 1,
                -sideLen * widthRatio, sideLen, -sideLen, 0, 1, 0,
                -sideLen * widthRatio, -sideLen, -sideLen, 0, 0, 0,
                sideLen * widthRatio, sideLen, -sideLen, 1, 1, 0,
                sideLen * widthRatio, -sideLen, -sideLen, 1, 0, 0,
        };
        GLuint indices[] = {
                0, 1, 2, 3,
                4, 5, 6, 7,
                0, 1, 4, 5,
                2, 3, 6, 7,
                4, 0, 6, 2,
                5, 1, 7, 3,
        };

        GLuint vio;
        glGenVertexArrays(1, &mVao);
        glGenBuffers(1, &mVbo);
        glGenBuffers(1, &vio);
        glBindVertexArray(mVao);
        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vio);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid *) 0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                              (GLvoid *) (3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        const std::string vertexShaderStr =
                "#version 310 es\n"
                "uniform mat4 ProjectionMatrix;\n"
                "uniform mat4 ViewMatrix;\n"
                "uniform mat4 ModelMatrix;\n"
                "in mediump vec3 vertexPosition;\n"
                "in mediump vec3 vertexColor;\n"
                "out mediump vec4 v_Color;\n"
                "\n"
                "void main( void )\n"
                "{\n"
                "	vec4 vertexWorldPos = ModelMatrix * vec4( vertexPosition, 1.0 );\n"
                "	gl_Position = ProjectionMatrix * ( ViewMatrix * vertexWorldPos );\n"
                "	v_Color = vec4(vertexColor, 1.0);\n"
                "}\n";
        const std::string fragmentShaderStr =
                "#version 310 es\n"
                "precision mediump float;\n"
                "in mediump vec4 v_Color;\n"
                "out mediump vec4 outColor;\n"
                "void main()\n"
                "{\n"
                "	outColor = v_Color;\n"
                "}\n";

        WLOGI("CubeRender::init before create vertex shader");
        GLuint vs = createShader(GL_VERTEX_SHADER, vertexShaderStr.c_str());
        if (0 == vs) {
            return -1;
        }
        WLOGI("CubeRender::init before create fragment shader");
        GLuint fs = createShader(GL_FRAGMENT_SHADER, fragmentShaderStr.c_str());
        if (0 == fs) {
            return -1;
        }

        mProgramId = glCreateProgram();
        glAttachShader(mProgramId, vs);
        glAttachShader(mProgramId, fs);
        glLinkProgram(mProgramId);
        int res = 0;
        glGetProgramiv(mProgramId, GL_LINK_STATUS, &res);
        if (GL_FALSE == res) {
            GLchar msg[4096];
            glGetProgramInfoLog(mProgramId, sizeof(msg), 0, msg);
            WLOGE("UIManager::subThreadRender Failed for Linking program: %s\n",
                  msg);
            status = -1;
            return status;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        mUniformPLoc = glGetUniformLocation(mProgramId, "ProjectionMatrix");
        mUniformVLoc = glGetUniformLocation(mProgramId, "ViewMatrix");
        mUniformMLoc = glGetUniformLocation(mProgramId, "ModelMatrix");

        mModelMatrix = glm::translate(glm::mat4(1), glm::vec3(positionX, positionY, positionZ));

        WLOGI("CubeRender::init done mProgramId=%u", mProgramId);
        mRenderingStatus = RenderingStatus::INITIALIZED;
        return status;
    }
    void CubeRender::updateCubePosition(float X,float Y,float Z){
        WLOGI("@@@ updateCubePosition:: %f,%f,%f ", X,Y,Z);

        positionX = X;
        positionY = Y;// moveup,to place on the plan
        positionZ = Z;
    }
    void CubeRender::setFindPlan(bool find){
        mFindPlan = find;
    }
    int CubeRender::updateData() {
        return 0;
    }


    int CubeRender::render(float *projectionMatrix, float *viewMatrix) {

        glBindVertexArray(mVao);

        mRenderingStatus = RenderingStatus::RENDERING;
        //if (!mFindPlan) {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //}
        glEnable(GL_DEPTH_TEST);

        glUseProgram(mProgramId);
        glUniformMatrix4fv(mUniformPLoc, 1, GL_FALSE, projectionMatrix);

        mModelMatrix = glm::translate(glm::mat4(1), glm::vec3(positionX, positionY+0.1f, positionZ));

        glUniformMatrix4fv(mUniformMLoc, 1, GL_FALSE, glm::value_ptr(mModelMatrix));

        glUniformMatrix4fv(mUniformVLoc, 1, GL_FALSE, viewMatrix);
        glDrawElements(GL_TRIANGLE_STRIP, 24, GL_UNSIGNED_INT, NULL);
        return 0;
    }

    int CubeRender::release() {
        if (RenderingStatus::RELEASED  == mRenderingStatus) {
            WLOGI("CubeRender::release no need to release again");
            return 0;
        }
        WLOGI("CubeRender::release start");
        glDeleteProgram(mProgramId);
        glDeleteVertexArrays(1, &mVao);
        mRenderingStatus = RenderingStatus::RELEASED;
        WLOGI("CubeRender::release done");
        return 0;
    }
}