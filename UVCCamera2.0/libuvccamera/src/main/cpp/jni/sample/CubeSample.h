//
// Created by 86150 on 2020/10/14.
//

#ifndef UVCCAMERA_MASTER_CUBESAMPLE_H
#define UVCCAMERA_MASTER_CUBESAMPLE_H

#include "GLSampleBase.h"
#include "detail/type_mat.hpp"
#include "shader.h"
#include "ImageDef.h"

class CubeSample : public GLSampleBase  {
public:
    CubeSample();

    virtual ~CubeSample();
    virtual void LoadImage(NativeImage *pImage);
    virtual void Init();
    virtual void Draw(int screenW, int screenH);

    virtual void Destroy();

    virtual void UpdateTransformMatrix(float rotateX, float rotateY, float scaleX, float scaleY);

    void UpdateMVPMatrix(glm::mat4 &mvpMatrix, int angleX, int angleY, float ratio);

private:
    glm::mat4 m_MVPMatrix;
    glm::mat4 m_ModelMatrix;
    Shader *m_pShader;
};


#endif //UVCCAMERA_MASTER_CUBESAMPLE_H
