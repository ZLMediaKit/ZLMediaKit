/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package com.zlm.rtc.client;

import android.os.ParcelFileDescriptor;
import android.util.Log;

import org.webrtc.PeerConnection;

import java.io.File;
import java.io.IOException;

public class RtcEventLog {
  private static final String TAG = "RtcEventLog";
  private static final int OUTPUT_FILE_MAX_BYTES = 10_000_000;
  private final PeerConnection peerConnection;
  private RtcEventLogState state = RtcEventLogState.INACTIVE;

  enum RtcEventLogState {
    INACTIVE,
    STARTED,
    STOPPED,
  }

  public RtcEventLog(PeerConnection peerConnection) {
    if (peerConnection == null) {
      throw new NullPointerException("The peer connection is null.");
    }
    this.peerConnection = peerConnection;
  }

  public void start(final File outputFile) {
    if (state == RtcEventLogState.STARTED) {
      Log.e(TAG, "RtcEventLog has already started.");
      return;
    }
    final ParcelFileDescriptor fileDescriptor;
    try {
      fileDescriptor = ParcelFileDescriptor.open(outputFile,
          ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE
              | ParcelFileDescriptor.MODE_TRUNCATE);
    } catch (IOException e) {
      Log.e(TAG, "Failed to create a new file", e);
      return;
    }

    // Passes ownership of the file to WebRTC.
    boolean success =
        peerConnection.startRtcEventLog(fileDescriptor.detachFd(), OUTPUT_FILE_MAX_BYTES);
    if (!success) {
      Log.e(TAG, "Failed to start RTC event log.");
      return;
    }
    state = RtcEventLogState.STARTED;
    Log.d(TAG, "RtcEventLog started.");
  }

  public void stop() {
    if (state != RtcEventLogState.STARTED) {
      Log.e(TAG, "RtcEventLog was not started.");
      return;
    }
    peerConnection.stopRtcEventLog();
    state = RtcEventLogState.STOPPED;
    Log.d(TAG, "RtcEventLog stopped.");
  }
}
