/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

/** EGL 1.0 implementation of EglBase. */
public interface EglBase10 extends EglBase {
  interface Context extends EglBase.Context {
    EGLContext getRawContext();
  }

  interface EglConnection extends EglBase.EglConnection {
    EGL10 getEgl();

    EGLContext getContext();

    EGLDisplay getDisplay();

    EGLConfig getConfig();
  }
}
