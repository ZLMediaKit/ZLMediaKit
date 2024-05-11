//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package org.webrtc.voiceengine;

import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Build.VERSION;

import androidx.annotation.Nullable;

import org.webrtc.ContextUtils;
import org.webrtc.Logging;

import java.util.Timer;
import java.util.TimerTask;

public class WebRtcAudioManager {
    private static final boolean DEBUG = false;
    private static final String TAG = "WebRtcAudioManager";
    private static final boolean blacklistDeviceForAAudioUsage = true;
    private static boolean useStereoOutput;
    private static boolean useStereoInput;
    private static boolean blacklistDeviceForOpenSLESUsage;
    private static boolean blacklistDeviceForOpenSLESUsageIsOverridden;
    private static final int BITS_PER_SAMPLE = 16;
    private static final int DEFAULT_FRAME_PER_BUFFER = 256;
    private final long nativeAudioManager;
    private final AudioManager audioManager;
    private boolean initialized;
    private int nativeSampleRate;
    private int nativeChannels;
    private boolean hardwareAEC;
    private boolean hardwareAGC;
    private boolean hardwareNS;
    private boolean lowLatencyOutput;
    private boolean lowLatencyInput;
    private boolean proAudio;
    private boolean aAudio;
    private int sampleRate;
    private int outputChannels;
    private int inputChannels;
    private int outputBufferSize;
    private int inputBufferSize;
    private final VolumeLogger volumeLogger;

    public static synchronized void setBlacklistDeviceForOpenSLESUsage(boolean enable) {
        blacklistDeviceForOpenSLESUsageIsOverridden = true;
        blacklistDeviceForOpenSLESUsage = enable;
    }

    public static synchronized void setStereoOutput(boolean enable) {
        Logging.w("WebRtcAudioManager", "Overriding default output behavior: setStereoOutput(" + enable + ')');
        useStereoOutput = enable;
    }

    public static synchronized void setStereoInput(boolean enable) {
        Logging.w("WebRtcAudioManager", "Overriding default input behavior: setStereoInput(" + enable + ')');
        useStereoInput = enable;
    }

    public static synchronized boolean getStereoOutput() {
        return useStereoOutput;
    }

    public static synchronized boolean getStereoInput() {
        return useStereoInput;
    }

    WebRtcAudioManager(long nativeAudioManager) {
        Logging.d("WebRtcAudioManager", "ctor" + WebRtcAudioUtils.getThreadInfo());
        this.nativeAudioManager = nativeAudioManager;
        this.audioManager = (AudioManager)ContextUtils.getApplicationContext().getSystemService("audio");
        this.volumeLogger = new VolumeLogger(this.audioManager);
        this.storeAudioParameters();
        this.nativeCacheAudioParameters(this.sampleRate, this.outputChannels, this.inputChannels, this.hardwareAEC, this.hardwareAGC, this.hardwareNS, this.lowLatencyOutput, this.lowLatencyInput, this.proAudio, this.aAudio, this.outputBufferSize, this.inputBufferSize, nativeAudioManager);
        WebRtcAudioUtils.logAudioState("WebRtcAudioManager");
    }

    private boolean init() {
        Logging.d("WebRtcAudioManager", "init" + WebRtcAudioUtils.getThreadInfo());
        if (this.initialized) {
            return true;
        } else {
            Logging.d("WebRtcAudioManager", "audio mode is: " + WebRtcAudioUtils.modeToString(this.audioManager.getMode()));
            this.initialized = true;
            this.volumeLogger.start();
            return true;
        }
    }

    private void dispose() {
        Logging.d("WebRtcAudioManager", "dispose" + WebRtcAudioUtils.getThreadInfo());
        if (this.initialized) {
            this.volumeLogger.stop();
        }
    }

    private boolean isCommunicationModeEnabled() {
        return this.audioManager.getMode() == 3;
    }

    private boolean isDeviceBlacklistedForOpenSLESUsage() {
        boolean blacklisted = blacklistDeviceForOpenSLESUsageIsOverridden ? blacklistDeviceForOpenSLESUsage : WebRtcAudioUtils.deviceIsBlacklistedForOpenSLESUsage();
        if (blacklisted) {
            Logging.d("WebRtcAudioManager", Build.MODEL + " is blacklisted for OpenSL ES usage!");
        }

        return blacklisted;
    }

    private void storeAudioParameters() {
        this.outputChannels = getStereoOutput() ? 2 : 1;
        this.inputChannels = getStereoInput() ? 2 : 1;
        this.sampleRate = this.getNativeOutputSampleRate();
        this.hardwareAEC = isAcousticEchoCancelerSupported();
        this.hardwareAGC = false;
        this.hardwareNS = isNoiseSuppressorSupported();
        this.lowLatencyOutput = this.isLowLatencyOutputSupported();
        this.lowLatencyInput = this.isLowLatencyInputSupported();
        this.proAudio = this.isProAudioSupported();
        this.aAudio = this.isAAudioSupported();
        this.outputBufferSize = this.lowLatencyOutput ? this.getLowLatencyOutputFramesPerBuffer() : getMinOutputFrameSize(this.sampleRate, this.outputChannels);
        this.inputBufferSize = this.lowLatencyInput ? this.getLowLatencyInputFramesPerBuffer() : getMinInputFrameSize(this.sampleRate, this.inputChannels);
    }

    private boolean hasEarpiece() {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature("android.hardware.telephony");
    }

    private boolean isLowLatencyOutputSupported() {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature("android.hardware.audio.low_latency");
    }

    public boolean isLowLatencyInputSupported() {
        return VERSION.SDK_INT >= 21 && this.isLowLatencyOutputSupported();
    }

    private boolean isProAudioSupported() {
        return VERSION.SDK_INT >= 23 && ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature("android.hardware.audio.pro");
    }

    private boolean isAAudioSupported() {
        Logging.w("WebRtcAudioManager", "AAudio support is currently disabled on all devices!");
        return false;
    }

    private int getNativeOutputSampleRate() {
        if (WebRtcAudioUtils.runningOnEmulator()) {
            Logging.d("WebRtcAudioManager", "Running emulator, overriding sample rate to 8 kHz.");
            return 8000;
        } else if (WebRtcAudioUtils.isDefaultSampleRateOverridden()) {
            Logging.d("WebRtcAudioManager", "Default sample rate is overriden to " + WebRtcAudioUtils.getDefaultSampleRateHz() + " Hz");
            return WebRtcAudioUtils.getDefaultSampleRateHz();
        } else {
            int sampleRateHz = this.getSampleRateForApiLevel();
            Logging.d("WebRtcAudioManager", "Sample rate is set to " + sampleRateHz + " Hz");
            return sampleRateHz;
        }
    }

    private int getSampleRateForApiLevel() {
        if (VERSION.SDK_INT < 17) {
            return WebRtcAudioUtils.getDefaultSampleRateHz();
        } else {
            String sampleRateString = this.audioManager.getProperty("android.media.property.OUTPUT_SAMPLE_RATE");
            return sampleRateString == null ? WebRtcAudioUtils.getDefaultSampleRateHz() : Integer.parseInt(sampleRateString);
        }
    }

    private int getLowLatencyOutputFramesPerBuffer() {
        assertTrue(this.isLowLatencyOutputSupported());
        if (VERSION.SDK_INT < 17) {
            return 256;
        } else {
            String framesPerBuffer = this.audioManager.getProperty("android.media.property.OUTPUT_FRAMES_PER_BUFFER");
            return framesPerBuffer == null ? 256 : Integer.parseInt(framesPerBuffer);
        }
    }

    private static boolean isAcousticEchoCancelerSupported() {
        return WebRtcAudioEffects.canUseAcousticEchoCanceler();
    }

    private static boolean isNoiseSuppressorSupported() {
        return WebRtcAudioEffects.canUseNoiseSuppressor();
    }

    private static int getMinOutputFrameSize(int sampleRateInHz, int numChannels) {
        int bytesPerFrame = numChannels * 2;
        int channelConfig = numChannels == 1 ? 4 : 12;
        return AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, 2) / bytesPerFrame;
    }

    private int getLowLatencyInputFramesPerBuffer() {
        assertTrue(this.isLowLatencyInputSupported());
        return this.getLowLatencyOutputFramesPerBuffer();
    }

    private static int getMinInputFrameSize(int sampleRateInHz, int numChannels) {
        int bytesPerFrame = numChannels * 2;
        int channelConfig = numChannels == 1 ? 16 : 12;
        return AudioRecord.getMinBufferSize(sampleRateInHz, channelConfig, 2) / bytesPerFrame;
    }

    private static void assertTrue(boolean condition) {
        if (!condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    private native void nativeCacheAudioParameters(int var1, int var2, int var3, boolean var4, boolean var5, boolean var6, boolean var7, boolean var8, boolean var9, boolean var10, int var11, int var12, long var13);

    private static class VolumeLogger {
        private static final String THREAD_NAME = "WebRtcVolumeLevelLoggerThread";
        private static final int TIMER_PERIOD_IN_SECONDS = 30;
        private final AudioManager audioManager;
        @Nullable
        private Timer timer;

        public VolumeLogger(AudioManager audioManager) {
            this.audioManager = audioManager;
        }

        public void start() {
            this.timer = new Timer("WebRtcVolumeLevelLoggerThread");
            this.timer.schedule(new LogVolumeTask(this.audioManager.getStreamMaxVolume(2), this.audioManager.getStreamMaxVolume(0)), 0L, 30000L);
        }

        private void stop() {
            if (this.timer != null) {
                this.timer.cancel();
                this.timer = null;
            }

        }

        private class LogVolumeTask extends TimerTask {
            private final int maxRingVolume;
            private final int maxVoiceCallVolume;

            LogVolumeTask(int maxRingVolume, int maxVoiceCallVolume) {
                this.maxRingVolume = maxRingVolume;
                this.maxVoiceCallVolume = maxVoiceCallVolume;
            }

            public void run() {
                int mode = VolumeLogger.this.audioManager.getMode();
                if (mode == 1) {
                    Logging.d("WebRtcAudioManager", "STREAM_RING stream volume: " + VolumeLogger.this.audioManager.getStreamVolume(2) + " (max=" + this.maxRingVolume + ")");
                } else if (mode == 3) {
                    Logging.d("WebRtcAudioManager", "VOICE_CALL stream volume: " + VolumeLogger.this.audioManager.getStreamVolume(0) + " (max=" + this.maxVoiceCallVolume + ")");
                }

            }
        }
    }
}
