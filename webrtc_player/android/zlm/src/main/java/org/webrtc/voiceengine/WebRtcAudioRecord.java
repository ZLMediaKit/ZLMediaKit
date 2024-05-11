//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package org.webrtc.voiceengine;

import android.media.AudioRecord;
import android.os.Build.VERSION;
import android.os.Process;

import androidx.annotation.Nullable;

import org.webrtc.Logging;
import org.webrtc.ThreadUtils;

import java.nio.ByteBuffer;
import java.util.Arrays;

public class WebRtcAudioRecord {
    private static final boolean DEBUG = false;
    private static final String TAG = "WebRtcAudioRecord";
    private static final int BITS_PER_SAMPLE = 16;
    private static final int CALLBACK_BUFFER_SIZE_MS = 10;
    private static final int BUFFERS_PER_SECOND = 100;
    private static final int BUFFER_SIZE_FACTOR = 2;
    private static final long AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS = 2000L;
    private static final int DEFAULT_AUDIO_SOURCE = getDefaultAudioSource();
    private static int audioSource;
    private final long nativeAudioRecord;
    @Nullable
    private WebRtcAudioEffects effects;
    private ByteBuffer byteBuffer;
    @Nullable
    private AudioRecord audioRecord;
    @Nullable
    private AudioRecordThread audioThread;
    private static volatile boolean microphoneMute;
    private byte[] emptyBytes;
    @Nullable
    private static WebRtcAudioRecordErrorCallback errorCallback;
    @Nullable
    private static WebRtcAudioRecordSamplesReadyCallback audioSamplesReadyCallback;

    public static void setErrorCallback(WebRtcAudioRecordErrorCallback errorCallback) {
        Logging.d("WebRtcAudioRecord", "Set error callback");
        WebRtcAudioRecord.errorCallback = errorCallback;
    }

    public static void setOnAudioSamplesReady(WebRtcAudioRecordSamplesReadyCallback callback) {
        audioSamplesReadyCallback = callback;
    }

    public WebRtcAudioRecord(long nativeAudioRecord) {
        Logging.d("WebRtcAudioRecord", "ctor" + WebRtcAudioUtils.getThreadInfo());
        this.nativeAudioRecord = nativeAudioRecord;
        this.effects = WebRtcAudioEffects.create();
    }

    private boolean enableBuiltInAEC(boolean enable) {
        Logging.d("WebRtcAudioRecord", "enableBuiltInAEC(" + enable + ')');
        if (this.effects == null) {
            Logging.e("WebRtcAudioRecord", "Built-in AEC is not supported on this platform");
            return false;
        } else {
            return this.effects.setAEC(enable);
        }
    }

    private boolean enableBuiltInNS(boolean enable) {
        Logging.d("WebRtcAudioRecord", "enableBuiltInNS(" + enable + ')');
        if (this.effects == null) {
            Logging.e("WebRtcAudioRecord", "Built-in NS is not supported on this platform");
            return false;
        } else {
            return this.effects.setNS(enable);
        }
    }

    private int initRecording(int sampleRate, int channels) {
        Logging.d("WebRtcAudioRecord", "initRecording(sampleRate=" + sampleRate + ", channels=" + channels + ")");
        if (this.audioRecord != null) {
            this.reportWebRtcAudioRecordInitError("InitRecording called twice without StopRecording.");
            return -1;
        } else {
            int bytesPerFrame = channels * 2;
            int framesPerBuffer = sampleRate / 100;
            this.byteBuffer = ByteBuffer.allocateDirect(bytesPerFrame * framesPerBuffer);
            Logging.d("WebRtcAudioRecord", "byteBuffer.capacity: " + this.byteBuffer.capacity());
            this.emptyBytes = new byte[this.byteBuffer.capacity()];
            this.nativeCacheDirectBufferAddress(this.byteBuffer, this.nativeAudioRecord);
            int channelConfig = this.channelCountToConfiguration(channels);
            int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, 2);
            if (minBufferSize != -1 && minBufferSize != -2) {
                Logging.d("WebRtcAudioRecord", "AudioRecord.getMinBufferSize: " + minBufferSize);
                int bufferSizeInBytes = Math.max(2 * minBufferSize, this.byteBuffer.capacity());
                Logging.d("WebRtcAudioRecord", "bufferSizeInBytes: " + bufferSizeInBytes);

                try {
                    this.audioRecord = new AudioRecord(audioSource, sampleRate, channelConfig, 2, bufferSizeInBytes);
                } catch (IllegalArgumentException var9) {
                    this.reportWebRtcAudioRecordInitError("AudioRecord ctor error: " + var9.getMessage());
                    this.releaseAudioResources();
                    return -1;
                }

                if (this.audioRecord != null && this.audioRecord.getState() == 1) {
                    if (this.effects != null) {
                        this.effects.enable(this.audioRecord.getAudioSessionId());
                    }

                    this.logMainParameters();
                    this.logMainParametersExtended();
                    return framesPerBuffer;
                } else {
                    this.reportWebRtcAudioRecordInitError("Failed to create a new AudioRecord instance");
                    this.releaseAudioResources();
                    return -1;
                }
            } else {
                this.reportWebRtcAudioRecordInitError("AudioRecord.getMinBufferSize failed: " + minBufferSize);
                return -1;
            }
        }
    }

    private boolean startRecording() {
        Logging.d("WebRtcAudioRecord", "startRecording");
        assertTrue(this.audioRecord != null);
        assertTrue(this.audioThread == null);

        try {
            this.audioRecord.startRecording();
        } catch (IllegalStateException var2) {
            this.reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_EXCEPTION, "AudioRecord.startRecording failed: " + var2.getMessage());
            return false;
        }

        if (this.audioRecord.getRecordingState() != 3) {
            this.reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_STATE_MISMATCH, "AudioRecord.startRecording failed - incorrect state :" + this.audioRecord.getRecordingState());
            return false;
        } else {
            this.audioThread = new AudioRecordThread("AudioRecordJavaThread");
            this.audioThread.start();
            return true;
        }
    }

    private boolean stopRecording() {
        Logging.d("WebRtcAudioRecord", "stopRecording");
        assertTrue(this.audioThread != null);
        this.audioThread.stopThread();
        if (!ThreadUtils.joinUninterruptibly(this.audioThread, 2000L)) {
            Logging.e("WebRtcAudioRecord", "Join of AudioRecordJavaThread timed out");
            WebRtcAudioUtils.logAudioState("WebRtcAudioRecord");
        }

        this.audioThread = null;
        if (this.effects != null) {
            this.effects.release();
        }

        this.releaseAudioResources();
        return true;
    }

    private void logMainParameters() {
        Logging.d("WebRtcAudioRecord", "AudioRecord: session ID: " + this.audioRecord.getAudioSessionId() + ", channels: " + this.audioRecord.getChannelCount() + ", sample rate: " + this.audioRecord.getSampleRate());
    }

    private void logMainParametersExtended() {
        if (VERSION.SDK_INT >= 23) {
            Logging.d("WebRtcAudioRecord", "AudioRecord: buffer size in frames: " + this.audioRecord.getBufferSizeInFrames());
        }

    }

    private static void assertTrue(boolean condition) {
        if (!condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    private int channelCountToConfiguration(int channels) {
        return channels == 1 ? 16 : 12;
    }

    private native void nativeCacheDirectBufferAddress(ByteBuffer var1, long var2);

    private native void nativeDataIsRecorded(int var1, long var2);

    public static synchronized void setAudioSource(int source) {
        Logging.w("WebRtcAudioRecord", "Audio source is changed from: " + audioSource + " to " + source);
        audioSource = source;
    }

    private static int getDefaultAudioSource() {
        return 7;
    }

    public static void setMicrophoneMute(boolean mute) {
        Logging.w("WebRtcAudioRecord", "setMicrophoneMute(" + mute + ")");
        microphoneMute = mute;
    }

    private void releaseAudioResources() {
        Logging.d("WebRtcAudioRecord", "releaseAudioResources");
        if (this.audioRecord != null) {
            this.audioRecord.release();
            this.audioRecord = null;
        }

    }

    private void reportWebRtcAudioRecordInitError(String errorMessage) {
        Logging.e("WebRtcAudioRecord", "Init recording error: " + errorMessage);
        WebRtcAudioUtils.logAudioState("WebRtcAudioRecord");
        if (errorCallback != null) {
            errorCallback.onWebRtcAudioRecordInitError(errorMessage);
        }

    }

    private void reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode errorCode, String errorMessage) {
        Logging.e("WebRtcAudioRecord", "Start recording error: " + errorCode + ". " + errorMessage);
        WebRtcAudioUtils.logAudioState("WebRtcAudioRecord");
        if (errorCallback != null) {
            errorCallback.onWebRtcAudioRecordStartError(errorCode, errorMessage);
        }

    }

    private void reportWebRtcAudioRecordError(String errorMessage) {
        Logging.e("WebRtcAudioRecord", "Run-time recording error: " + errorMessage);
        WebRtcAudioUtils.logAudioState("WebRtcAudioRecord");
        if (errorCallback != null) {
            errorCallback.onWebRtcAudioRecordError(errorMessage);
        }

    }

    static {
        audioSource = DEFAULT_AUDIO_SOURCE;
    }

    private class AudioRecordThread extends Thread {
        private volatile boolean keepAlive = true;

        public AudioRecordThread(String name) {
            super(name);
        }

        public void run() {
            Process.setThreadPriority(-19);
            Logging.d("WebRtcAudioRecord", "AudioRecordThread" + WebRtcAudioUtils.getThreadInfo());
            WebRtcAudioRecord.assertTrue(WebRtcAudioRecord.this.audioRecord.getRecordingState() == 3);
            long lastTime = System.nanoTime();

            while(this.keepAlive) {
                int bytesRead = WebRtcAudioRecord.this.audioRecord.read(WebRtcAudioRecord.this.byteBuffer, WebRtcAudioRecord.this.byteBuffer.capacity());
                if (bytesRead == WebRtcAudioRecord.this.byteBuffer.capacity()) {
                    if (WebRtcAudioRecord.microphoneMute) {
                        WebRtcAudioRecord.this.byteBuffer.clear();
                        WebRtcAudioRecord.this.byteBuffer.put(WebRtcAudioRecord.this.emptyBytes);
                    }

                    if (this.keepAlive) {
                        WebRtcAudioRecord.this.nativeDataIsRecorded(bytesRead, WebRtcAudioRecord.this.nativeAudioRecord);
                    }

                    if (WebRtcAudioRecord.audioSamplesReadyCallback != null) {
                        byte[] data = Arrays.copyOf(WebRtcAudioRecord.this.byteBuffer.array(), WebRtcAudioRecord.this.byteBuffer.capacity());
                        WebRtcAudioRecord.audioSamplesReadyCallback.onWebRtcAudioRecordSamplesReady(new AudioSamples(WebRtcAudioRecord.this.audioRecord, data));
                    }
                } else {
                    String errorMessage = "AudioRecord.read failed: " + bytesRead;
                    Logging.e("WebRtcAudioRecord", errorMessage);
                    if (bytesRead == -3) {
                        this.keepAlive = false;
                        WebRtcAudioRecord.this.reportWebRtcAudioRecordError(errorMessage);
                    }
                }
            }

            try {
                if (WebRtcAudioRecord.this.audioRecord != null) {
                    WebRtcAudioRecord.this.audioRecord.stop();
                }
            } catch (IllegalStateException var5) {
                Logging.e("WebRtcAudioRecord", "AudioRecord.stop failed: " + var5.getMessage());
            }

        }

        public void stopThread() {
            Logging.d("WebRtcAudioRecord", "stopThread");
            this.keepAlive = false;
        }
    }

    public interface WebRtcAudioRecordSamplesReadyCallback {
        void onWebRtcAudioRecordSamplesReady(AudioSamples var1);
    }

    public static class AudioSamples {
        private final int audioFormat;
        private final int channelCount;
        private final int sampleRate;
        private final byte[] data;

        private AudioSamples(AudioRecord audioRecord, byte[] data) {
            this.audioFormat = audioRecord.getAudioFormat();
            this.channelCount = audioRecord.getChannelCount();
            this.sampleRate = audioRecord.getSampleRate();
            this.data = data;
        }

        public int getAudioFormat() {
            return this.audioFormat;
        }

        public int getChannelCount() {
            return this.channelCount;
        }

        public int getSampleRate() {
            return this.sampleRate;
        }

        public byte[] getData() {
            return this.data;
        }
    }

    public interface WebRtcAudioRecordErrorCallback {
        void onWebRtcAudioRecordInitError(String var1);

        void onWebRtcAudioRecordStartError(AudioRecordStartErrorCode var1, String var2);

        void onWebRtcAudioRecordError(String var1);
    }

    public static enum AudioRecordStartErrorCode {
        AUDIO_RECORD_START_EXCEPTION,
        AUDIO_RECORD_START_STATE_MISMATCH;

        private AudioRecordStartErrorCode() {
        }
    }
}
