//
// Created by willie on 2019-12-9
//

#include "PlaneRender.h"
#include <iomanip>
#include <sstream>

namespace InVision {
    int PlaneRender::init(int width, int height) {
        WLOGI("PlaneRender::init start width=%d, height=%d", width, height);
        AbstractRender::init(width, height);
        int status = 0;
        const float sideLen = 0.1f;
        float widthRatio = 1;
        GLfloat vertices[] = {
                -sideLen * widthRatio, sideLen, sideLen, 0, 1, 1,
                -sideLen * widthRatio, -sideLen, sideLen, 0, 0, 1,
                sideLen * widthRatio, sideLen, sideLen, 1, 1, 1,
                sideLen * widthRatio, -sideLen, sideLen, 1, 0, 1,
//                -sideLen * widthRatio, sideLen, -sideLen, 0, 1, 0,
//                -sideLen * widthRatio, -sideLen, -sideLen, 0, 0, 0,
//                sideLen * widthRatio, sideLen, -sideLen, 1, 1, 0,
//                sideLen * widthRatio, -sideLen, -sideLen, 1, 0, 0,
        };
        GLuint indices[] = {
                0, 1, 2, 3,
//                4, 5, 6, 7,
//                0, 1, 4, 5,
//                2, 3, 6, 7,
//                4, 0, 6, 2,
//                5, 1, 7, 3,
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
                "	v_Color = vec4(vertexColor, 0.4);\n"
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

        WLOGI("PlaneRender::init before create vertex shader");
        GLuint vs = createShader(GL_VERTEX_SHADER, vertexShaderStr.c_str());
        if (0 == vs) {
            return -1;
        }
        WLOGI("PlaneRender::init before create fragment shader");
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

        mBaseModelMatrix = glm::translate(glm::mat4(1), glm::vec3(0, 0, 0));
        mModelMatrix = mBaseModelMatrix;

        WLOGI("PlaneRender::init done mProgramId=%u", mProgramId);
        mRenderingStatus = RenderingStatus::INITIALIZED;
        return status;
    }

    int PlaneRender::updateData() {
        return 0;
    }

    void PlaneRender::generateQuads2(float *outVerticesArray, float *normalCenterArray, float quadLen){
        glm::vec3 p = glm::vec3(normalCenterArray[3],normalCenterArray[4],normalCenterArray[5]);
        glm::vec3 n = glm::vec3(normalCenterArray[0],normalCenterArray[1],normalCenterArray[2]);
        float pn = glm::dot(p,n);
        glm::vec3 newPn= pn*n;

        glm::vec3 x = glm::normalize(p-newPn);
        glm::vec3 y = glm::cross(n,x);

        glm::vec3 pointA = p+quadLen/2*x+quadLen/2*y;
        glm::vec3 pointB = p+quadLen/2*x-quadLen/2*y;
        glm::vec3 pointC = p-quadLen/2*x+quadLen/2*y;
        glm::vec3 pointD = p-quadLen/2*x-quadLen/2*y;
        memcpy(outVerticesArray,glm::value_ptr(pointA),3*sizeof(float));
        memcpy(outVerticesArray+3,glm::value_ptr(pointB),3*sizeof(float));
        memcpy(outVerticesArray+6,glm::value_ptr(pointC),3*sizeof(float));
        memcpy(outVerticesArray+9,glm::value_ptr(pointD),3*sizeof(float));

    }

    void PlaneRender::setPlanInfo(float A, float B, float C, float X, float Y, float Z){
        LOGI("@@@before A,B,C,X,Y,Z = %f,%f,%f,%f,%f,%f",A,B,C,X,Y,Z);
        mNormalCenterArray[0] =A;
        mNormalCenterArray[1] =B;
        mNormalCenterArray[2] =C;
        mNormalCenterArray[3] =X;
        mNormalCenterArray[4] =Y;
        mNormalCenterArray[5] =Z;

    }

    int PlaneRender::render(float *projectionMatrix, float *viewMatrix) {

        float scale = 3;
        float sideLen = 0.1f * scale;
        GLfloat vertices[] = {
                -sideLen, sideLen, sideLen,
                -sideLen, -sideLen, sideLen,
                sideLen, sideLen, sideLen,
                sideLen, -sideLen, sideLen,
        };

        float quadLen = scale;
//        float tmp[] = {-0.007595,2.675138,-0.050034,-0.221046,-0.399949,-1.363852};
//        glm::vec3 normal{-0.007595,2.675138,-0.050034};
        float tmp[] = {mNormalCenterArray[0],mNormalCenterArray[1],mNormalCenterArray[2],mNormalCenterArray[3], mNormalCenterArray[4], mNormalCenterArray[5]};
        glm::vec3 normal{mNormalCenterArray[0],mNormalCenterArray[1],mNormalCenterArray[2]};

        normal = glm::normalize(normal);
        LOGI("normalized normal=[%f, %f, %f]", normal[0], normal[1], normal[2]);
        memcpy(tmp, glm::value_ptr(normal), sizeof(float) * 3);
        LOGI("new tmp=[%f, %f, %f, %f, %f, %f]",
                tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);
        generateQuads2(vertices, tmp, quadLen);

        GLfloat color[] = {
//                0, 1, 1,
//                0, 0, 1,
//                1, 1, 1,
//                1, 0, 1,
                0, 1, 0,
                0, 1, 0,
                0, 1, 0,
                0, 1, 0,
        };
        float total[24];
        memcpy(total, vertices, sizeof(vertices));
        memcpy(total + 12, color, sizeof(color));

        glBindVertexArray(mVao);
        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(total), total, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *) 0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                              (GLvoid *) (sizeof(vertices)));
        glEnableVertexAttribArray(1);


        mRenderingStatus = RenderingStatus::RENDERING;
//        glClearColor(0, 0, 0, 1);
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);


        glUseProgram(mProgramId);
        glUniformMatrix4fv(mUniformPLoc, 1, GL_FALSE, projectionMatrix);
        glUniformMatrix4fv(mUniformMLoc, 1, GL_FALSE, glm::value_ptr(mModelMatrix));

        glUniformMatrix4fv(mUniformVLoc, 1, GL_FALSE, viewMatrix);
        glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, NULL);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        return 0;
    }


    int PlaneRender::release() {
        if (RenderingStatus::RELEASED  == mRenderingStatus) {
            WLOGI("PlaneRender::release no need to release again");
            return 0;
        }
        WLOGI("PlaneRender::release start");
        glDeleteProgram(mProgramId);
        glDeleteVertexArrays(1, &mVao);
        mRenderingStatus = RenderingStatus::RELEASED;
        WLOGI("PlaneRender::release done");
        return 0;
    }

}