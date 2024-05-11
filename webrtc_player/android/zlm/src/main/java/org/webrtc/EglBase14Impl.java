/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLExt;
import android.opengl.EGLSurface;
import android.opengl.GLException;
import android.view.Surface;
import androidx.annotation.Nullable;

/**
 * Holds EGL state and utility methods for handling an EGL14 EGLContext, an EGLDisplay,
 * and an EGLSurface.
 */
@SuppressWarnings("ReferenceEquality") // We want to compare to EGL14 constants.
class EglBase14Impl implements EglBase14 {
  private static final String TAG = "EglBase14Impl";
  private static final EglConnection EGL_NO_CONNECTION = new EglConnection();

  private EGLSurface eglSurface = EGL14.EGL_NO_SURFACE;
  private EglConnection eglConnection;

  public static class Context implements EglBase14.Context {
    private final EGLContext egl14Context;

    @Override
    public EGLContext getRawContext() {
      return egl14Context;
    }

    @Override
    public long getNativeEglContext() {
      return egl14Context.getNativeHandle();
    }

    public Context(android.opengl.EGLContext eglContext) {
      this.egl14Context = eglContext;
    }
  }

  public static class EglConnection implements EglBase14.EglConnection {
    private final EGLContext eglContext;
    private final EGLDisplay eglDisplay;
    private final EGLConfig eglConfig;
    private final RefCountDelegate refCountDelegate;
    private EGLSurface currentSurface = EGL14.EGL_NO_SURFACE;

    public EglConnection(EGLContext sharedContext, int[] configAttributes) {
      eglDisplay = getEglDisplay();
      eglConfig = getEglConfig(eglDisplay, configAttributes);
      final int openGlesVersion = EglBase.getOpenGlesVersionFromConfig(configAttributes);
      Logging.d(TAG, "Using OpenGL ES version " + openGlesVersion);
      eglContext = createEglContext(sharedContext, eglDisplay, eglConfig, openGlesVersion);

      // Ref count delegate with release callback.
      refCountDelegate = new RefCountDelegate(() -> {
        synchronized (EglBase.lock) {
          EGL14.eglMakeCurrent(
              eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
          EGL14.eglDestroyContext(eglDisplay, eglContext);
        }
        EGL14.eglReleaseThread();
        EGL14.eglTerminate(eglDisplay);
        currentSurface = EGL14.EGL_NO_SURFACE;
      });
    }

    // Returns a "null" EglConnection. Useful to represent a released instance with default values.
    private EglConnection() {
      eglContext = EGL14.EGL_NO_CONTEXT;
      eglDisplay = EGL14.EGL_NO_DISPLAY;
      eglConfig = null;
      refCountDelegate = new RefCountDelegate(() -> {});
    }

    @Override
    public void retain() {
      refCountDelegate.retain();
    }

    @Override
    public void release() {
      refCountDelegate.release();
    }

    @Override
    public EGLContext getContext() {
      return eglContext;
    }

    @Override
    public EGLDisplay getDisplay() {
      return eglDisplay;
    }

    @Override
    public EGLConfig getConfig() {
      return eglConfig;
    }

    public void makeCurrent(EGLSurface eglSurface) {
      if (EGL14.eglGetCurrentContext() == eglContext && currentSurface == eglSurface) {
        return;
      }

      synchronized (EglBase.lock) {
        if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
          throw new GLException(EGL14.eglGetError(),
              "eglMakeCurrent failed: 0x" + Integer.toHexString(EGL14.eglGetError()));
        }
      }
      currentSurface = eglSurface;
    }

    public void detachCurrent() {
      synchronized (EglBase.lock) {
        if (!EGL14.eglMakeCurrent(
                eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)) {
          throw new GLException(EGL14.eglGetError(),
              "eglDetachCurrent failed: 0x" + Integer.toHexString(EGL14.eglGetError()));
        }
      }
      currentSurface = EGL14.EGL_NO_SURFACE;
    }
  }
  // Create a new context with the specified config type, sharing data with sharedContext.
  // `sharedContext` may be null.
  public EglBase14Impl(EGLContext sharedContext, int[] configAttributes) {
    this.eglConnection = new EglConnection(sharedContext, configAttributes);
  }

  // Create a new EglBase using an existing, possibly externally managed, EglConnection.
  public EglBase14Impl(EglConnection eglConnection) {
    this.eglConnection = eglConnection;
    this.eglConnection.retain();
  }

  // Create EGLSurface from the Android Surface.
  @Override
  public void createSurface(Surface surface) {
    createSurfaceInternal(surface);
  }

  // Create EGLSurface from the Android SurfaceTexture.
  @Override
  public void createSurface(SurfaceTexture surfaceTexture) {
    createSurfaceInternal(surfaceTexture);
  }

  // Create EGLSurface from either Surface or SurfaceTexture.
  private void createSurfaceInternal(Object surface) {
    if (!(surface instanceof Surface) && !(surface instanceof SurfaceTexture)) {
      throw new IllegalStateException("Input must be either a Surface or SurfaceTexture");
    }
    checkIsNotReleased();
    if (eglSurface != EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    int[] surfaceAttribs = {EGL14.EGL_NONE};
    eglSurface = EGL14.eglCreateWindowSurface(
        eglConnection.getDisplay(), eglConnection.getConfig(), surface, surfaceAttribs, 0);
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new GLException(EGL14.eglGetError(),
          "Failed to create window surface: 0x" + Integer.toHexString(EGL14.eglGetError()));
    }
  }

  @Override
  public void createDummyPbufferSurface() {
    createPbufferSurface(1, 1);
  }

  @Override
  public void createPbufferSurface(int width, int height) {
    checkIsNotReleased();
    if (eglSurface != EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    int[] surfaceAttribs = {EGL14.EGL_WIDTH, width, EGL14.EGL_HEIGHT, height, EGL14.EGL_NONE};
    eglSurface = EGL14.eglCreatePbufferSurface(
        eglConnection.getDisplay(), eglConnection.getConfig(), surfaceAttribs, 0);
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new GLException(EGL14.eglGetError(),
          "Failed to create pixel buffer surface with size " + width + "x" + height + ": 0x"
              + Integer.toHexString(EGL14.eglGetError()));
    }
  }

  @Override
  public Context getEglBaseContext() {
    return new Context(eglConnection.getContext());
  }

  @Override
  public boolean hasSurface() {
    return eglSurface != EGL14.EGL_NO_SURFACE;
  }

  @Override
  public int surfaceWidth() {
    final int[] widthArray = new int[1];
    EGL14.eglQuerySurface(eglConnection.getDisplay(), eglSurface, EGL14.EGL_WIDTH, widthArray, 0);
    return widthArray[0];
  }

  @Override
  public int surfaceHeight() {
    final int[] heightArray = new int[1];
    EGL14.eglQuerySurface(eglConnection.getDisplay(), eglSurface, EGL14.EGL_HEIGHT, heightArray, 0);
    return heightArray[0];
  }

  @Override
  public void releaseSurface() {
    if (eglSurface != EGL14.EGL_NO_SURFACE) {
      EGL14.eglDestroySurface(eglConnection.getDisplay(), eglSurface);
      eglSurface = EGL14.EGL_NO_SURFACE;
    }
  }

  private void checkIsNotReleased() {
    if (eglConnection == EGL_NO_CONNECTION) {
      throw new RuntimeException("This object has been released");
    }
  }

  @Override
  public void release() {
    checkIsNotReleased();
    releaseSurface();
    eglConnection.release();
    eglConnection = EGL_NO_CONNECTION;
  }

  @Override
  public void makeCurrent() {
    checkIsNotReleased();
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't make current");
    }
    eglConnection.makeCurrent(eglSurface);
  }

  // Detach the current EGL context, so that it can be made current on another thread.
  @Override
  public void detachCurrent() {
    eglConnection.detachCurrent();
  }

  @Override
  public void swapBuffers() {
    checkIsNotReleased();
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't swap buffers");
    }
    synchronized (EglBase.lock) {
      EGL14.eglSwapBuffers(eglConnection.getDisplay(), eglSurface);
    }
  }

  @Override
  public void swapBuffers(long timeStampNs) {
    checkIsNotReleased();
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't swap buffers");
    }
    synchronized (EglBase.lock) {
      // See
      // https://android.googlesource.com/platform/frameworks/native/+/tools_r22.2/opengl/specs/EGL_ANDROID_presentation_time.txt
      EGLExt.eglPresentationTimeANDROID(eglConnection.getDisplay(), eglSurface, timeStampNs);
      EGL14.eglSwapBuffers(eglConnection.getDisplay(), eglSurface);
    }
  }

  // Return an EGLDisplay, or die trying.
  private static EGLDisplay getEglDisplay() {
    EGLDisplay eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
      throw new GLException(EGL14.eglGetError(),
          "Unable to get EGL14 display: 0x" + Integer.toHexString(EGL14.eglGetError()));
    }
    int[] version = new int[2];
    if (!EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) {
      throw new GLException(EGL14.eglGetError(),
          "Unable to initialize EGL14: 0x" + Integer.toHexString(EGL14.eglGetError()));
    }
    return eglDisplay;
  }

  // Return an EGLConfig, or die trying.
  private static EGLConfig getEglConfig(EGLDisplay eglDisplay, int[] configAttributes) {
    EGLConfig[] configs = new EGLConfig[1];
    int[] numConfigs = new int[1];
    if (!EGL14.eglChooseConfig(
            eglDisplay, configAttributes, 0, configs, 0, configs.length, numConfigs, 0)) {
      throw new GLException(EGL14.eglGetError(),
          "eglChooseConfig failed: 0x" + Integer.toHexString(EGL14.eglGetError()));
    }
    if (numConfigs[0] <= 0) {
      throw new RuntimeException("Unable to find any matching EGL config");
    }
    final EGLConfig eglConfig = configs[0];
    if (eglConfig == null) {
      throw new RuntimeException("eglChooseConfig returned null");
    }
    return eglConfig;
  }

  // Return an EGLConfig, or die trying.
  private static EGLContext createEglContext(@Nullable EGLContext sharedContext,
      EGLDisplay eglDisplay, EGLConfig eglConfig, int openGlesVersion) {
    if (sharedContext != null && sharedContext == EGL14.EGL_NO_CONTEXT) {
      throw new RuntimeException("Invalid sharedContext");
    }
    int[] contextAttributes = {EGL14.EGL_CONTEXT_CLIENT_VERSION, openGlesVersion, EGL14.EGL_NONE};
    EGLContext rootContext = sharedContext == null ? EGL14.EGL_NO_CONTEXT : sharedContext;
    final EGLContext eglContext;
    synchronized (EglBase.lock) {
      eglContext = EGL14.eglCreateContext(eglDisplay, eglConfig, rootContext, contextAttributes, 0);
    }
    if (eglContext == EGL14.EGL_NO_CONTEXT) {
      throw new GLException(EGL14.eglGetError(),
          "Failed to create EGL context: 0x" + Integer.toHexString(EGL14.eglGetError()));
    }
    return eglContext;
  }
}
