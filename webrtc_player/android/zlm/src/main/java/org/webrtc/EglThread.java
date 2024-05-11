/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.List;
import org.webrtc.EglBase.EglConnection;

/** EGL graphics thread that allows multiple clients to share the same underlying EGLContext. */
public class EglThread implements RenderSynchronizer.Listener {
  /** Callback for externally managed reference count. */
  public interface ReleaseMonitor {
    /**
     * Called by EglThread when a client releases its reference. Returns true when there are no more
     * references and resources should be released.
     */
    boolean onRelease(EglThread eglThread);
  }

  /** Interface for clients to schedule rendering updates that will run synchronized. */
  public interface RenderUpdate {

    /**
     * Called by EglThread when the rendering window is open. `runsInline` is true when the update
     * is executed directly while the client schedules the update.
     */
    void update(boolean runsInline);
  }

  public static EglThread create(
      @Nullable ReleaseMonitor releaseMonitor,
      @Nullable final EglBase.Context sharedContext,
      final int[] configAttributes,
      @Nullable RenderSynchronizer renderSynchronizer) {
    final HandlerThread renderThread = new HandlerThread("EglThread");
    renderThread.start();
    HandlerWithExceptionCallbacks handler =
        new HandlerWithExceptionCallbacks(renderThread.getLooper());

    // Not creating the EGLContext on the thread it will be used on seems to cause issues with
    // creating window surfaces on certain devices. So keep the same legacy behavior as EglRenderer
    // and create the context on the render thread.
    EglConnection eglConnection = ThreadUtils.invokeAtFrontUninterruptibly(handler, () -> {
      // If sharedContext is null, then texture frames are disabled. This is typically for old
      // devices that might not be fully spec compliant, so force EGL 1.0 since EGL 1.4 has
      // caused trouble on some weird devices.
      if (sharedContext == null) {
        return EglConnection.createEgl10(configAttributes);
      } else {
        return EglConnection.create(sharedContext, configAttributes);
      }
    });

    return new EglThread(
        releaseMonitor != null ? releaseMonitor : eglThread -> true,
        handler,
        eglConnection,
        renderSynchronizer);
  }

  public static EglThread create(
      @Nullable ReleaseMonitor releaseMonitor,
      @Nullable final EglBase.Context sharedContext,
      final int[] configAttributes) {
    return create(releaseMonitor, sharedContext, configAttributes, /* renderSynchronizer= */ null);
  }

  /**
   * Handler that triggers callbacks when an uncaught exception happens when handling a message.
   */
  private static class HandlerWithExceptionCallbacks extends Handler {
    private final Object callbackLock = new Object();
    @GuardedBy("callbackLock") private final List<Runnable> exceptionCallbacks = new ArrayList<>();

    public HandlerWithExceptionCallbacks(Looper looper) {
      super(looper);
    }

    @Override
    public void dispatchMessage(Message msg) {
      try {
        super.dispatchMessage(msg);
      } catch (Exception e) {
        Logging.e("EglThread", "Exception on EglThread", e);
        synchronized (callbackLock) {
          for (Runnable callback : exceptionCallbacks) {
            callback.run();
          }
        }
        throw e;
      }
    }

    public void addExceptionCallback(Runnable callback) {
      synchronized (callbackLock) {
        exceptionCallbacks.add(callback);
      }
    }

    public void removeExceptionCallback(Runnable callback) {
      synchronized (callbackLock) {
        exceptionCallbacks.remove(callback);
      }
    }
  }

  private final ReleaseMonitor releaseMonitor;
  private final HandlerWithExceptionCallbacks handler;
  private final EglConnection eglConnection;
  private final RenderSynchronizer renderSynchronizer;
  private final List<RenderUpdate> pendingRenderUpdates = new ArrayList<>();
  private boolean renderWindowOpen = true;

  private EglThread(
      ReleaseMonitor releaseMonitor,
      HandlerWithExceptionCallbacks handler,
      EglConnection eglConnection,
      RenderSynchronizer renderSynchronizer) {
    this.releaseMonitor = releaseMonitor;
    this.handler = handler;
    this.eglConnection = eglConnection;
    this.renderSynchronizer = renderSynchronizer;
    if (renderSynchronizer != null) {
      renderSynchronizer.registerListener(this);
    }
  }

  public void release() {
    if (!releaseMonitor.onRelease(this)) {
      // Thread is still in use, do not release yet.
      return;
    }

    if (renderSynchronizer != null) {
      renderSynchronizer.removeListener(this);
    }

    handler.post(eglConnection::release);
    handler.getLooper().quitSafely();
  }

  /**
   * Creates an EglBase instance with the EglThread's EglConnection. This method can be called on
   * any thread, but the returned EglBase instance should only be used on this EglThread's Handler.
   */
  public EglBase createEglBaseWithSharedConnection() {
    return EglBase.create(eglConnection);
  }

  /**
   * Returns the Handler to interact with Gl/EGL on. Callers need to make sure that their own
   * EglBase is current on the handler before running any graphics operations since the EglThread
   * can be shared by multiple clients.
   */
  public Handler getHandler() {
    return handler;
  }

  /**
   * Adds a callback that will be called on the EGL thread if there is an exception on the thread.
   */
  public void addExceptionCallback(Runnable callback) {
    handler.addExceptionCallback(callback);
  }

  /**
   * Removes a previously added exception callback.
   */
  public void removeExceptionCallback(Runnable callback) {
    handler.removeExceptionCallback(callback);
  }

  /**
   * Schedules a render update (like swapBuffers) to be run in sync with other updates on the next
   * open render window. If the render window is currently open the update will run immediately.
   * This method must be called on the EglThread during a render pass.
   */
  public void scheduleRenderUpdate(RenderUpdate update) {
    if (renderWindowOpen) {
      update.update(/* runsInline = */true);
    } else {
      pendingRenderUpdates.add(update);
    }
  }

  @Override
  public void onRenderWindowOpen() {
    handler.post(
        () -> {
          renderWindowOpen = true;
          for (RenderUpdate update : pendingRenderUpdates) {
            update.update(/* runsInline = */false);
          }
          pendingRenderUpdates.clear();
        });
  }

  @Override
  public void onRenderWindowClose() {
    handler.post(() -> renderWindowOpen = false);
  }
}
