//
// Created by ByteFlow on 2019/7/30.
//

#ifndef NDK_OPENGLES_3_0_MODEL3DSample_H
#define NDK_OPENGLES_3_0_MODEL3DSample_H


#include <detail/type_mat.hpp>
#include <detail/type_mat4x4.hpp>
#include <shader.h>
#include <model.h>
#include "GLSampleBase.h"

class Model3DSample : public GLSampleBase
{
public:
	Model3DSample();

	virtual ~Model3DSample();

	virtual void LoadImage(NativeImage *pImage);

	virtual void Init();
	virtual void Draw(int screenW, int screenH);

	virtual void Destroy();

	virtual void UpdateTransformMatrix(float rotateX, float rotateY, float scaleX, float scaleY);

	void UpdateMVPMatrix(glm::mat4 &mvpMatrix, glm::mat4 &mvpMatrix2, int angleX, int angleY, float ratio);

private:
	glm::mat4  m_MVPMatrixRightEye;
	glm::mat4 m_ModelMatrix;
	glm::mat4 m_MVPMatrixLeftEye;
	Shader *m_pShader;
	Model *m_pModel;

	int m_AngleX;
	int m_AngleY;
	float m_ScaleX;
	float m_ScaleY;

};

//class Matrix4x4 {
//public:
//	float m[4][4];
//
//	// Multiplies two matrices.
//	Matrix4x4 operator*(const Matrix4x4& right);
//};



#endif //NDK_OPENGLES_3_0_MODEL3DSample_H
