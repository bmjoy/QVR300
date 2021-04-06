package com.serenegiant.display;

import android.opengl.GLSurfaceView;
import android.opengl.GLU;

import com.serenegiant.helper.Native;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;


public class MyGLRender implements GLSurfaceView.Renderer {
    private int mSampleType;

    private static final String TAG = "MyGLRender";

    private int mWidth;
    private int mHeight;

    private ArrayList<FloatBuffer> mVertices = new ArrayList<>();

    private final float xx = -0.4f;
    private final float zz = -0.2f;
    private final float zz2 = -1.0f;

//    private final float xx = -0.3f;
//    private final float zz = -0.3f;
//    private final float zz2 = -0.9f;

    float[] verticesFront = { xx, xx, zz,   xx, xx, zz2,   -xx, xx, zz2,  -xx, xx, zz };
    float[] verticesBack = { xx, -xx, zz,   xx, -xx, zz2,  -xx, -xx, zz2, -xx, -xx, zz };
    float[] verticesTop = { xx, xx, zz,      xx, -xx, zz,   -xx, -xx, zz,  -xx, xx, zz };
    float[] verticesBottom = { xx, xx, zz2,  xx, -xx, zz2,  -xx, -xx, zz2, -xx, xx, zz2 };
    float[] verticesLeft = { -xx, xx, zz,  -xx, xx, zz2,  -xx, -xx, zz2,  -xx, -xx, zz };
    float[] verticesRight = { xx, xx, zz,   xx, xx, zz2,   xx, -xx, zz2,  xx, -xx, zz };

    int pointCount = verticesFront.length / 3;

    float[] poseArray = new float[16];



    MyGLRender() {
        mVertices.add(getFloatBuffer(verticesFront));
        mVertices.add(getFloatBuffer(verticesBack));
        mVertices.add(getFloatBuffer(verticesTop));
        mVertices.add(getFloatBuffer(verticesBottom));
        mVertices.add(getFloatBuffer(verticesLeft));
        mVertices.add(getFloatBuffer(verticesRight));
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.glShadeModel(GL10.GL_SMOOTH);
        gl.glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
//        gl.glLineWidth(5.f);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        mWidth = width;
        mHeight = height;
    }

    private static final float hFov = 23.0f;
    private static final float eyeZ = -0.045f;
    private static final float zNear = 0.3f;
    private static final float zFar = 6.0f;
    private static final float base_line = 0.063f;
    private static final float aspect = (float) 1280.0 / (float) 720.0;

    @Override
    public void onDrawFrame(GL10 gl) {
        Native.getPose(poseArray);

        if (poseArray[12] == 0 && poseArray[13] == 0 && poseArray[14] == 0) {
            return;
        }

        gl.glClear(GL10.GL_COLOR_BUFFER_BIT | GL10.GL_DEPTH_BUFFER_BIT);

        gl.glViewport(0, 0, 1920, 1080);
        gl.glMatrixMode(GL10.GL_PROJECTION);
        gl.glLoadIdentity();
        GLU.gluPerspective(gl, hFov, aspect , zNear, zFar);
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glLoadIdentity();
        GLU.gluLookAt(gl, 0.026f, 0.014f, eyeZ, 0.026f, 0.014f, 5.0f, 0.0f, -1.0f, 0.0f );
        FloatBuffer floatBuffer = getFloatBuffer(poseArray);
        gl.glMultMatrixf(floatBuffer);
        drawCube(gl);

        gl.glViewport(1920, 0, 1920, 1080);
        gl.glMatrixMode(GL10.GL_PROJECTION);
        gl.glLoadIdentity();
        GLU.gluPerspective(gl, hFov, aspect , zNear, zFar);
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glLoadIdentity();
        GLU.gluLookAt(gl, 0.026f+ base_line, 0.014f, eyeZ, 0.026f+ base_line, 0.014f, 5.0f, 0.0f, -1.0f, 0.0f );
        gl.glMultMatrixf(floatBuffer);
        drawCube(gl);


        gl.glFlush();
    }

    private void drawCube(GL10 gl) {
        gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);

        for (FloatBuffer buffer : mVertices) {

            gl.glVertexPointer(3, GL10.GL_FLOAT, 0, buffer);
            gl.glDrawArrays(GL10.GL_LINE_LOOP, 0, pointCount);
        }
        gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
    }

    public void init() {
        Native.renderInit();
    }

    public void unInit() {
        Native.renderUnInit();
    }


    public void updateTransformMatrix(float rotateX, float rotateY, float scaleX, float scaleY) {
        Native.renderUpdateTransformMatrix(rotateX, rotateY, scaleX, scaleY);
    }

    private static FloatBuffer getFloatBuffer(float[] array) {
        //初始化字节缓冲区的大小=数组长度*数组元素大小。float类型的元素大小为Float.SIZE，
        //int类型的元素大小为Integer.SIZE，double类型的元素大小为Double.SIZE。
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(array.length * Float.SIZE);
        //以本机字节顺序来修改字节缓冲区的字节顺序
        //OpenGL在底层的实现是C语言，与Java默认的数据存储字节顺序可能不同，即大端小端问题。
        //因此，为了保险起见，在将数据传递给OpenGL之前，需要指明使用本机的存储顺序
        byteBuffer.order(ByteOrder.nativeOrder());
        //根据设置好的参数构造浮点缓冲区
        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        //把数组数据写入缓冲区
        floatBuffer.put(array);
        //设置浮点缓冲区的初始位置
        floatBuffer.position(0);


        return floatBuffer;
    }

}
