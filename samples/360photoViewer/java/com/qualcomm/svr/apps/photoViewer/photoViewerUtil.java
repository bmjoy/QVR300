//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
package com.qualcomm.svr.apps.photoViewer;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.opengl.GLES30;
import android.util.Log;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import android.opengl.GLUtils;


public class photoViewerUtil {

    private static final String TAG = "photoViewerUtil";

    public static int shaderCompile(final int shaderType, final String shaderSource) {
        int shaderHandle = GLES30.glCreateShader(shaderType);

        if (shaderHandle != 0) {
            // Pass in the shader source.
            GLES30.glShaderSource(shaderHandle, shaderSource);

            // Compile the shader.
            GLES30.glCompileShader(shaderHandle);

            // Get the compilation status.
            final int[] compileStatus = new int[1];
            GLES30.glGetShaderiv(shaderHandle, GLES30.GL_COMPILE_STATUS, compileStatus, 0);

            // If the compilation failed, delete the shader.
            if (compileStatus[0] == 0) {
                Log.e(TAG, "Error compiling shader: " + GLES30.glGetShaderInfoLog(shaderHandle));
                GLES30.glDeleteShader(shaderHandle);
                shaderHandle = 0;
            }
        }

        if (shaderHandle == 0) {
            throw new RuntimeException("Error creating shader.");
        }

        return shaderHandle;
    }

    public static int shaderProgramCreateAndLink(final int vertexShaderHandle, final int fragmentShaderHandle, final String[] attributes) {
        int programHandle = GLES30.glCreateProgram();

        if (programHandle != 0) {
            // Bind the vertex shader to the program.
            GLES30.glAttachShader(programHandle, vertexShaderHandle);

            // Bind the fragment shader to the program.
            GLES30.glAttachShader(programHandle, fragmentShaderHandle);

            // Bind attributes
            if (attributes != null) {
                final int size = attributes.length;
                for (int i = 0; i < size; i++) {
                    GLES30.glBindAttribLocation(programHandle, i, attributes[i]);
                }
            }

            // Link the two shaders together into a program.
            GLES30.glLinkProgram(programHandle);

            // Get the link status.
            final int[] linkStatus = new int[1];
            GLES30.glGetProgramiv(programHandle, GLES30.GL_LINK_STATUS, linkStatus, 0);

            // If the link failed, delete the program.
            if (linkStatus[0] == 0) {
                Log.e(TAG, "Error compiling program: " + GLES30.glGetProgramInfoLog(programHandle));
                GLES30.glDeleteProgram(programHandle);
                programHandle = 0;
            }
        }

        if (programHandle == 0) {
            throw new RuntimeException("Error creating program.");
        }

        return programHandle;
    }

    public static String textureFromRawResource(final Context context,
                                                final int resourceId) {
        final InputStream inputStream = context.getResources().openRawResource(
                resourceId);
        final InputStreamReader inputStreamReader = new InputStreamReader(
                inputStream);
        final BufferedReader bufferedReader = new BufferedReader(
                inputStreamReader);

        String nextLine;
        final StringBuilder stringBuilder = new StringBuilder();

        try {
            while ((nextLine = bufferedReader.readLine()) != null) {
                stringBuilder.append(nextLine);
                stringBuilder.append('\n');
            }
        } catch (IOException e) {
            return null;
        }

        return stringBuilder.toString();
    }

    public static int textureLoad(final Context context, final int resourceId) {
        final int[] localTextureHandle = new int[1];

        GLES30.glGenTextures(1, localTextureHandle, 0);

        if (localTextureHandle[0] != 0) {
            final BitmapFactory.Options options = new BitmapFactory.Options();
            options.inScaled = false;

            final Bitmap bitmap = BitmapFactory.decodeResource(context.getResources(), resourceId, options);

            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, localTextureHandle[0]);

            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_NEAREST);
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_NEAREST);

            GLUtils.texImage2D(GLES30.GL_TEXTURE_2D, 0, bitmap, 0);

            bitmap.recycle();
        }

        if (localTextureHandle[0] == 0) {
            throw new RuntimeException("error texture loading");
        }

        return localTextureHandle[0];
    }
}
