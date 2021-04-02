//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
package com.qualcomm.svr.apps.photoViewer;

import android.app.Activity;
import android.content.Context;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.opengl.EGL14;
import android.opengl.EGLExt;
import android.opengl.GLDebugHelper;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.qualcomm.svrapi.SvrApi;
import com.qualcomm.svrapi.SvrApi.svrBeginParams;
import com.qualcomm.svrapi.SvrApi.svrEyeMask;
import com.qualcomm.svrapi.SvrApi.svrFrameParams;
import com.qualcomm.svrapi.SvrApi.svrHeadPoseState;
import com.qualcomm.svrapi.SvrApi.svrLayoutCoords;
import com.qualcomm.svrapi.SvrApi.svrPerfLevel;
import com.qualcomm.svrapi.SvrApi.svrTextureType;
import com.qualcomm.svrapi.SvrApi.svrTrackingMode;
import com.qualcomm.svrapi.SvrApi.svrWarpType;
import com.qualcomm.svrapi.SvrApi.svrDeviceInfo;

import java.io.Writer;
import java.lang.ref.WeakReference;
import java.util.ArrayList;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGL11;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL;
import javax.microedition.khronos.opengles.GL10;


/**
 * An implementation of SurfaceView for Snapdragon Virtual Reality Java app rendering.
 * <p>
 * svrView provides the following features:
 * <p>
 * <ul>
 * <li>manage Snapdragon VR Java app EGL Context.
 * <li>customized  VR Renderer object
 * <li>Renders on a separate thread to decouple  from the UI thread.
 * <li>Supports both on-demand and continuous rendering.
 * </ul>
 * <p>
 * <p>
 * <h3>Using svrView</h3>
 * <p>
 * Typically you use svrView by subclassing it and overriding one or more of the
 * View system input event methods. If your application does not need to override event
 * methods then svrView can be used as-is. For the most part
 * svrView behavior can be customized by calling "config" methods rather than by subclassing.
 * For example, unlike a regular View, drawing is delegated to a separate Renderer object which
 * is registered with the svrView
 * using the {@link #setSvrRenderer(svrRenderer)} call.
 * <p>
 * <h3>Initialize svrView</h3>
 * to initialize a svrView, you need to call {@link #setSvrRenderer(svrRenderer)}.
 * Meanwhile, you can customize svrView by calling one or
 * more of below methods before calling setSvrRenderer:
 * <ul>
 * <li>{@link #configDebugLogs(int)}
 * <li>{@link #configEGLAttribute(boolean)}
 * <li>{@link #configEGLAttribute(svrEGLConfigManager)}
 * <li>{@link #configEGLAttribute(int, int, int, int, int, int)}
 * <li>{@link #configGLManager(svrGLManager)}
 * </ul>
 * <p>
 * <h4>Config the android.view.Surface</h4>
 * svrView create a PixelFormat.RGB_888 format surface by default .
 * Call getHolder().setFormat(PixelFormat.TRANSLUCENT) If you need a translucent surface
 * <p>
 * <h4>Config EGLConfig Attribute List</h4>
 * The configurations can be  differ in how may channels of data are present,
 * how many bits are allocated to each channel. The first thing
 * svrView has to do is to config what exactly EGLConfig is
 * <p>
 * By default svrView chooses a EGLConfig that has an RGB_888 pixel format,
 * with at least a 16-bit depth buffer and no stencil.
 * <p>
 * If you need a different EGLConfig
 * just overriding the default behavior by calling one of the
 * configEGLAttribute methods. for example:
 * <pre class="prettyprint">
 * private class myEGLConfig implements svrView.svrEGLConfigManager {
 * .... config your attribute list here
 * }
 * then call configEGLAttribute(new myEGLConfig());
 * </pre>
 * <p>
 * <h4>Debug Log</h4>
 * Optionally modify the behavior of svrView by calling
 * one or more of the debugging methods {@link #configDebugLogs(int)},
 * and {@link #configGLManager}.
 * typically they are called before setSvrRenderer so that they take effect immediately.
 * <p>
 * <h4>Setting a Renderer</h4>
 * Finally, you must call {@link #setSvrRenderer} to register a {@link svrRenderer}.
 * The renderer is
 * responsible for doing the actual OpenGL rendering.
 * <p>
 * <h3>Rendering Mode</h3>
 * Once the renderer is set, you can control whether the renderer draws
 * continuously or on-demand by calling
 * {@link #configRenderMode}. The default is continuous rendering.
 * <p>
 * <h3>Activity Life-cycle</h3>
 * A svrView must be notified when the activity is paused and resumed. svrView clients
 * are required to call {@link #onPause()} when the activity pauses and
 * {@link #onResume()} when the activity resumes. These calls allow svrView to
 * pause and resume the rendering thread, and then also allow svrView to release and recreate
 * the OpenGL display.
 * <p>
 * <h3>Handling events</h3>
 * <p>
 * Typically subclass svrView and override the appropriate method to handle an event ,
 * just as we would do with any other View.
 * However, when handling the event, is possible that we need to communicate with the Renderer
 * object
 * that's running in the rendering thread, we can do this using any
 * standard Java cross-thread communication mechanism(wait()/notify()/notifyAll()).
 * More convenient way to communicate with your renderer is to call
 * {@link #svrQueueEvent(Runnable)}. For example:
 * <pre class="prettyprint">
 * class MySvrView extends svrView {
 * <p>
 * private MySvrRenderer mMyRenderer;
 * <p>
 * public void start() {
 * mMySvrRenderer = ...;
 * setSvrRenderer(mMySvrRenderer);
 * }
 * <p>
 * public boolean onKeyDown(int keyCode, KeyEvent event) {
 * if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
 * svrQueueEvent(new Runnable() {
 * // This method will be called on the rendering
 * // thread:
 * public void run() {
 * mMySvrRenderer.handleControlCenter();
 * }});
 * return true;
 * }
 * return super.onKeyDown(keyCode, event);
 * }
 * }
 * </pre>
 */
public class svrView extends SurfaceView implements SurfaceHolder.Callback {
    private final static String TAG = "svrView";
    private final static boolean SVR_ATTACH_DETACH_LOG = true;
    private final static boolean SVR_THREAD_LOG = true;
    private final static boolean SVR_PAUSE_RESUME_LOG = true;
    private final static boolean SVR_SURFACE_LOG = true;
    private final static boolean SVR_RENDERER_LOG = true;
    private final static boolean SVR_SUBMIT_FRAME_LOG = true;
    private final static boolean SVR_EGL_LOG = true;
    //public static SvrApi svrApiRenderer;
    private static Activity m_ParentActivity;
    /**
     * Renders when the surface created, or when {@link #applyRender} is called.
     * Good example: http://mewgen.com/webgl/s/render-mode.html
     *
     * @see #checkRenderMode()
     * @see #configRenderMode(int)
     * @see #applyRender()
     */
    public final static int SVR_ON_DEMAND_MODE = 0;
    /**
     * Render continuously.
     *
     * @see #checkRenderMode()
     * @see #configRenderMode(int)
     */
    public final static int SVR_CONTINUOUSLY_MODE = 1;

    /**
     * Log glError() after  GL call and throw an exception if glError indicates
     * that an error has occurred.
     *
     * @see #checkDebugLogs
     * @see #configDebugLogs
     */
    public final static int SVR_GL_ERROR_CHECK = 1;

    /**
     * Add GL calls' log  to the system with tag "svrView".
     *
     * @see #checkDebugLogs
     * @see #configDebugLogs
     */
    public final static int SVR_GL_CALL_LOG = 2;


    /**
     * Svr View constructor. call {@link #setSvrRenderer} to register a renderer.
     */
    public svrView(Context context, Activity activity) {
        super(context);
        Log.i(TAG, "svrView constructor");
        mContext = context;
        //svrApiRenderer = svrApi;
        m_ParentActivity = activity;
        init();
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (mSvrGLThread != null) {
                // in case svrGLThread  still be running if view was never
                // attached to a window.
                mSvrGLThread.applyExitThenWaiting();
            }
        } finally {
            super.finalize();
        }
    }

    private void init() {
        // get notified when the underlying surface is created and destroyed
        // initialize snapdragon vr here
        SurfaceHolder svrHolder = getHolder();
        svrHolder.addCallback(this);
        svrApiRenderer = new SvrApi();
        mSvrFrameParams = svrApiRenderer.new svrFrameParams();
        mBeginParams = svrApiRenderer.new svrBeginParams(svrHolder.getSurface());
        mHeadPoseState = svrApiRenderer.new svrHeadPoseState();
        layoutCoords = svrApiRenderer.new svrLayoutCoords();
        Log.i(TAG, svrApiRenderer.svrGetVersion());
        Log.i(TAG, svrApiRenderer.svrGetVrServiceVersion());
        Log.i(TAG, svrApiRenderer.svrGetVrClientVersion());
        svrApiRenderer.svrInitialize(m_ParentActivity);
    }

    /**
     * Config the mWrapGL. mWrapGL's
     * {@link svrGLManager#glManager(GL)} method being called
     * whenever surface is created. you may want to customize your GL like this:
     * <pre class="prettyprint">
     * mSvrView = new svrView(this);
     * mSvrView.configGLManager(new svrView.svrGLManager() {
     * public GL glManager(GL gl) {
     * return new myCustomGL(gl);
     * }});
     * <p>
     * class myCustomGL implements GL, GL10, GL10Ext, GL11, GL11Ext {
     * ......implement your own GL here
     * }
     * </pre>
     *
     * @param mWrapGL the new svrGLManager
     */
    public void configGLManager(svrGLManager mWrapGL) {
        mLocalGL = mWrapGL;
    }

    /**
     * Config the debug flags to a new value.
     * The debug value take effect whenever a surface is created.
     * The default value is zero.
     *
     * @param debugLog the new debug flags
     * @see #SVR_GL_ERROR_CHECK
     * @see #SVR_GL_CALL_LOG
     */
    public void configDebugLogs(int debugLog) {
        mDebugLog = debugLog;
    }

    /**
     * Get the current value of the debug setting.
     *
     * @return the value of the debug setting.
     */
    public int checkDebugLogs() {
        return mDebugLog;
    }

    /**
     * Whether the EGL context will be reserved when the svrView paused and
     * resumed.
     * <p>
     * If true, EGL context may be reserved when the svrView is paused.
     * Android device which capable of running VR apps would support more than one EGL
     * contexts. on the other hand, devices which only support a certain number of EGL contexts may
     * have to
     * release the  EGL context in order to allow multiple applications to access the GPU.
     * you may think it's a good idea to release the context and any associated resources when an
     * app is kicked into the background,
     * since you don't know how long it will be there.
     * Whether release or not, that depends and this function give us this flexibility.
     * <p>
     * If false, EGL context will be released when the svrView is paused,
     * and recreated when the svrView is resumed.
     * <p>
     * <p>
     * The default is false.
     *
     * @param reserveWhenPause reserve the EGL context when paused
     */
    public void reserveEGLContextWhenPause(boolean reserveWhenPause) {
        mReserveEglContextWhenPause = reserveWhenPause;
    }

    /**
     * @return true if the EGL context will be reserved when paused
     */
    public boolean getReserveEGLContextWhenPause() {
        return mReserveEglContextWhenPause;
    }

    /**
     * bind a renderer with this view and starts the GL thread
     * <p>This method should be called once and only once in the life-cycle of
     * a svrView.
     * <p>The following svrView methods can only be called <em>before</em>
     * setSvrRenderer being called:
     * <ul>
     * <li>{@link #configEGLAttribute(boolean)}
     * <li>{@link #configEGLAttribute(svrEGLConfigManager)}
     * <li>{@link #configEGLAttribute(int, int, int, int, int, int)}
     * </ul>
     * <p>
     * The following svrView methods can only be called <em>after</em>
     * setSvrRenderer is called:
     * <ul>
     * <li>{@link #checkRenderMode()}
     * <li>{@link #onPause()}
     * <li>{@link #onResume()}
     * <li>{@link #svrQueueEvent(Runnable)}
     * <li>{@link #applyRender()}
     * <li>{@link #configRenderMode(int)}
     * </ul>
     *
     * @param renderer the renderer to use to perform OpenGL drawing.
     */
    public void setSvrRenderer(svrRenderer renderer) {
        renderThreadRunningState();
        if (mConfigEGL == null) {
            mConfigEGL = new defaultConfigEGL(true);//defaultConfigEGL(true); ConfigChooser()
        }
        if (mEGLContextManager == null) {
            mEGLContextManager = new defaultEGLContextManager();
        }
        if (mEGLWindowSurfaceManager == null) {
            mEGLWindowSurfaceManager = new defaultWindowSurfaceManager();
        }
        mSvrRenderer = renderer;
        mSvrGLThread = new svrGLThread(mThisWeakRef);
        mSvrGLThread.start();
    }

    /**
     * implement this interface to config the svr Frame parameter
     */
    public interface configSvrFrameParams {
        public void setSvrFrameParams();

        public void updatePerFrameParams();
    }

    private class defaultConfigFrameParams implements configSvrFrameParams {
        public void setSvrFrameParams() {
            mSvrFrameParams.minVsyncs = 1;
            mSvrFrameParams.renderLayers[0].imageType = svrTextureType.kTypeTexture;
            mSvrFrameParams.renderLayers[0].eyeMask = svrEyeMask.kEyeMaskLeft;
            mSvrFrameParams.renderLayers[0].layerFlags = 0;
            mSvrFrameParams.renderLayers[1].imageType = svrTextureType.kTypeTexture;
            mSvrFrameParams.renderLayers[1].eyeMask = svrEyeMask.kEyeMaskRight;
            mSvrFrameParams.renderLayers[1].layerFlags = 0;
            mSvrFrameParams.headPoseState = mHeadPoseState;
            mSvrFrameParams.warpType = svrWarpType.kSimple;
            mSvrFrameParams.fieldOfView = 90.0f;
            mSvrFrameParams.renderLayers[0].imageCoords = layoutCoords;
            mSvrFrameParams.renderLayers[1].imageCoords = layoutCoords;
            svrApiRenderer.svrSetPerformanceLevels(svrPerfLevel.kPerfMaximum, svrPerfLevel.kPerfMaximum);
            svrDeviceInfo deviceInfo = svrApiRenderer.svrGetDeviceInfo();
            int supportedMode = svrApiRenderer.svrGetSupportedTrackingModes();
            int trackingMode = svrTrackingMode.kTrackingPosition.getTrackingMode();
            svrApiRenderer.svrSetTrackingMode(trackingMode);
            int trackedMode = svrApiRenderer.svrGetTrackingMode();
        }

        public void updatePerFrameParams() {
            mSvrFrameParams.frameIndex++;
            predictedTimeMs = svrApiRenderer.svrGetPredictedDisplayTime();
            mHeadPoseState = svrApiRenderer.svrGetPredictedHeadPose(predictedTimeMs);
            mSvrFrameParams.headPoseState = mHeadPoseState;
        }
    }

    public void setFrameParams() {
        if (mConfigSvrFrameParams == null) {
            mConfigSvrFrameParams = new defaultConfigFrameParams();
        }
    }

    /**
     * set the svrEGLContextManager.
     * <p>If this method is
     * called, it must be called before {@link #setSvrRenderer(svrRenderer)}
     * is called.
     * <p>
     * If this method is not called, then by default
     * a context will be created with no shared context and
     * with a null attribute list.
     */
    public void setEGLContextManager(svrEGLContextManager factory) {
        renderThreadRunningState();
        mEGLContextManager = factory;
    }

    /**
     * set svrEGLWindowSurfaceManager.
     * <p>If this method is
     * called, it must be called before {@link #setSvrRenderer(svrRenderer)}
     * is called.
     * <p>
     * If this method is not called, then by default
     * a window surface will be created with a null attribute list.
     */
    public void setEGLWindowSurfaceManager(svrEGLWindowSurfaceManager factory) {
        renderThreadRunningState();
        mEGLWindowSurfaceManager = factory;
    }

    public void setLayoutCoords(float[] lowerLeftPos, float[] lowerRightPos, float[] upperLeftPos,
                                float[] upperRightPos, float[] lowerUVs, float[] upperUVs, float[] transformMatrix) {
        layoutCoords.LowerLeftPos = lowerLeftPos; // {-1,-1,0,1}
        layoutCoords.LowerRightPos = lowerRightPos; // {1,-1,0,1}
        layoutCoords.UpperLeftPos = upperLeftPos; // {-1,1,0,1}
        layoutCoords.UpperRightPos = upperRightPos; // {1,1,0,1}
        layoutCoords.LowerUVs = lowerUVs;
        layoutCoords.UpperUVs = upperUVs;
        layoutCoords.TransformMatrix = transformMatrix;
    }

    /**
     * config svrEGLConfigManager.
     * <p>If this method is
     * called, it must be  before {@link #setSvrRenderer(svrRenderer)}
     * is called.
     * <p>
     * without configEGLAttribute method is called,  by default the
     * svrView will choose an EGLConfig which is compatible with the current
     * android.view.Surface, with a depth buffer depth of
     * at least 16 bits.
     * {@link #setSvrRenderer(svrRenderer)}
     */
    public void configEGLAttribute(svrEGLConfigManager configAttribute) {
        renderThreadRunningState();
        mConfigEGL = configAttribute;
    }

    /**
     * set a config chooser with depth info
     * <p>If this method is
     * called, it must be called before {@link #setSvrRenderer(svrRenderer)}
     * is called.
     * <p>
     * If no configEGLAttribute method is called, then by default the
     * view will choose an RGB_888 surface with a depth buffer depth of
     * at least 16 bits.
     */
    public void configEGLAttribute(boolean depthInfo) {
        configEGLAttribute(new defaultConfigEGL(depthInfo));
    }

    /**
     * set a config with each component size specified.
     * <p>If this method is
     * called, it must be called before {@link #setSvrRenderer(svrRenderer)}
     * is called.
     * <p>
     * If no configEGLAttribute method set, by default the
     * view will choose an RGB_888 surface with a depth buffer depth of
     * at least 16 bits.
     */
    public void configEGLAttribute(int rSize, int gSize, int bSize,
                                   int aSize, int dSize, int stSize) {
        configEGLAttribute(new setEGLComponentSize(rSize, gSize,
                bSize, aSize, dSize, stSize));
    }

    /**
     * Tell the default svrEGLContextManager and default svrEGLConfigManager
     * which EGLContext client version to pick.
     * <p>Use this method to create an OpenGL ES 3.0 compatible context.
     * for Example:
     * <pre class="prettyprint">
     * public MySvrView(Context context) {
     * super(context);
     * setEGLContextClientVersion(3); // Pick an OpenGL ES 3.0 context.
     * setSvrRenderer(new MySvrRenderer());
     * }
     * </pre>
     * <p>Note: Activities require OpenGL ES 3.0 should set this by
     * setting @lt;uses-feature android:glEsVersion="0x00030000" /> in the activity's
     * AndroidManifest.xml file.
     * <p>This method must be called before {@link #setSvrRenderer(svrRenderer)}
     * is called if you want it
     * <p>This method only affects the behavior of the default EGLContexFactory and the
     * default svrEGLConfigManager. If
     * {@link #setEGLContextManager(svrEGLContextManager)}  called, the customized
     * svrEGLContextManager is responsible for creating an OpenGL ES 3.0 compatible context.
     * If
     * {@link #configEGLAttribute(svrEGLConfigManager)} called, the customized
     * svrEGLConfigManager is responsible for choosing an OpenGL ES 3.0 compatible config.
     *
     * @param version The EGLContext client version to choose. Use 3 for OpenGL ES 3.0
     */
    public void setEGLContextClientVersion(int version) {
        renderThreadRunningState();
        mEGLContextClientVersion = version;
    }

    /**
     * Set the rendering mode. When renderMode is
     * SVR_CONTINUOUSLY_MODE, the renderer will be called
     * repeatedly to render the scene. When renderMode
     * is SVR_ON_DEMAND_MODE, the renderer only rendered when the surface
     * is created, or when {@link #applyRender} is called. Defaults to SVR_CONTINUOUSLY_MODE.
     * <p>
     * Using SVR_ON_DEMAND_MODE can improve battery life and  system performance
     * by giving the GPU and CPU the chance to idle when the view does not need to be redraw.
     * <p>
     * This method can only be called after {@link #setSvrRenderer(svrRenderer)}
     *
     * @param renderMode one of the SVR_X_MODE constants
     * @see #SVR_CONTINUOUSLY_MODE
     * @see #SVR_ON_DEMAND_MODE
     */
    public void configRenderMode(int renderMode) {
        mSvrGLThread.configRenderMode(renderMode);
    }

    /**
     * check the current rendering mode. May be called
     * from any thread. Don't call this before setting a renderer.
     *
     * @return the current rendering mode.
     * @see #SVR_CONTINUOUSLY_MODE
     * @see #SVR_ON_DEMAND_MODE
     */
    public int checkRenderMode() {
        return mSvrGLThread.checkRenderMode();
    }

    /**
     * Apply the renderer to render a frame.
     * Usually this being called when the render mode set to
     * {@link #SVR_ON_DEMAND_MODE}, so  frames are only rendered on demand.
     * May be called
     * from any thread. Don't call this before setting a renderer.
     */
    public void applyRender() {
        mSvrGLThread.applyRender();
    }

    /**
     * SurfaceHolder.Callback interface,called immediately after the surface is first created.
     * normally this is not being called or subclassed by clients of svrView.
     */
    public void surfaceCreated(SurfaceHolder holder) {
        mSvrGLThread.surfaceCreated();
    }

    /**
     * SurfaceHolder.Callback interface, called immediately before a surface is being destroyed
     * normally this is not being called or subclassed by clients of svrView.
     */
    public void surfaceDestroyed(SurfaceHolder holder) {
        // Surface will be destroyed when we return
        mSvrGLThread.surfaceDestroyed();
    }

    /**
     * SurfaceHolder.Callback interface, and is
     * normally this is not being called or subclassed by clients of svrView.
     */
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        mSvrGLThread.onWindowSizeChanged(w, h);
    }

    /**
     * call svrEndVr() here
     * this method will pause the rendering thread.
     * Don't call this before setting a renderer.
     */
    public void onPause() {
        mSvrGLThread.onPause();
        svrApiRenderer.svrEndVr();
    }

    /**
     * This method will recreate the OpenGL display and resume the rendering thread.
     * only after OpenGL ready, we can call svrBeginVr then svrSubmitFrame
     * Don't call this before a renderer has been set.
     * Don't call svrBeginVr here as OpenGL not ready here
     */
    public void onResume() {
        mSvrGLThread.onResume();
    }

    /**
     * Queue a runnable to run on the rendering thread.
     * Used to talk to the Renderer.
     * Don't call this before setting a renderer
     *
     * @param r the runnable to  run on the rendering thread.
     */
    public void svrQueueEvent(Runnable r) {
        mSvrGLThread.svrQueueEvent(r);
    }

    /**
     * part of the View class and normally
     * not being called or subclassed by clients of svrView.
     */
    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (SVR_ATTACH_DETACH_LOG) {
            Log.d(TAG, "onAttachedToWindow reattach =" + mDetached);
        }
        if (mDetached && (mSvrRenderer != null)) {
            int renderMode = SVR_CONTINUOUSLY_MODE;
            if (mSvrGLThread != null) {
                renderMode = mSvrGLThread.checkRenderMode();
            }
            mSvrGLThread = new svrGLThread(mThisWeakRef);
            if (renderMode != SVR_CONTINUOUSLY_MODE) {
                mSvrGLThread.configRenderMode(renderMode);
            }
            mSvrGLThread.start();
        }
        mDetached = false;
    }

    /**
     * shutdown Snapdragon VR here
     * part of the View class and normally
     * not being called or subclassed by clients of svrView.
     */
    @Override
    protected void onDetachedFromWindow() {
        if (SVR_ATTACH_DETACH_LOG) {
            Log.d(TAG, "onDetachedFromWindow");
        }
        if (mSvrGLThread != null) {
            mSvrGLThread.applyExitThenWaiting();
        }
        mDetached = true;
        svrApiRenderer.svrShutdown();
        super.onDetachedFromWindow();
    }

    // ----------------------------------------------------------------------

    /**
     * An interface used to glManager a GL interface.
     * <p>you may want to add log or trace for GL methods, to achieve this,
     * having your own class which implement all the GL methods by delegating to another GL
     * instance.
     * Then you can add your own behavior(log/trace) before or after calling the
     * delegate. All the svrGLManager would do is to instantiate and return the
     * wrapper GL instance:
     * <pre class="prettyprint">
     * class MyGLDelegate implements svrGLManager {
     * GL glManager(GL gl) {
     * return new MyGLImpl(gl);
     * }
     * static class MyGLImpl implements GL,GL10,GL11,... {
     * ...
     * }
     * }
     * </pre>
     *
     * @see #configGLManager(svrGLManager)
     */
    public interface svrGLManager {
        /**
         * add a local gl interface in another gl interface.
         *
         * @param gl a GL interface which to be wrapped.
         * @return the input gl or another GL object which wrap the input gl.
         */
        GL glManager(GL gl);
    }

    /**
     * Renderer interface.
     * <p>
     * The renderer is responsible for making OpenGL calls to render a frame.
     * <p>
     * svrView clients  create their own classes that implement
     * this interface, and then call {@link svrView#setSvrRenderer} to
     * register the renderer with the svrView.
     * <p>
     * <h3>Threading</h3>
     * Renderer will be called on a different thread, not UI thread.
     * so Clients typically need to talk to the renderer from the UI thread  where
     * input events received. Clients can talk using any of the
     * standard Java techniques for cross-thread communication(wait()/notify()/notifyAll()), or they
     * can
     * use the {@link svrView#svrQueueEvent(Runnable)} for convenient.
     * <p>
     * <h3>EGL Context Lost</h3>
     * In some situations , the EGL rendering context will be lost. Waking up from sleep for
     * example.
     * In that case, all OpenGL resources (textures for example) that are
     * associated with the context will be automatically deleted. In order to
     * render correctly, a renderer has to recreate any lost resources
     * if they still needs. The {@link #onSvrBegin(GL10, EGLConfig)} method
     * is the place to do this.
     *
     * @see #setSvrRenderer(svrRenderer)
     */
    public interface svrRenderer {
        /**
         * call svrBeginVr() here
         * Called when the surface is created or recreated, typically called when
         * VR app launching and resume from pause, then going to begin vr rendering.
         * <p>
         * Call this when the rendering thread starts and whenever the EGL context is lost.
         * <p>
         * As this method being called at the beginning of rendering, as well as
         * when the EGL context is lost, this method is a good place to put
         * code to create resources, and anything else which need to be recreated if the EGL context
         * is lost.
         * For example, you might want to create textures here.
         * Specifically in this example, we initialize the FBO render target here,
         * to make sure GL calls run in the same thread
         * <p>
         * Note that all OpenGL resources associated with context will be automatically deleted if
         * EGL Context lost
         * No need to call the corresponding "glDelete" methods such as glDeleteTextures to
         * manually delete the lost resources.
         * <p>
         *
         * @param gl     the GL interface. Use <code>instanceof</code> to
         *               check if it supports GL11 or higher interfaces.
         * @param config the EGLConfig we used to create surface.
         */
        void onSvrBegin(GL10 gl, EGLConfig config);

        /**
         * Called when VR launched or resume.
         * align with onSurfaceChanged of GLSurfaceView
         * <p>
         * Typically called when VR app launching and resume from pause.
         * Usually we don't have rotation/resize when VR app in the foreground
         * The purpose of this interface in this example is to keep behavior same with GLSurfaceView
         * <p>
         * Typically we set viewport here. If your camera
         * is fixed then you could also set your projection matrix here:
         * <pre class="prettyprint">
         * void onSvrViewChanged(GL10 gl, int width, int height) {
         * GLES30.glViewport(0, 0, width, height);
         * // set the projection as well for fixed camera
         * float ratio = (float) width / height;
         * GLES30.glMatrixMode(GL10.GL_PROJECTION);
         * GLES30.glLoadIdentity();
         * GLES30.glFrustumf(-ratio, ratio, -2, 0, 0, 8);
         * or
         * GLES30.glViewport(0, 0, width, height);
         * float fovy = 90.0f;
         * float aspect = (float) mLefEye.mWidth / (float) mLefEye.mHeight;
         * float zNear = 0.1f;
         * float zFar = 25.0f;
         * Matrix.perspectiveM(mProjectionMatrix, 0, fovy, aspect, zNear, zFar);
         * }
         * </pre>
         *
         * @param gl the GL interface. Use <code>instanceof</code> to
         *           check if this supports GL11 or higher interfaces.
         */
        void onSvrViewChanged(GL10 gl, int width, int height);

        /**
         * Submit the VR frame.
         * <p>
         * This method  drawing the current VR frame.
         * <p>
         * The implementation of this method typically looks like this:
         * <pre class="prettyprint">
         * void onSvrSubmitFrame(GL10 gl) {
         * GLES30.glClear(GLES30.GL_DEPTH_BUFFER_BIT | GLES30.GL_COLOR_BUFFER_BIT);
         * //... other gl calls to render the scene ...
         * svrSubmitFrame();
         * }
         * </pre>
         *
         * @param gl the GL interface. Use <code>instanceof</code>  to
         *           check if this supports GL11 or higher interfaces.
         */
        void onSvrSubmitFrame(GL10 gl);
    }

    /**
     * An interface for customizing the eglCreateContext and eglDestroyContext calls.
     * <p>
     * Implement this if clients want to customize this
     * {@link svrView#setEGLContextManager(svrEGLContextManager)}
     */
    public interface svrEGLContextManager {
        EGLContext svrCreateEGLContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig);

        void svrDestroyEGLContext(EGL10 egl, EGLDisplay display, EGLContext context);
    }

    private class defaultEGLContextManager implements svrEGLContextManager {
        //Must be followed by an integer that determines which version of an OpenGL ES context to create
        private int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

        public EGLContext svrCreateEGLContext(EGL10 egl, EGLDisplay display, EGLConfig config) {
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, mEGLContextClientVersion,
                    EGL10.EGL_NONE};

            return egl.eglCreateContext(display, config, EGL10.EGL_NO_CONTEXT,
                    mEGLContextClientVersion != 0 ? attrib_list : null);
        }

        public void svrDestroyEGLContext(EGL10 egl, EGLDisplay display,
                                         EGLContext context) {
            if (!egl.eglDestroyContext(display, context)) {
                Log.e("EGLContextManager", "display:" + display + " context: " + context);
                if (SVR_THREAD_LOG) {
                    Log.i("EGLContextManager", "tid=" + Thread.currentThread().getId());
                }
                EGLManager.svrEGLException("eglDestroyContex", egl.eglGetError());
            }
        }
    }

    /**
     * An interface for customizing the eglCreateWindowSurface and eglDestroySurface calls.
     * <p>
     * Implement this if clients want to customize this
     * {@link svrView#setEGLWindowSurfaceManager(svrEGLWindowSurfaceManager)}
     */
    public interface svrEGLWindowSurfaceManager {
        /**
         * @return null if the surface cannot be constructed.
         */
        EGLSurface svrEGLWindowSurfaceCreate(EGL10 egl, EGLDisplay display, EGLConfig config,
                                             Object nativeWindow);

        void svrEGLWindowSurfaceDestroy(EGL10 egl, EGLDisplay display, EGLSurface surface);
    }

    private static class defaultWindowSurfaceManager implements svrEGLWindowSurfaceManager {

        public EGLSurface svrEGLWindowSurfaceCreate(EGL10 egl, EGLDisplay display,
                                                    EGLConfig config, Object nativeWindow) {
            EGLSurface result = null;
            try {
                result = egl.eglCreateWindowSurface(display, config, nativeWindow, null);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "eglCreateWindowSurface", e);
            }
            return result;
        }

        public void svrEGLWindowSurfaceDestroy(EGL10 egl, EGLDisplay display,
                                               EGLSurface surface) {
            egl.eglDestroySurface(display, surface);
        }
    }

    /**
     * An interface for configuring an EGLConfig configuration
     * <p>
     * Implement this if clients want to customize this
     * {@link svrView#configEGLAttribute(svrEGLConfigManager)} for example
     * <p>
     * <pre class="prettyprint">
     * configEGLAttribute(new myConfiguEGLAtt());
     * private class myConfiguEGLAtt implements svrEGLConfigManager {
     * public EGLConfig ConfigEGLAttr(EGL10 egl, EGLDisplay display) {
     * int[] attributeList = {
     * EGL10.EGL_RED_SIZE, 8,
     * EGL10.EGL_GREEN_SIZE, 8,
     * EGL10.EGL_BLUE_SIZE, 8,
     * EGL10.EGL_ALPHA_SIZE, 8,
     * EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,//EGL10.EGL_PBUFFER_BIT,
     * EGL10.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,//EGL_OPENGL_ES2_BIT,
     * EGL10.EGL_NONE
     * };
     * EGLConfig[] configs = new EGLConfig[1];
     * if (egl.eglChooseConfig(display, attributeList, configs, 1,
     * new int[] { 1 })) {
     * return configs[0];
     * } else {
     * throw new IllegalStateException(
     * "Could not get EGL config...");
     * }
     * }
     * }
     * </pre>
     */
    public interface svrEGLConfigManager {
        /**
         * Choose a configuration from the list.
         *
         * @param egl     the EGL10 for the current display.
         * @param display the current EGL display.
         * @return the chosen configuration.
         */
        EGLConfig ConfigEGLAttr(EGL10 egl, EGLDisplay display);
    }

    private abstract class defaultConfigEGLAttrImpl
            implements svrEGLConfigManager {
        public defaultConfigEGLAttrImpl(int[] configValue) {
            mConfigValue = parseConfigSetting(configValue);
        }

        public EGLConfig ConfigEGLAttr(EGL10 egl, EGLDisplay display) {
            int[] configAtt = new int[1];
            if (!egl.eglChooseConfig(display, mConfigValue, null, 0,
                    configAtt)) {
                throw new IllegalArgumentException("eglChooseConfig failed");
            }

            int mConfig = configAtt[0];

            if (mConfig <= 0) {
                throw new IllegalArgumentException(
                        "No configs match configValue");
            }

            EGLConfig[] theConfig = new EGLConfig[mConfig];
            if (!egl.eglChooseConfig(display, mConfigValue, theConfig, mConfig,
                    configAtt)) {
                throw new IllegalArgumentException("eglChooseConfig#2 failed");
            }
            EGLConfig config = ConfigEGLAttr(egl, display, theConfig);
            if (config == null) {
                throw new IllegalArgumentException("No config chosen");
            }
            return config;
        }

        abstract EGLConfig ConfigEGLAttr(EGL10 egl, EGLDisplay display,
                                         EGLConfig[] configs);

        protected int[] mConfigValue;

        private int[] parseConfigSetting(int[] configValue) {
            if (mEGLContextClientVersion != 2 && mEGLContextClientVersion != 3) {
                return configValue;
            }
            int len = configValue.length;
            int[] newConfigValue = new int[len + 2];
            System.arraycopy(configValue, 0, newConfigValue, 0, len - 1);
            newConfigValue[len - 1] = EGL10.EGL_RENDERABLE_TYPE;
            if (mEGLContextClientVersion == 2) {
                newConfigValue[len] = EGL14.EGL_OPENGL_ES2_BIT;  /* EGL_OPENGL_ES2_BIT */
            } else {
                newConfigValue[len] = EGLExt.EGL_OPENGL_ES3_BIT_KHR; /* EGL_OPENGL_ES3_BIT_KHR */
            }
            newConfigValue[len + 1] = EGL10.EGL_NONE;
            return newConfigValue;
        }
    }

    /**
     * Choose a configuration with exactly the specified r,g,b,a sizes,
     * and at least the specified depth and stencil sizes.
     */
    private class setEGLComponentSize extends defaultConfigEGLAttrImpl {
        public setEGLComponentSize(int rSize, int gSize, int bSize,
                                   int aSize, int dSize, int stSize) {
            super(new int[]{
                    EGL10.EGL_RED_SIZE, rSize,
                    EGL10.EGL_GREEN_SIZE, gSize,
                    EGL10.EGL_BLUE_SIZE, bSize,
                    EGL10.EGL_ALPHA_SIZE, aSize,
                    EGL10.EGL_DEPTH_SIZE, dSize,
                    EGL10.EGL_STENCIL_SIZE, stSize,
                    EGL10.EGL_NONE});
            mValue = new int[1];
            mRSize = rSize;
            mGSize = gSize;
            mBSize = bSize;
            mASize = aSize;
            mDSize = dSize;
            mStSize = stSize;
        }

        @Override
        public EGLConfig ConfigEGLAttr(EGL10 egl, EGLDisplay display,
                                       EGLConfig[] configs) {
            for (EGLConfig config : configs) {
                int d = getEGLConfigAttr(egl, display, config,
                        EGL10.EGL_DEPTH_SIZE, 0);
                int s = getEGLConfigAttr(egl, display, config,
                        EGL10.EGL_STENCIL_SIZE, 0);
                if ((d >= mDSize) && (s >= mStSize)) {
                    int r = getEGLConfigAttr(egl, display, config,
                            EGL10.EGL_RED_SIZE, 0);
                    int g = getEGLConfigAttr(egl, display, config,
                            EGL10.EGL_GREEN_SIZE, 0);
                    int b = getEGLConfigAttr(egl, display, config,
                            EGL10.EGL_BLUE_SIZE, 0);
                    int a = getEGLConfigAttr(egl, display, config,
                            EGL10.EGL_ALPHA_SIZE, 0);
                    if ((r == mRSize) && (g == mGSize)
                            && (b == mBSize) && (a == mASize)) {
                        return config;
                    }
                }
            }
            return null;
        }

        private int getEGLConfigAttr(EGL10 egl, EGLDisplay display,
                                     EGLConfig config, int attribute, int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        private int[] mValue;
        // Subclasses can adjust these values:
        protected int mRSize;
        protected int mGSize;
        protected int mBSize;
        protected int mASize;
        protected int mDSize;
        protected int mStSize;
    }

    /**
     * Choose a RGB_888 surface and probably with a depth buffer.
     */
    private class defaultConfigEGL extends setEGLComponentSize {
        public defaultConfigEGL(boolean enableDepthBuffer) {
            super(8, 8, 8, 0, enableDepthBuffer ? 16 : 0, 0);
        }
    }

    /**
     * EGL manage class.
     */

    private static class EGLManager {
        public EGLManager(WeakReference<svrView> svrViewManagerWeakRef) {
            mSvrViewManagerWeakRef = svrViewManagerWeakRef;
        }

        /**
         * Initialize EGL with config attribute.
         * //* @param configValue
         */
        public void start() {
            if (SVR_EGL_LOG) {
                Log.w("EGLManager", "start() tid=" + Thread.currentThread().getId());
            }
            /*
             * Get the EGL instance
             */
            mSvrEgl = (EGL10) EGLContext.getEGL();

            /*
             * Get The EGL display.
             */
            mSvrDisplayEgl = mSvrEgl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

            if (mSvrDisplayEgl == EGL10.EGL_NO_DISPLAY) {
                throw new RuntimeException("eglGetDisplay failed");
            }

            /*
             * now initialize EGL for  display
             */
            int[] version = new int[2];
            if (!mSvrEgl.eglInitialize(mSvrDisplayEgl, version)) {
                throw new RuntimeException("eglInitialize failed");
            }
            svrView view = mSvrViewManagerWeakRef.get();
            if (view == null) {
                mSvrConfigEgl = null;
                mSvrContextEgl = null;
            } else {
                mSvrConfigEgl = view.mConfigEGL.ConfigEGLAttr(mSvrEgl, mSvrDisplayEgl);

                /*
                * EGL context is a heavy object and create new object is the last thing we want to do.
                */
                mSvrContextEgl = view.mEGLContextManager.svrCreateEGLContext(mSvrEgl, mSvrDisplayEgl, mSvrConfigEgl);
            }
            if (mSvrContextEgl == null || mSvrContextEgl == EGL10.EGL_NO_CONTEXT) {
                mSvrContextEgl = null;
                svrEGLException("svrCreateEGLContext");
            }
            if (SVR_EGL_LOG) {
                Log.w("EGLManager", "svrCreateEGLContext " + mSvrContextEgl + " tid=" + Thread.currentThread().getId());
            }

            mSvrSurfaceEgl = null;
        }

        /**
         * Create an egl surface
         *
         * @return true if  successfully.
         */
        public boolean createSvrSurface() {
            if (SVR_EGL_LOG) {
                Log.w("EGLManager", "createSvrSurface()  tid=" + Thread.currentThread().getId());
            }
            /*
             * make sure we are ready here.
             */
            if (mSvrEgl == null) {
                throw new RuntimeException("egl not initialized");
            }
            if (mSvrDisplayEgl == null) {
                throw new RuntimeException("eglDisplay not initialized");
            }
            if (mSvrConfigEgl == null) {
                throw new RuntimeException("mSvrConfigEgl not initialized");
            }

            svrSurfaceDestroy();

            /*
             * svrEGLWindowSurfaceCreate.
             */
            svrView view = mSvrViewManagerWeakRef.get();
            if (view != null) {
                mSvrSurfaceEgl = view.mEGLWindowSurfaceManager.svrEGLWindowSurfaceCreate(mSvrEgl,
                        mSvrDisplayEgl, mSvrConfigEgl, view.getHolder());
            } else {
                mSvrSurfaceEgl = null;
            }

            if (mSvrSurfaceEgl == null || mSvrSurfaceEgl == EGL10.EGL_NO_SURFACE) {
                int error = mSvrEgl.eglGetError();
                if (error == EGL10.EGL_BAD_NATIVE_WINDOW) {
                    Log.e("EGLManager", "svrEGLWindowSurfaceCreate returned EGL_BAD_NATIVE_WINDOW.");
                }
                return false;
            }

            /*
             * make current before issuing GL commands
             */
            if (!mSvrEgl.eglMakeCurrent(mSvrDisplayEgl, mSvrSurfaceEgl, mSvrSurfaceEgl, mSvrContextEgl)) {
                /*
                 *  make the context current fail, maybe the underlying
                 *  surface has been destroyed.
                 */
                eglErrorToWarningLog("EGLHelper", "eglMakeCurrent", mSvrEgl.eglGetError());
                return false;
            }
            Log.i(TAG, "before swap EGL Surface is = " + mSvrSurfaceEgl + "EGL Context" + mSvrContextEgl);

            return true;
        }

        /**
         * GL object for the  EGL context.
         */
        GL createGL() {

            GL gl = mSvrContextEgl.getGL();
            svrView view = mSvrViewManagerWeakRef.get();
            if (view != null) {
                if (view.mLocalGL != null) {
                    gl = view.mLocalGL.glManager(gl);
                }

                if ((view.mDebugLog & (SVR_GL_ERROR_CHECK | SVR_GL_CALL_LOG)) != 0) {
                    int configFlags = 0;
                    Writer log = null;
                    if ((view.mDebugLog & SVR_GL_ERROR_CHECK) != 0) {
                        configFlags |= GLDebugHelper.CONFIG_CHECK_GL_ERROR;
                    }
                    if ((view.mDebugLog & SVR_GL_CALL_LOG) != 0) {
                        log = new writeSvrLog();
                    }
                    gl = GLDebugHelper.wrap(gl, configFlags, log);
                }
            }
            return gl;
        }

        /**
         * Swap the buffer.
         * for SVR, we swap this in the warp thread
         * so swap here just for debug purpose without svr
         *
         * @return the error code from eglSwapBuffers.
         */
        public int swap() {
            if (!mSvrEgl.eglSwapBuffers(mSvrDisplayEgl, mSvrSurfaceEgl)) {
                return mSvrEgl.eglGetError();
            }
            return EGL10.EGL_SUCCESS;
        }

        public void svrEGLWindowSurfaceDestroy() {
            if (SVR_EGL_LOG) {
                Log.w("EGLManager", "svrEGLWindowSurfaceDestroy()  tid=" + Thread.currentThread().getId());
            }
            svrSurfaceDestroy();
        }

        private void svrSurfaceDestroy() {
            if (mSvrSurfaceEgl != null && mSvrSurfaceEgl != EGL10.EGL_NO_SURFACE) {
                mSvrEgl.eglMakeCurrent(mSvrDisplayEgl, EGL10.EGL_NO_SURFACE,
                        EGL10.EGL_NO_SURFACE,
                        EGL10.EGL_NO_CONTEXT);
                svrView view = mSvrViewManagerWeakRef.get();
                if (view != null) {
                    view.mEGLWindowSurfaceManager.svrEGLWindowSurfaceDestroy(mSvrEgl, mSvrDisplayEgl, mSvrSurfaceEgl);
                }
                mSvrSurfaceEgl = null;
            }
        }

        public void destroyAndTerminateEGL() {
            if (SVR_EGL_LOG) {
                Log.w("EGLManager", "destroyAndTerminateEGL() tid=" + Thread.currentThread().getId());
            }
            if (mSvrContextEgl != null) {
                svrView view = mSvrViewManagerWeakRef.get();
                if (view != null) {
                    view.mEGLContextManager.svrDestroyEGLContext(mSvrEgl, mSvrDisplayEgl, mSvrContextEgl);
                }
                mSvrContextEgl = null;
            }
            if (mSvrDisplayEgl != null) {
                mSvrEgl.eglTerminate(mSvrDisplayEgl);
                mSvrDisplayEgl = null;
            }
        }

        private void svrEGLException(String function) {
            svrEGLException(function, mSvrEgl.eglGetError());
        }

        public static void svrEGLException(String function, int error) {
            String message = formatEglError(function, error);
            if (SVR_THREAD_LOG) {
                Log.e("EGLManager", "svrEGLException tid=" + Thread.currentThread().getId() + " "
                        + message);
            }
            throw new RuntimeException(message);
        }

        public static void eglErrorToWarningLog(String tag, String function, int error) {
            Log.w(tag, formatEglError(function, error));
        }

        public static String formatEglError(String function, int error) {
            return function + " failed: " + getErrorString(error);
        }

        private static String getHex(int value) {
            return "0x" + Integer.toHexString(value);
        }

        public static String getErrorString(int error) {
            switch (error) {
                case EGL10.EGL_SUCCESS:
                    return "EGL_SUCCESS";
                case EGL10.EGL_NOT_INITIALIZED:
                    return "EGL_NOT_INITIALIZED";
                case EGL10.EGL_BAD_ACCESS:
                    return "EGL_BAD_ACCESS";
                case EGL10.EGL_BAD_ALLOC:
                    return "EGL_BAD_ALLOC";
                case EGL10.EGL_BAD_ATTRIBUTE:
                    return "EGL_BAD_ATTRIBUTE";
                case EGL10.EGL_BAD_CONFIG:
                    return "EGL_BAD_CONFIG";
                case EGL10.EGL_BAD_CONTEXT:
                    return "EGL_BAD_CONTEXT";
                case EGL10.EGL_BAD_CURRENT_SURFACE:
                    return "EGL_BAD_CURRENT_SURFACE";
                case EGL10.EGL_BAD_DISPLAY:
                    return "EGL_BAD_DISPLAY";
                case EGL10.EGL_BAD_MATCH:
                    return "EGL_BAD_MATCH";
                case EGL10.EGL_BAD_NATIVE_PIXMAP:
                    return "EGL_BAD_NATIVE_PIXMAP";
                case EGL10.EGL_BAD_NATIVE_WINDOW:
                    return "EGL_BAD_NATIVE_WINDOW";
                case EGL10.EGL_BAD_PARAMETER:
                    return "EGL_BAD_PARAMETER";
                case EGL10.EGL_BAD_SURFACE:
                    return "EGL_BAD_SURFACE";
                case EGL11.EGL_CONTEXT_LOST:
                    return "EGL_CONTEXT_LOST";
                default:
                    return getHex(error);
            }
        }

        private WeakReference<svrView> mSvrViewManagerWeakRef;
        EGL10 mSvrEgl;
        EGLDisplay mSvrDisplayEgl;
        EGLSurface mSvrSurfaceEgl;
        EGLConfig mSvrConfigEgl;
        EGLContext mSvrContextEgl;

    }

    /**
     * GL Thread. initialize EGL and GL. An instance to do the actual drawing.
     * Render continuously or on demand.
     */
    static class svrGLThread extends Thread {
        svrGLThread(WeakReference<svrView> svrViewManagerWeakRef) {
            super();
            mWidth = 0;
            mHeight = 0;
            mToRender = true;
            mRenderMode = SVR_CONTINUOUSLY_MODE;
            mSvrViewManagerWeakRef = svrViewManagerWeakRef;
        }

        @Override
        public void run() {
            setName("svrGLThread " + getId());
            if (SVR_THREAD_LOG) {
                Log.i("svrGLThread", "starting tid=" + getId());
            }

            try {
                svrRendering();
            } catch (InterruptedException e) {
                // fall thru and exit normally
            } finally {
                sSvrGLThreadManager.threadExiting(this);
            }
        }

        /*
         * called inside a synchronized(sSvrGLThreadManager) block.
         */
        private void svrReleaseEGLSurface() {
            if (mEglSurfaceReady) {
                Log.i(TAG, "svrReleaseEGLSurface");
                mEglSurfaceReady = false;
                mEglManager.svrEGLWindowSurfaceDestroy();
            }
        }

        /*
         * called inside a synchronized(sSvrGLThreadManager) block.
         */
        private void svrReleaseEGLContext() {
            if (mEglContextReady) {
                Log.i(TAG, "svrReleaseEGLContext");
                mEglManager.destroyAndTerminateEGL();
                mEglContextReady = false;
                sSvrGLThreadManager.svrReleaseEGLContext(this);
            }
        }

        private void svrRendering() throws InterruptedException {
            mEglManager = new EGLManager(mSvrViewManagerWeakRef);
            mEglContextReady = false;
            mEglSurfaceReady = false;
            try {
                GL10 gl = null;
                boolean svrEglCreateContext = false;
                boolean svrEglCreateSurface = false;
                boolean svrGlCreateInterface = false;
                boolean lostEglContext = false;
                boolean handleNewSize = false;
                boolean needRenderBroadcast = false;
                boolean sendRenderBroadcast = false;
                boolean applyEglContextToRelease = false;
                int w = 0;
                int h = 0;
                Runnable event = null;

                while (true) {
                    synchronized (sSvrGLThreadManager) {
                        while (true) {
                            if (mToExit) {
                                return;
                            }

                            if (!mSvrEventQueue.isEmpty()) {
                                event = mSvrEventQueue.remove(0);
                                break;
                            }

                            // pause?
                            boolean pausing = false;
                            if (mPaused != mToPause) {
                                pausing = mToPause;
                                mPaused = mToPause;
                                sSvrGLThreadManager.notifyAll();
                                if (SVR_PAUSE_RESUME_LOG) {
                                    Log.i("svrGLThread", "mPaused is now " + mPaused + " tid=" + getId());
                                }
                            }

                            // to drop the EGL context?
                            if (mToReleaseEglContext) {
                                if (SVR_SURFACE_LOG) {
                                    Log.i("svrGLThread", "releasing EGL context because asked to tid=" + getId());
                                }
                                svrReleaseEGLSurface();
                                svrReleaseEGLContext();
                                mToReleaseEglContext = false;
                                applyEglContextToRelease = true;
                            }

                            //lost the EGL context?
                            if (lostEglContext) {
                                Log.i(TAG, "lost Egl context");
                                svrReleaseEGLSurface();
                                svrReleaseEGLContext();
                                lostEglContext = false;
                            }

                            // When pausing, release the EGL surface:
                            if (pausing && mEglSurfaceReady) {
                                if (SVR_SURFACE_LOG) {
                                    Log.i("svrGLThread", "releasing EGL surface because paused tid=" + getId());
                                }
                                svrReleaseEGLSurface();
                            }

                            // We may need to release the EGL Context on pause
                            if (pausing && mEglContextReady) {
                                svrView view = mSvrViewManagerWeakRef.get();
                                boolean preserveEglContextOnPause = view == null ?
                                        false : view.mReserveEglContextWhenPause;
                                if (!preserveEglContextOnPause || sSvrGLThreadManager.eglContextToReleaseOnPause()) {
                                    svrReleaseEGLContext();
                                    if (SVR_SURFACE_LOG) {
                                        Log.i("svrGLThread", "releasing EGL context because paused tid=" + getId());
                                    }
                                }
                            }

                            // We may need to terminate EGL on pause:
                            if (pausing) {
                                if (sSvrGLThreadManager.eglToTerminateOnPause()) {
                                    mEglManager.destroyAndTerminateEGL();
                                    if (SVR_SURFACE_LOG) {
                                        Log.i("svrGLThread", "terminating EGL because paused tid=" + getId());
                                    }
                                }
                            }

                            // lost the surface?
                            if ((!mSurfaceReady) && (!mExpectingSurface)) {
                                if (SVR_SURFACE_LOG) {
                                    Log.i("svrGLThread", "noticed surfaceView surface lost tid=" + getId());
                                }
                                if (mEglSurfaceReady) {
                                    svrReleaseEGLSurface();
                                }
                                mExpectingSurface = true;
                                mBadSurface = false;
                                sSvrGLThreadManager.notifyAll();
                            }

                            // got the surface view surface?
                            if (mSurfaceReady && mExpectingSurface) {
                                if (SVR_SURFACE_LOG) {
                                    Log.i("svrGLThread", "noticed surfaceView surface acquired tid=" + getId());
                                }
                                mExpectingSurface = false;
                                sSvrGLThreadManager.notifyAll();
                            }

                            if (sendRenderBroadcast) {
                                if (SVR_SURFACE_LOG) {
                                    Log.i("svrGLThread", "sending render notification tid=" + getId());
                                }
                                needRenderBroadcast = false;
                                sendRenderBroadcast = false;
                                mFinishRender = true;
                                sSvrGLThreadManager.notifyAll();
                            }

                            // good to draw?
                            if (aboutToDraw()) {
                                Log.i(TAG, "ready to draw in svrRendering");
                                // try to acquire one EGL Context if we don't have.
                                if (!mEglContextReady) {
                                    if (applyEglContextToRelease) {
                                        applyEglContextToRelease = false;
                                    } else if (sSvrGLThreadManager.svrTryGetEGLContex(this)) {
                                        try {
                                            mEglManager.start();
                                        } catch (RuntimeException t) {
                                            sSvrGLThreadManager.svrReleaseEGLContext(this);
                                            throw t;
                                        }
                                        mEglContextReady = true;
                                        svrEglCreateContext = true;

                                        sSvrGLThreadManager.notifyAll();
                                    }
                                }

                                if (mEglContextReady && !mEglSurfaceReady) {
                                    mEglSurfaceReady = true;
                                    svrEglCreateSurface = true;
                                    svrGlCreateInterface = true;
                                    handleNewSize = true;
                                }

                                if (mEglSurfaceReady) {
                                    if (mNewSize) {
                                        handleNewSize = true;
                                        w = mWidth;
                                        h = mHeight;
                                        needRenderBroadcast = true;
                                        if (SVR_SURFACE_LOG) {
                                            Log.i("svrGLThread",
                                                    "noticing that we want render notification tid="
                                                            + getId());
                                        }
                                        svrEglCreateSurface = true;
                                        mNewSize = false;
                                    }
                                    mToRender = false;
                                    sSvrGLThreadManager.notifyAll();
                                    break;
                                }
                            }

                            // only place in a svrGLThread thread where  wait().
                            if (SVR_THREAD_LOG) {
                                Log.i("svrGLThread", "waiting tid=" + getId()
                                        + " mEglContextReady: " + mEglContextReady
                                        + " mEglSurfaceReady: " + mEglSurfaceReady
                                        + " mFinishedCreatingEglSurface: " + mFinishedCreatingEglSurface
                                        + " mPaused: " + mPaused
                                        + " mSurfaceReady: " + mSurfaceReady
                                        + " mBadSurface: " + mBadSurface
                                        + " mExpectingSurface: " + mExpectingSurface
                                        + " mWidth: " + mWidth
                                        + " mHeight: " + mHeight
                                        + " mToRender: " + mToRender
                                        + " mRenderMode: " + mRenderMode);
                            }
                            sSvrGLThreadManager.wait();
                        }
                    } // end of synchronized(sSvrGLThreadManager)

                    if (event != null) {
                        event.run();
                        event = null;
                        continue;
                    }

                    if (svrEglCreateSurface) {
                        if (SVR_SURFACE_LOG) {
                            Log.w("svrGLThread", "egl createSvrSurface");
                        }
                        if (mEglManager.createSvrSurface()) {
                            synchronized (sSvrGLThreadManager) {
                                mFinishedCreatingEglSurface = true;
                                sSvrGLThreadManager.notifyAll();
                            }
                        } else {
                            synchronized (sSvrGLThreadManager) {
                                mFinishedCreatingEglSurface = true;
                                mBadSurface = true;
                                sSvrGLThreadManager.notifyAll();
                            }
                            continue;
                        }
                        svrEglCreateSurface = false;
                    }

                    if (svrGlCreateInterface) {
                        gl = (GL10) mEglManager.createGL();

                        sSvrGLThreadManager.svrGLDriverCheck(gl);
                        svrGlCreateInterface = false;
                    }

                    if (svrEglCreateContext) {
                        if (SVR_RENDERER_LOG) {
                            Log.w("svrGLThread", "onSvrBegin in createEglContect of svrRendering");
                        }
                        svrView view = mSvrViewManagerWeakRef.get();
                        if (view != null) {
                            try {
                                //Trace.traceBegin(Trace.TRACE_TAG_VIEW, "onSvrBegin");
                                view.mConfigSvrFrameParams.setSvrFrameParams();
                                view.mSvrRenderer.onSvrBegin(gl, mEglManager.mSvrConfigEgl);
                                svrApiRenderer.svrBeginVr(m_ParentActivity, mBeginParams);

                            } finally {
                                //Trace.traceEnd(Trace.TRACE_TAG_VIEW);
                            }
                        }
                        svrEglCreateContext = false;
                    }

                    if (handleNewSize) {
                        if (SVR_RENDERER_LOG) {
                            Log.w("svrGLThread", "onSvrViewChanged(" + w + ", " + h + ")");
                        }
                        svrView view = mSvrViewManagerWeakRef.get();
                        if (view != null) {
                            try {
                                //Trace.traceBegin(Trace.TRACE_TAG_VIEW, "onSvrViewChanged");
                                view.mSvrRenderer.onSvrViewChanged(gl, w, h);
                            } finally {
                                //Trace.traceEnd(Trace.TRACE_TAG_VIEW);
                            }
                        }
                        handleNewSize = false;
                    }

                    if (SVR_SUBMIT_FRAME_LOG) {
                        Log.w("svrGLThread", "onSvrSubmitFrame tid=" + getId());
                    }
                    {
                        svrView view = mSvrViewManagerWeakRef.get();
                        if (view != null) {
                            try {
                                view.mSvrRenderer.onSvrSubmitFrame(gl);
                                view.mConfigSvrFrameParams.updatePerFrameParams();
                                svrApiRenderer.svrSubmitFrame(m_ParentActivity, mSvrFrameParams);
                            } finally {
                                //Trace.traceEnd(Trace.TRACE_TAG_VIEW);
                            }
                        }
                    }


                    if (needRenderBroadcast) {
                        sendRenderBroadcast = true;
                    }
                }
            } finally {
                synchronized (sSvrGLThreadManager) {
                    svrReleaseEGLSurface();
                    svrReleaseEGLContext();
                }
            }
        }

        public boolean goodToDraw() {
            return mEglContextReady && mEglSurfaceReady && aboutToDraw();
        }

        private boolean aboutToDraw() {
            return (!mPaused) && mSurfaceReady && (!mBadSurface)
                    && (mWidth > 0) && (mHeight > 0)
                    && (mToRender || (mRenderMode == SVR_CONTINUOUSLY_MODE));
        }

        public void configRenderMode(int renderMode) {
            if (!((SVR_ON_DEMAND_MODE <= renderMode) && (renderMode <= SVR_CONTINUOUSLY_MODE))) {
                throw new IllegalArgumentException("renderMode");
            }
            synchronized (sSvrGLThreadManager) {
                mRenderMode = renderMode;
                sSvrGLThreadManager.notifyAll();
            }
        }

        public int checkRenderMode() {
            synchronized (sSvrGLThreadManager) {
                return mRenderMode;
            }
        }

        public void applyRender() {
            synchronized (sSvrGLThreadManager) {
                mToRender = true;
                sSvrGLThreadManager.notifyAll();
            }
        }

        public void surfaceCreated() {
            synchronized (sSvrGLThreadManager) {
                if (SVR_THREAD_LOG) {
                    Log.i("svrGLThread", "surfaceCreated tid=" + getId());
                }
                mSurfaceReady = true;
                mFinishedCreatingEglSurface = false;
                sSvrGLThreadManager.notifyAll();
                while (mExpectingSurface
                        && !mFinishedCreatingEglSurface
                        && !mHadExited) {
                    try {
                        sSvrGLThreadManager.wait();
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }

        public void surfaceDestroyed() {
            synchronized (sSvrGLThreadManager) {
                if (SVR_THREAD_LOG) {
                    Log.i("svrGLThread", "surfaceDestroyed tid=" + getId());
                }
                mSurfaceReady = false;
                sSvrGLThreadManager.notifyAll();
                while ((!mExpectingSurface) && (!mHadExited)) {
                    try {
                        sSvrGLThreadManager.wait();
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }

        public void onPause() {
            synchronized (sSvrGLThreadManager) {
                if (SVR_PAUSE_RESUME_LOG) {
                    Log.i("svrGLThread", "onPause tid=" + getId());
                }
                mToPause = true;
                sSvrGLThreadManager.notifyAll();
                while ((!mHadExited) && (!mPaused)) {
                    if (SVR_PAUSE_RESUME_LOG) {
                        Log.i("Main thread", "onPause waiting for mPaused.");
                    }
                    try {
                        sSvrGLThreadManager.wait();
                    } catch (InterruptedException ex) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }

        public void onResume() {
            synchronized (sSvrGLThreadManager) {
                if (SVR_PAUSE_RESUME_LOG) {
                    Log.i("svrGLThread", "onResume tid=" + getId());
                }
                mToPause = false;
                mToRender = true;
                mFinishRender = false;
                sSvrGLThreadManager.notifyAll();
                while ((!mHadExited) && mPaused && (!mFinishRender)) {
                    if (SVR_PAUSE_RESUME_LOG) {
                        Log.i("Main thread", "onResume waiting for !mPaused.");
                    }
                    try {
                        sSvrGLThreadManager.wait();
                    } catch (InterruptedException ex) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }

        public void onWindowSizeChanged(int w, int h) {
            if ((mWidth == w) && (mHeight == h)) {
                Log.i(TAG, "size actually the same, return!");
                return;

            }
            synchronized (sSvrGLThreadManager) {
                mWidth = w;
                mHeight = h;
                mNewSize = true;
                mToRender = true;
                mFinishRender = false;
                sSvrGLThreadManager.notifyAll();

                // Wait for thread to react to resize and render a frame
                while (!mHadExited && !mPaused && !mFinishRender
                        && goodToDraw()) {
                    if (SVR_SURFACE_LOG) {
                        Log.i("Main thread", "Waiting for render complete from tid=" + getId());
                    }
                    try {
                        sSvrGLThreadManager.wait();
                    } catch (InterruptedException ex) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }

        public void applyExitThenWaiting() {
            // don't call this from svrGLThread thread or it is a guaranteed
            // deadlock!
            Log.i("svrGLThread", "applyExitThenWaiting" + getId());
            synchronized (sSvrGLThreadManager) {
                mToExit = true;
                sSvrGLThreadManager.notifyAll();
                while (!mHadExited) {
                    try {
                        sSvrGLThreadManager.wait();
                    } catch (InterruptedException ex) {
                        Thread.currentThread().interrupt();
                    }
                }
            }
        }

        public void applySvrReleaseEGLContext() {
            Log.i("svrGLThread", "applySvrReleaseEGLContext" + getId());
            mToReleaseEglContext = true;
            sSvrGLThreadManager.notifyAll();
        }

        /**
         * Queue an "event" to be run on the GL rendering thread.
         *
         * @param r the runnable to be run on the GL rendering thread.
         */
        public void svrQueueEvent(Runnable r) {
            Log.i("svrGLThread", "svrQueueEvent" + getId());
            if (r == null) {
                throw new IllegalArgumentException("r must not be null");
            }
            synchronized (sSvrGLThreadManager) {
                mSvrEventQueue.add(r);
                sSvrGLThreadManager.notifyAll();
            }
        }

        //  protected by the sSvrGLThreadManager monitor once thread start
        private boolean mToExit;
        private boolean mHadExited;
        private boolean mToPause;
        private boolean mPaused;
        private boolean mSurfaceReady;
        private boolean mBadSurface;
        private boolean mExpectingSurface;
        private boolean mEglContextReady;
        private boolean mEglSurfaceReady;
        private boolean mFinishedCreatingEglSurface;
        private boolean mToReleaseEglContext;
        private int mWidth;
        private int mHeight;
        private int mRenderMode;
        private boolean mToRender;
        private boolean mFinishRender;
        private ArrayList<Runnable> mSvrEventQueue = new ArrayList<Runnable>();
        private boolean mNewSize = true;

        // End of member  protected by the sSvrGLThreadManager monitor.

        private EGLManager mEglManager;

        /**
         * for svrView to be garbage collected while
         * the svrGLThread is still alive.
         */
        private WeakReference<svrView> mSvrViewManagerWeakRef;

    }

    static class writeSvrLog extends Writer {

        @Override
        public void close() {
            flushBuilder();
        }

        @Override
        public void flush() {
            flushBuilder();
        }

        @Override
        public void write(char[] buf, int offset, int count) {
            for (int i = 0; i < count; i++) {
                char c = buf[offset + i];
                if (c == '\n') {
                    flushBuilder();
                } else {
                    mBuilder.append(c);
                }
            }
        }

        private void flushBuilder() {
            if (mBuilder.length() > 0) {
                Log.v("svrView", mBuilder.toString());
                mBuilder.delete(0, mBuilder.length());
            }
        }

        private StringBuilder mBuilder = new StringBuilder();
    }


    private void renderThreadRunningState() {
        if (mSvrGLThread != null) {
            throw new IllegalStateException(
                    "setSvrRenderer exist!.");
        }
    }

    private static class svrGLThreadManager {
        private static String TAG = "svrGLThreadManager";

        public synchronized void threadExiting(svrGLThread thread) {
            if (SVR_THREAD_LOG) {
                Log.i("svrGLThread", "exiting tid=" + thread.getId());
            }
            thread.mHadExited = true;
            if (mSvrOwnerEGL == thread) {
                mSvrOwnerEGL = null;
            }
            notifyAll();
        }

        /*
         * Try to acquire  EGL context to use
         * @return true if  an EGL context was acquired.
         */
        public boolean svrTryGetEGLContex(svrGLThread thread) {
            if (mSvrOwnerEGL == thread || mSvrOwnerEGL == null) {
                mSvrOwnerEGL = thread;
                notifyAll();
                return true;
            }
            svrGlesVersionCheck();
            if (mSvrAllowedMulGlesContext) {
                return true;
            }
            // if the owning thread is in continuously mode it will just
            // reacquire the EGL context.
            if (mSvrOwnerEGL != null) {
                mSvrOwnerEGL.applySvrReleaseEGLContext();
            }
            return false;
        }

        /*
         * Release the EGL context.
         */
        public void svrReleaseEGLContext(svrGLThread thread) {
            if (mSvrOwnerEGL == thread) {
                mSvrOwnerEGL = null;
            }
            notifyAll();
        }

        public synchronized boolean eglContextToReleaseOnPause() {
            return mGLESContextLimited;
        }

        public synchronized boolean eglToTerminateOnPause() {
            svrGlesVersionCheck();
            return !mSvrAllowedMulGlesContext;
        }

        public synchronized void svrGLDriverCheck(GL10 gl) {
            if (!mSvrGLESDriverCheckDone) {
                svrGlesVersionCheck();
                String renderer = gl.glGetString(GL10.GL_RENDERER);
                mGLESContextLimited = !mSvrAllowedMulGlesContext;
                if (SVR_SURFACE_LOG) {
                    Log.w(TAG, "svrGLDriverCheck renderer = \"" + renderer + "\" multipleContextsAllowed = "
                            + mSvrAllowedMulGlesContext
                            + " mGLESContextLimited = " + mGLESContextLimited);
                }
                mSvrGLESDriverCheckDone = true;
            }
        }

        private static int glesVersionFromPM(Context context) {
            PackageManager packageManager = context.getPackageManager();
            FeatureInfo[] mFeatureInfo = packageManager.getSystemAvailableFeatures();
            if (mFeatureInfo != null && mFeatureInfo.length > 0) {
                for (FeatureInfo vFeatureInfo : mFeatureInfo) {
                    if (vFeatureInfo.name == null) {
                        if (vFeatureInfo.reqGlEsVersion != FeatureInfo.GL_ES_VERSION_UNDEFINED) {
                            return vFeatureInfo.reqGlEsVersion;
                        } else {
                            return 1; // OpenGL ES version 1 IF WE got nothing
                        }
                    }
                }
            }
            return 1;
        }

        private void svrGlesVersionCheck() {
            if (!mSvrCheckGLESVersionDone) {
                mVersionGLES = glesVersionFromPM(mContext);
                Log.i(TAG, "GLES Version is " + mVersionGLES);
                if (mVersionGLES >= GLES20Value) {
                    mSvrAllowedMulGlesContext = true;
                }
                if (SVR_SURFACE_LOG) {
                    Log.w(TAG, "svrGlesVersionCheck mVersionGLES =" +
                            " " + mVersionGLES + " mSvrAllowedMulGlesContext = " + mSvrAllowedMulGlesContext);
                }
                mSvrCheckGLESVersionDone = true;
            }
        }

        /**
         * multiple EGL contexts are supported on all Android 3.0+ EGL drivers.
         * reserve this for future higher opengl version e.g 4.0
         */
        private boolean mSvrCheckGLESVersionDone;
        private int mVersionGLES;
        private boolean mSvrGLESDriverCheckDone;
        private boolean mSvrAllowedMulGlesContext;
        private boolean mGLESContextLimited;
        private static final int GLES20Value = 0x20000;
        private svrGLThread mSvrOwnerEGL;
    }//end of svrGLThreadManager

    private static final svrGLThreadManager sSvrGLThreadManager = new svrGLThreadManager();

    private final WeakReference<svrView> mThisWeakRef =
            new WeakReference<svrView>(this);
    private svrGLThread mSvrGLThread;
    private svrRenderer mSvrRenderer;
    private configSvrFrameParams mConfigSvrFrameParams;
    private boolean mDetached;
    private svrEGLConfigManager mConfigEGL;
    private svrEGLContextManager mEGLContextManager;
    private svrEGLWindowSurfaceManager mEGLWindowSurfaceManager;
    private svrGLManager mLocalGL;
    private int mDebugLog;
    private int mEGLContextClientVersion;
    private boolean mReserveEglContextWhenPause;
    public static Context mContext;
    public static SvrApi svrApiRenderer;
    public static svrFrameParams mSvrFrameParams;
    public static svrHeadPoseState mHeadPoseState;
    public static svrBeginParams mBeginParams;
    public static svrLayoutCoords layoutCoords;
    public static float predictedTimeMs = 0.0f;

}
