//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
package com.qualcomm.svr.apps.photoViewer;

import android.opengl.GLES30;

public class renderTarget {
    private static final String TAG = "renderTarget";

    public int mWidth = 0;
    public int mHeight = 0;
    public static int mColorAttachmentId = 0;
    public static int mDepthAttachmentId = 0;
    public static int mFramebufferId = 0;


    public renderTarget(int width, int height, int sizedFormat, int Format, int type) {
        this.mWidth = width;
        this.mHeight = height;

        // Create the color attachment
        final int[] colorHandle = new int[1];
        GLES30.glGenTextures(1, colorHandle, 0);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, colorHandle[0]);
        GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, sizedFormat, width, height, 0, Format, type, null);

        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, 0);

        mColorAttachmentId = colorHandle[0];

        // Create the depth attachment
        final int[] depthHandle = new int[1];
        GLES30.glGenTextures(1, depthHandle, 0);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, depthHandle[0]);
        GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_DEPTH_COMPONENT, width, height, 0, GLES30.GL_DEPTH_COMPONENT, GLES30.GL_UNSIGNED_INT, null);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, 0);
        mDepthAttachmentId = depthHandle[0];

        // Create the frame buffer
        final int[] fboHandle = new int[1];
        GLES30.glGenFramebuffers(1, fboHandle, 0);
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, fboHandle[0]);
        GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER, GLES30.GL_COLOR_ATTACHMENT0, GLES30.GL_TEXTURE_2D, mColorAttachmentId, 0);
        GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER, GLES30.GL_DEPTH_ATTACHMENT, GLES30.GL_TEXTURE_2D, mDepthAttachmentId, 0);
        mFramebufferId = fboHandle[0];

        // Unbind from the framebuffer before returning
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);

    }

    public void Destroy() {
        if (mFramebufferId != 0) {
            final int[] fboHandle = new int[1];
            fboHandle[0] = mFramebufferId;
            GLES30.glDeleteFramebuffers(1, fboHandle, 0);
        }

        if (mColorAttachmentId != 0) {
            final int[] colorAttHandle = new int[1];
            colorAttHandle[0] = mColorAttachmentId;
            GLES30.glDeleteTextures(1, colorAttHandle, 0);
        }

        if (mDepthAttachmentId != 0) {
            final int[] depthAttHandle = new int[1];
            depthAttHandle[0] = mDepthAttachmentId;
            GLES30.glDeleteTextures(1, depthAttHandle, 0);
        }

        mFramebufferId = 0;
        mColorAttachmentId = 0;
        mDepthAttachmentId = 0;
    }

    public void Bind() {
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, mFramebufferId);
    }

    public void Unbind() {
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);
    }

    public int GetColorAttachment() {
        return mColorAttachmentId;
    }

    public int GetDepthAttachment() {
        return mDepthAttachmentId;
    }

    public int GetFrameBufferId() {
        return mFramebufferId;
    }

    public int GetWidth() {
        return mWidth;
    }

    public int GetHeight() {
        return mHeight;
    }

}
