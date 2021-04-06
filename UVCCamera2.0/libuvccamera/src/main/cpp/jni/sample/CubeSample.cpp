//
// Created by 86150 on 2020/10/14.
//

#include <gtc/matrix_transform.hpp>
#include "CubeSample.h"
#include "ARCORE_API.h"

static float r = 0.5f;
static float vertexBuffer[]{
        -r, r, r,//0
        -r, -r, r,//1
        r, -r, r,//2
        r, r, r,//3
        r, -r, -r,//4
        r, r, -r,//5
        -r, -r, -r,//6
        -r, r, -r,//7
};

static short indices[]{
        0, 1, 2, 0, 2, 3,
        3, 2, 4, 3, 4, 5,
        5, 4, 6, 5, 6, 7,
        7, 6, 1, 7, 1, 0,
        7, 0, 3, 7, 3, 5,
        6, 1, 2, 6, 2, 4
};

static float colors[]{
        1.f, 0.f, 0.f, 1.f,
        0.f, 1.f, 0.f, 1.f,
        0.f, 0.f, 1.f, 1.f,
        1.f, 1.f, 0.f, 1.f,
        1.f, 0.f, 1.f, 1.f,
        0.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, 0.f, 0.f, 1.f
};

CubeSample::CubeSample() {
    m_pShader = nullptr;
}

CubeSample::~CubeSample() {

}

void CubeSample::Init() {
    if (m_pShader != nullptr)
        return;

    char vShaderStr[] =
            "attribute vec4 a_Position;\n"
            "attribute vec4 a_color;\n"
            "uniform mat4 mvpMatrix;\n"
            "varying vec4 v_color;\n"
            "void main()\n"
            "{\n"
            "    v_color = a_color;\n"
            "    gl_Position = mvpMatrix * a_Position;\n"
            "}";

    char fShaderStr[] =
            "precision mediump float;\n"
            "uniform vec4 u_color;\n"
            "varying vec4 v_color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = v_color;\n"
            "}";

    m_pShader = new Shader(vShaderStr, fShaderStr);
}

void CubeSample::LoadImage(NativeImage *pImage) {

}

//void CubeSample::Draw(int screenW, int screenH) {
//    if(m_pShader == nullptr) {
//        return;
//    }
//    glClearColor(0.f, 0.f, 0.f, 0.f);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glEnable(GL_DEPTH_TEST);
//    glEnable(GL_BLEND);// 启用混合模式
//    glBlendFunc(GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA);
//
//    static float pose[16];
//    memset(pose, 0, sizeof(pose));
//    ARCORE_get_pose(pose, 0.02f);
//
//    glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -0.5f));
//    glm::mat4 projection_mat = glm::perspective(26.8957f, (GLfloat) 1280.0f / (GLfloat) 720.0f, 0.3f,
//                                            6.0f);
//    glm::mat4 Tcw = glm::mat4(
//            pose[0], pose[1], pose[2], pose[3],
//            pose[4], pose[5], pose[6], pose[7],
//            pose[8], pose[9], pose[10], pose[11],
//            pose[12], pose[13], pose[14], pose[15]
//    );
//    glm::mat4 T(1.0f);
//    T[1][1] = T[2][2] = -1;
//    glm::mat4 view_mat = T * Tcw * T;
//    m_pShader->use();
//    m_pShader->setVertexAttribPointer("a_Position", vertexBuffer);
//    m_pShader->setVertexAttribPointer("a_color", colors);
//    m_pShader->setMat4("mvpMatrix", projection_mat * view_mat * model_mat);
//    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, indices);
//    glUseProgram(0);
//}
//
void CubeSample::Draw(int screenW, int screenH) {
    if (m_pShader == nullptr) {
        LOGCATD("pShader is null");
        return;
    }

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);// 启用混合模式
    glBlendFunc(GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA); // 设置半透明

    static float pose[16];
    memset(pose, 0, sizeof(pose));
    ARCORE_get_pose(pose, 0.029f);
//    LOGCATD("before --> %f : %f : %f", pose[12], pose[13], pose[14]);

    glViewport(0, 0, (GLsizei) 1920, (GLsizei) 1080);

    glm::mat4 projection_mat = glm::perspective(26.8957f, (GLfloat) 1280.0f / (GLfloat) 720.0f, 0.3f,
                                                6.0f);

    glm::mat4 pose_mat = glm::mat4(
            pose[0], pose[1], pose[2], pose[3],
            pose[4], pose[5], pose[6], pose[7],
            pose[8], pose[9], pose[10], pose[11],
            pose[12], pose[13], pose[14], pose[15]
    );

    glm::mat4 model_view = glm::lookAt(glm::vec3(0.02f, 0.02f, -2.045f),
            glm::vec3(0.02f, 2.02f, 5.f),
            glm::vec3(0.0f, -1.0f, 0.0f)) ;


    m_pShader->use();
    m_pShader->setVertexAttribPointer("a_Position", vertexBuffer);
    m_pShader->setVertexAttribPointer("a_color", colors);
    m_pShader->setMat4("mvpMatrix", projection_mat*model_view);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, indices);
}

void CubeSample::Destroy() {

}

void CubeSample::UpdateMVPMatrix(glm::mat4 &mvpMatrix, int angleX, int angleY, float ratio) {

}

void CubeSample::UpdateTransformMatrix(float rotateX, float rotateY, float scaleX, float scaleY) {
}