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

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.opengl.GLException;
import android.view.Surface;
import android.view.SurfaceHolder;
import androidx.annotation.Nullable;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

/**
 * Holds EGL state and utility methods for handling an egl 1.0 EGLContext, an EGLDisplay,
 * and an EGLSurface.
 */
class EglBase10Impl implements EglBase10 {
  private static final String TAG = "EglBase10Impl";
  // This constant is taken from EGL14.EGL_CONTEXT_CLIENT_VERSION.
  private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

  private static final EglConnection EGL_NO_CONNECTION = new EglConnection();

  private EGLSurface eglSurface = EGL10.EGL_NO_SURFACE;
  private EglConnection eglConnection;

  // EGL wrapper for an actual EGLContext.
  private static class Context implements EglBase10.Context {
    private final EGL10 egl;
    private final EGLContext eglContext;
    private final EGLConfig eglContextConfig;

    @Override
    public EGLContext getRawContext() {
      return eglContext;
    }

    @Override
    public long getNativeEglContext() {
      EGLContext previousContext = egl.eglGetCurrentContext();
      EGLDisplay currentDisplay = egl.eglGetCurrentDisplay();
      EGLSurface previousDrawSurface = egl.eglGetCurrentSurface(EGL10.EGL_DRAW);
      EGLSurface previousReadSurface = egl.eglGetCurrentSurface(EGL10.EGL_READ);
      EGLSurface tempEglSurface = null;

      if (currentDisplay == EGL10.EGL_NO_DISPLAY) {
        currentDisplay = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
      }

      try {
        if (previousContext != eglContext) {
          int[] surfaceAttribs = {EGL10.EGL_WIDTH, 1, EGL10.EGL_HEIGHT, 1, EGL10.EGL_NONE};
          tempEglSurface =
              egl.eglCreatePbufferSurface(currentDisplay, eglContextConfig, surfaceAttribs);
          if (!egl.eglMakeCurrent(currentDisplay, tempEglSurface, tempEglSurface, eglContext)) {
            throw new GLException(egl.eglGetError(),
                "Failed to make temporary EGL surface active: " + egl.eglGetError());
          }
        }

        return nativeGetCurrentNativeEGLContext();
      } finally {
        if (tempEglSurface != null) {
          egl.eglMakeCurrent(
              currentDisplay, previousDrawSurface, previousReadSurface, previousContext);
          egl.eglDestroySurface(currentDisplay, tempEglSurface);
        }
      }
    }

    public Context(EGL10 egl, EGLContext eglContext, EGLConfig eglContextConfig) {
      this.egl = egl;
      this.eglContext = eglContext;
      this.eglContextConfig = eglContextConfig;
    }
  }

  public static class EglConnection implements EglBase10.EglConnection {
    private final EGL10 egl;
    private final EGLContext eglContext;
    private final EGLDisplay eglDisplay;
    private final EGLConfig eglConfig;
    private final RefCountDelegate refCountDelegate;
    private EGLSurface currentSurface = EGL10.EGL_NO_SURFACE;

    public EglConnection(EGLContext sharedContext, int[] configAttributes) {
      egl = (EGL10) EGLContext.getEGL();
      eglDisplay = getEglDisplay(egl);
      eglConfig = getEglConfig(egl, eglDisplay, configAttributes);
      final int openGlesVersion = EglBase.getOpenGlesVersionFromConfig(configAttributes);
      Logging.d(TAG, "Using OpenGL ES version " + openGlesVersion);
      eglContext = createEglContext(egl, sharedContext, eglDisplay, eglConfig, openGlesVersion);

      // Ref count delegate with release callback.
      refCountDelegate = new RefCountDelegate(() -> {
        synchronized (EglBase.lock) {
          egl.eglMakeCurrent(
              eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
        }
        egl.eglDestroyContext(eglDisplay, eglContext);
        egl.eglTerminate(eglDisplay);
        currentSurface = EGL10.EGL_NO_SURFACE;
      });
    }

    // Returns a "null" EglConnection. Useful to represent a released instance with default values.
    private EglConnection() {
      egl = (EGL10) EGLContext.getEGL();
      eglContext = EGL10.EGL_NO_CONTEXT;
      eglDisplay = EGL10.EGL_NO_DISPLAY;
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
    public EGL10 getEgl() {
      return egl;
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
      if (egl.eglGetCurrentContext() == eglContext && currentSurface == eglSurface) {
        return;
      }

      synchronized (EglBase.lock) {
        if (!egl.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
          throw new GLException(egl.eglGetError(),
              "eglMakeCurrent failed: 0x" + Integer.toHexString(egl.eglGetError()));
        }
      }
      currentSurface = eglSurface;
    }

    public void detachCurrent() {
      synchronized (EglBase.lock) {
        if (!egl.eglMakeCurrent(
                eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT)) {
          throw new GLException(egl.eglGetError(),
              "eglDetachCurrent failed: 0x" + Integer.toHexString(egl.eglGetError()));
        }
      }
      currentSurface = EGL10.EGL_NO_SURFACE;
    }
  }

  // Create a new context with the specified config type, sharing data with sharedContext.
  public EglBase10Impl(EGLContext sharedContext, int[] configAttributes) {
    this.eglConnection = new EglConnection(sharedContext, configAttributes);
  }

  public EglBase10Impl(EglConnection eglConnection) {
    this.eglConnection = eglConnection;
    this.eglConnection.retain();
  }

  @Override
  public void createSurface(Surface surface) {
    /**
     * We have to wrap Surface in a SurfaceHolder because for some reason eglCreateWindowSurface
     * couldn't actually take a Surface object until API 17. Older versions fortunately just call
     * SurfaceHolder.getSurface(), so we'll do that. No other methods are relevant.
     */
    class FakeSurfaceHolder implements SurfaceHolder {
      private final Surface surface;

      FakeSurfaceHolder(Surface surface) {
        this.surface = surface;
      }

      @Override
      public void addCallback(Callback callback) {}

      @Override
      public void removeCallback(Callback callback) {}

      @Override
      public boolean isCreating() {
        return false;
      }

      @Deprecated
      @Override
      public void setType(int i) {}

      @Override
      public void setFixedSize(int i, int i2) {}

      @Override
      public void setSizeFromLayout() {}

      @Override
      public void setFormat(int i) {}

      @Override
      public void setKeepScreenOn(boolean b) {}

      @Nullable
      @Override
      public Canvas lockCanvas() {
        return null;
      }

      @Nullable
      @Override
      public Canvas lockCanvas(Rect rect) {
        return null;
      }

      @Override
      public void unlockCanvasAndPost(Canvas canvas) {}

      @Nullable
      @Override
      public Rect getSurfaceFrame() {
        return null;
      }

      @Override
      public Surface getSurface() {
        return surface;
      }
    }

    createSurfaceInternal(new FakeSurfaceHolder(surface));
  }

  // Create EGLSurface from the Android SurfaceTexture.
  @Override
  public void createSurface(SurfaceTexture surfaceTexture) {
    createSurfaceInternal(surfaceTexture);
  }

  // Create EGLSurface from either a SurfaceHolder or a SurfaceTexture.
  private void createSurfaceInternal(Object nativeWindow) {
    if (!(nativeWindow instanceof SurfaceHolder) && !(nativeWindow instanceof SurfaceTexture)) {
      throw new IllegalStateException("Input must be either a SurfaceHolder or SurfaceTexture");
    }
    checkIsNotReleased();
    if (eglSurface != EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }

    EGL10 egl = eglConnection.getEgl();
    int[] surfaceAttribs = {EGL10.EGL_NONE};
    eglSurface = egl.eglCreateWindowSurface(
        eglConnection.getDisplay(), eglConnection.getConfig(), nativeWindow, surfaceAttribs);
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new GLException(egl.eglGetError(),
          "Failed to create window surface: 0x" + Integer.toHexString(egl.eglGetError()));
    }
  }

  // Create dummy 1x1 pixel buffer surface so the context can be made current.
  @Override
  public void createDummyPbufferSurface() {
    createPbufferSurface(1, 1);
  }

  @Override
  public void createPbufferSurface(int width, int height) {
    checkIsNotReleased();
    if (eglSurface != EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    EGL10 egl = eglConnection.getEgl();
    int[] surfaceAttribs = {EGL10.EGL_WIDTH, width, EGL10.EGL_HEIGHT, height, EGL10.EGL_NONE};
    eglSurface = egl.eglCreatePbufferSurface(
        eglConnection.getDisplay(), eglConnection.getConfig(), surfaceAttribs);
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new GLException(egl.eglGetError(),
          "Failed to create pixel buffer surface with size " + width + "x" + height + ": 0x"
              + Integer.toHexString(egl.eglGetError()));
    }
  }

  @Override
  public org.webrtc.EglBase.Context getEglBaseContext() {
    return new Context(
        eglConnection.getEgl(), eglConnection.getContext(), eglConnection.getConfig());
  }

  @Override
  public boolean hasSurface() {
    return eglSurface != EGL10.EGL_NO_SURFACE;
  }

  @Override
  public int surfaceWidth() {
    final int widthArray[] = new int[1];
    eglConnection.getEgl().eglQuerySurface(
        eglConnection.getDisplay(), eglSurface, EGL10.EGL_WIDTH, widthArray);
    return widthArray[0];
  }

  @Override
  public int surfaceHeight() {
    final int heightArray[] = new int[1];
    eglConnection.getEgl().eglQuerySurface(
        eglConnection.getDisplay(), eglSurface, EGL10.EGL_HEIGHT, heightArray);
    return heightArray[0];
  }

  @Override
  public void releaseSurface() {
    if (eglSurface != EGL10.EGL_NO_SURFACE) {
      eglConnection.getEgl().eglDestroySurface(eglConnection.getDisplay(), eglSurface);
      eglSurface = EGL10.EGL_NO_SURFACE;
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
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
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
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't swap buffers");
    }
    synchronized (EglBase.lock) {
      eglConnection.getEgl().eglSwapBuffers(eglConnection.getDisplay(), eglSurface);
    }
  }

  @Override
  public void swapBuffers(long timeStampNs) {
    // Setting presentation time is not supported for EGL 1.0.
    swapBuffers();
  }

  // Return an EGLDisplay, or die trying.
  private static EGLDisplay getEglDisplay(EGL10 egl) {
    EGLDisplay eglDisplay = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL10.EGL_NO_DISPLAY) {
      throw new GLException(egl.eglGetError(),
          "Unable to get EGL10 display: 0x" + Integer.toHexString(egl.eglGetError()));
    }
    int[] version = new int[2];
    if (!egl.eglInitialize(eglDisplay, version)) {
      throw new GLException(egl.eglGetError(),
          "Unable to initialize EGL10: 0x" + Integer.toHexString(egl.eglGetError()));
    }
    return eglDisplay;
  }

  // Return an EGLConfig, or die trying.
  private static EGLConfig getEglConfig(EGL10 egl, EGLDisplay eglDisplay, int[] configAttributes) {
    EGLConfig[] configs = new EGLConfig[1];
    int[] numConfigs = new int[1];
    if (!egl.eglChooseConfig(eglDisplay, configAttributes, configs, configs.length, numConfigs)) {
      throw new GLException(
          egl.eglGetError(), "eglChooseConfig failed: 0x" + Integer.toHexString(egl.eglGetError()));
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
  private static EGLContext createEglContext(EGL10 egl, @Nullable EGLContext sharedContext,
      EGLDisplay eglDisplay, EGLConfig eglConfig, int openGlesVersion) {
    if (sharedContext != null && sharedContext == EGL10.EGL_NO_CONTEXT) {
      throw new RuntimeException("Invalid sharedContext");
    }
    int[] contextAttributes = {EGL_CONTEXT_CLIENT_VERSION, openGlesVersion, EGL10.EGL_NONE};
    EGLContext rootContext = sharedContext == null ? EGL10.EGL_NO_CONTEXT : sharedContext;
    final EGLContext eglContext;
    synchronized (EglBase.lock) {
      eglContext = egl.eglCreateContext(eglDisplay, eglConfig, rootContext, contextAttributes);
    }
    if (eglContext == EGL10.EGL_NO_CONTEXT) {
      throw new GLException(egl.eglGetError(),
          "Failed to create EGL context: 0x" + Integer.toHexString(egl.eglGetError()));
    }
    return eglContext;
  }

  private static native long nativeGetCurrentNativeEGLContext();
}
