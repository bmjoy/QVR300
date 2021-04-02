//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
package com.qualcomm.svr.apps.photoViewer;

import android.app.Activity;
import android.content.Context;
import android.opengl.GLES30;
import android.opengl.Matrix;

import com.qualcomm.svrapi.SvrApi.svrQuaternion;
import com.qualcomm.svrapi.SvrApi.svrVector3;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import static com.qualcomm.svr.apps.photoViewer.svrView.mSvrFrameParams;
import static com.qualcomm.svr.apps.photoViewer.svrView.svrApiRenderer;

public class photoViewerRenderer implements svrView.svrRenderer {
    public photoViewerRenderer(Activity parentActivity, Context mContext) {
        m_ParentActivity = parentActivity;
        mActivityContext = mContext;
        mSvrSphere = new svrSphereMesh(36, 72, false);
        rotationQuat = svrApiRenderer.new svrQuaternion();

        mIndices = ByteBuffer.allocateDirect(mSvrSphere.sphereIndices.length * mBytesPerChar)
                .order(ByteOrder.nativeOrder()).asCharBuffer();
        mIndices.put(mSvrSphere.sphereIndices).position(0);

        mVertice = ByteBuffer.allocateDirect(mSvrSphere.sphereVertices.length * mBytesPerFloat)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        mVertice.put(mSvrSphere.sphereVertices).position(0);
    }

    @Override
    public void onSvrBegin(GL10 glUnused, EGLConfig config) {
        // Set the background clear color to gray.
        GLES30.glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
        initializeFBORenderTarget();
        mSvrFrameParams.renderLayers[0].imageHandle = mLefEye.mColorAttachmentId;
        mSvrFrameParams.renderLayers[1].imageHandle = mRightEye.mColorAttachmentId;
        sphereTextureShaderInit();
        sphereDataInit();
        initViewMatrix();
    }

    @Override
    public void onSvrViewChanged(GL10 glUnused, int width, int height) {
        mWidthSurface = width;
        mHightSurface = height;
        // Set the OpenGL viewport to the same size as the surface.
        GLES30.glViewport(0, 0, width, height);
        float fovy = 90.0f;
        float aspect = (float) mLefEye.mWidth / (float) mLefEye.mHeight;
        float zNear = 0.1f;
        float zFar = 25.0f;
        Matrix.perspectiveM(mProjectionMatrix, 0, fovy, aspect, zNear, zFar);
    }

    @Override
    public void onSvrSubmitFrame(GL10 glUnused) {
        GLES30.glClear(GLES30.GL_DEPTH_BUFFER_BIT | GLES30.GL_COLOR_BUFFER_BIT);
        Matrix.setIdentityM(mModelMatrix, 0);
        //get quaternion data from qvr service for view matrix
        updateViewMatrix();

        GLES30.glDisable(GLES30.GL_CULL_FACE);
        GLES30.glCullFace(GLES30.GL_BACK);
        GLES30.glEnable(GLES30.GL_DEPTH_TEST);
        //draw left eye to FBO
        mLefEye.Bind();
        GLES30.glViewport(0, 0, mLefEye.mWidth, mLefEye.mHeight);
        GLES30.glScissor(0, 0, mLefEye.mWidth, mLefEye.mHeight);
        GLES30.glClear(GLES30.GL_DEPTH_BUFFER_BIT | GLES30.GL_COLOR_BUFFER_BIT);
        drawSphere();
        mLefEye.Unbind();

        //draw right eye to FBO
        mRightEye.Bind();
        GLES30.glViewport(0, 0, mRightEye.mWidth, mRightEye.mHeight);
        GLES30.glScissor(0, 0, mRightEye.mWidth, mRightEye.mHeight);
        GLES30.glClear(GLES30.GL_DEPTH_BUFFER_BIT | GLES30.GL_COLOR_BUFFER_BIT);
        drawSphere();
        mRightEye.Unbind();
    }


    public void sphereTextureShaderInit() {
        final String vertexShader = photoViewerUtil.textureFromRawResource(mActivityContext, R.raw.sphere_vs);
        final String fragmentShader = photoViewerUtil.textureFromRawResource(mActivityContext, R.raw.sphere_fs);

        final int vertexShaderHandle = photoViewerUtil.shaderCompile(GLES30.GL_VERTEX_SHADER, vertexShader);
        final int fragmentShaderHandle = photoViewerUtil.shaderCompile(GLES30.GL_FRAGMENT_SHADER, fragmentShader);

        mSphereProgramHandle = photoViewerUtil.shaderProgramCreateAndLink(vertexShaderHandle, fragmentShaderHandle,
                new String[]{"a_position", "a_texcoord"});
    }

    public void sphereDataInit() {

        mTextureDataHandle = photoViewerUtil.textureLoad(mActivityContext, R.drawable.stevepool);
        //vertex shader uniform
        mVsProjHandle = GLES30.glGetUniformLocation(mSphereProgramHandle, "u_proj");
        mVsViewHandle = GLES30.glGetUniformLocation(mSphereProgramHandle, "u_view");
        mVsModelHandle = GLES30.glGetUniformLocation(mSphereProgramHandle, "u_model");
        //fragment shader uniform
        mFsTextureHandle = GLES30.glGetUniformLocation(mSphereProgramHandle, "u_texture");
        mFsColorHandle = GLES30.glGetUniformLocation(mSphereProgramHandle, "u_color");
        mFsOpacityHandle = GLES30.glGetUniformLocation(mSphereProgramHandle, "u_opacity");

        GLES30.glUseProgram(mSphereProgramHandle);

        GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, mTextureDataHandle);
        GLES30.glUniform1i(mFsTextureHandle, 0);

        svrVector3 colorVec = svrApiRenderer.new svrVector3(1.0f, 1.0f, 1.0f);
        GLES30.glUniform3f(mFsColorHandle, colorVec.x, colorVec.y, colorVec.z);

        float opacity = 1.0f;
        GLES30.glUniform1f(mFsOpacityHandle, opacity);

        GLES30.glGenVertexArrays(1, mVAOId, 0);

        GLES30.glGenBuffers(1, mIndicesVboID, 0);
        GLES30.glGenBuffers(1, mVerticeVboID, 0);

        GLES30.glBindVertexArray(mVAOId[0]);

        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, mIndicesVboID[0]);
        mIndices.position(0);
        GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, mSvrSphere.sphereIndices.length * mBytesPerChar,
                mIndices, GLES30.GL_STATIC_DRAW);
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, mVerticeVboID[0]);
        mVertice.position(0);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, mSvrSphere.sphereVertices.length * mBytesPerFloat,
                mVertice, GLES30.GL_STATIC_DRAW);

        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, mVerticeVboID[0]);
        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, mIndicesVboID[0]);

        GLES30.glEnableVertexAttribArray(VERTEX_POS_INDX);
        GLES30.glEnableVertexAttribArray(VERTEX_TEXCOORDS_INDX);

        GLES30.glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GLES30.GL_FLOAT, false, VERTEX_STRIDE, 0);
        GLES30.glVertexAttribPointer(VERTEX_TEXCOORDS_INDX, VERTEX_TEXCOORDS_SIZE
                , GLES30.GL_FLOAT, false, VERTEX_STRIDE, VERTEX_POS_STRIDE);
    }

    public void drawSphere() {

        GLES30.glBindVertexArray(mVAOId[0]);

        GLES30.glUniformMatrix4fv(mVsModelHandle, 1, false, mModelMatrix, 0);
        GLES30.glUniformMatrix4fv(mVsViewHandle, 1, false, mViewMatrix, 0);
        GLES30.glUniformMatrix4fv(mVsProjHandle, 1, false, mProjectionMatrix, 0);

        GLES30.glDrawElements(GLES30.GL_TRIANGLES, mIndices.length(), GLES30.GL_UNSIGNED_SHORT, 0);
        //GLES30.glBindVertexArray(0);
    }

    public void updateViewMatrix() {

        rotationQuat.x = mSvrFrameParams.headPoseState.pose.rotation.x;
        rotationQuat.y = mSvrFrameParams.headPoseState.pose.rotation.y;
        rotationQuat.z = mSvrFrameParams.headPoseState.pose.rotation.z;
        rotationQuat.w = mSvrFrameParams.headPoseState.pose.rotation.w;
        mViewMatrix = rotationQuat.quatToMatrix().queueInArray();

    }

    public void initViewMatrix() {
        final float eyeX = 0.0f;
        final float eyeY = 0.0f;
        final float eyeZ = 0.0f;

        final float lookX = 0.0f;
        final float lookY = 0.0f;
        final float lookZ = -1.0f;

        final float upX = 0.0f;
        final float upY = 0.0f;
        final float upZ = 0.0f;

        Matrix.setLookAtM(mViewMatrix, 0,
                eyeX, eyeY, eyeZ,  //position
                lookX, lookY, lookZ, // target
                upX, upY, upZ); // Up
    }

    public void initializeFBORenderTarget() {
        mLefEye = new renderTarget(1024, 1024, GLES30.GL_RGBA, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE);
        mRightEye = new renderTarget(1024, 1024, GLES30.GL_RGBA, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE);
    }

    private int[] mVAOId = new int[1];
    private int[] mIndicesVboID = new int[1];
    private int[] mVerticeVboID = new int[1];

    private CharBuffer mIndices;
    private FloatBuffer mVertice;

    svrSphereMesh mSvrSphere;
    private int mTextureDataHandle;


    private final int mBytesPerFloat = 4;
    private final int mBytesPerChar = 2;

    final int VERTEX_POS_INDX = 0;
    final int VERTEX_TEXCOORDS_INDX = 1;
    final int VERTEX_POS_SIZE = 3;
    final int VERTEX_TEXCOORDS_SIZE = 2;
    final int VERTEX_POS_STRIDE = (mBytesPerFloat * VERTEX_POS_SIZE);
    final int VERTEX_TEXCOORDS_STRIDE = (mBytesPerFloat * VERTEX_TEXCOORDS_SIZE);
    final int VERTEX_STRIDE = VERTEX_POS_STRIDE + VERTEX_TEXCOORDS_STRIDE;

    private static final String TAG = "photoViewerRenderer";
    private float[] mModelMatrix = new float[16];
    private float[] mViewMatrix = new float[16];
    private float[] mProjectionMatrix = new float[16];

    private renderTarget mLefEye;
    private renderTarget mRightEye;

    public int mWidthSurface = 0;
    public int mHightSurface = 0;
    private final Context mActivityContext;

    public int mSphereProgramHandle = 0;


    public int mVsProjHandle = 0;
    public int mVsViewHandle = 0;
    public int mVsModelHandle = 0;
    public int mFsTextureHandle = 0;
    public int mFsColorHandle = 0;
    public int mFsOpacityHandle = 0;
    private Activity m_ParentActivity;

    public svrQuaternion rotationQuat;
}




