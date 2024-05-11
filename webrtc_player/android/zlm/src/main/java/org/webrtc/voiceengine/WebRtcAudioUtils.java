//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package org.webrtc.voiceengine;

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Build.VERSION;

import org.webrtc.ContextUtils;
import org.webrtc.Logging;

import java.util.Arrays;
import java.util.List;

public final class WebRtcAudioUtils {
    private static final String TAG = "WebRtcAudioUtils";
    private static final String[] BLACKLISTED_OPEN_SL_ES_MODELS = new String[0];
    private static final String[] BLACKLISTED_AEC_MODELS = new String[0];
    private static final String[] BLACKLISTED_NS_MODELS = new String[0];
    private static final int DEFAULT_SAMPLE_RATE_HZ = 16000;
    private static int defaultSampleRateHz = 16000;
    private static boolean isDefaultSampleRateOverridden;
    private static boolean useWebRtcBasedAcousticEchoCanceler;
    private static boolean useWebRtcBasedNoiseSuppressor;

    public WebRtcAudioUtils() {
    }

    public static synchronized void setWebRtcBasedAcousticEchoCanceler(boolean enable) {
        useWebRtcBasedAcousticEchoCanceler = enable;
    }

    public static synchronized void setWebRtcBasedNoiseSuppressor(boolean enable) {
        useWebRtcBasedNoiseSuppressor = enable;
    }

    public static synchronized void setWebRtcBasedAutomaticGainControl(boolean enable) {
        Logging.w("WebRtcAudioUtils", "setWebRtcBasedAutomaticGainControl() is deprecated");
    }

    public static synchronized boolean useWebRtcBasedAcousticEchoCanceler() {
        if (useWebRtcBasedAcousticEchoCanceler) {
            Logging.w("WebRtcAudioUtils", "Overriding default behavior; now using WebRTC AEC!");
        }

        return useWebRtcBasedAcousticEchoCanceler;
    }

    public static synchronized boolean useWebRtcBasedNoiseSuppressor() {
        if (useWebRtcBasedNoiseSuppressor) {
            Logging.w("WebRtcAudioUtils", "Overriding default behavior; now using WebRTC NS!");
        }

        return useWebRtcBasedNoiseSuppressor;
    }

    public static synchronized boolean useWebRtcBasedAutomaticGainControl() {
        return true;
    }

    public static boolean isAcousticEchoCancelerSupported() {
        return WebRtcAudioEffects.canUseAcousticEchoCanceler();
    }

    public static boolean isNoiseSuppressorSupported() {
        return WebRtcAudioEffects.canUseNoiseSuppressor();
    }

    public static boolean isAutomaticGainControlSupported() {
        return false;
    }

    public static synchronized void setDefaultSampleRateHz(int sampleRateHz) {
        isDefaultSampleRateOverridden = true;
        defaultSampleRateHz = sampleRateHz;
    }

    public static synchronized boolean isDefaultSampleRateOverridden() {
        return isDefaultSampleRateOverridden;
    }

    public static synchronized int getDefaultSampleRateHz() {
        return defaultSampleRateHz;
    }

    public static List<String> getBlackListedModelsForAecUsage() {
        return Arrays.asList(BLACKLISTED_AEC_MODELS);
    }

    public static List<String> getBlackListedModelsForNsUsage() {
        return Arrays.asList(BLACKLISTED_NS_MODELS);
    }

    public static String getThreadInfo() {
        return "@[name=" + Thread.currentThread().getName() + ", id=" + Thread.currentThread().getId() + "]";
    }

    public static boolean runningOnEmulator() {
        return Build.HARDWARE.equals("goldfish") && Build.BRAND.startsWith("generic_");
    }

    public static boolean deviceIsBlacklistedForOpenSLESUsage() {
        List<String> blackListedModels = Arrays.asList(BLACKLISTED_OPEN_SL_ES_MODELS);
        return blackListedModels.contains(Build.MODEL);
    }

    static void logDeviceInfo(String tag) {
        Logging.d(tag, "Android SDK: " + VERSION.SDK_INT + ", Release: " + VERSION.RELEASE + ", Brand: " + Build.BRAND + ", Device: " + Build.DEVICE + ", Id: " + Build.ID + ", Hardware: " + Build.HARDWARE + ", Manufacturer: " + Build.MANUFACTURER + ", Model: " + Build.MODEL + ", Product: " + Build.PRODUCT);
    }

    static void logAudioState(String tag) {
        logDeviceInfo(tag);
        Context context = ContextUtils.getApplicationContext();
        AudioManager audioManager = (AudioManager)context.getSystemService("audio");
        logAudioStateBasic(tag, audioManager);
        logAudioStateVolume(tag, audioManager);
        logAudioDeviceInfo(tag, audioManager);
    }

    private static void logAudioStateBasic(String tag, AudioManager audioManager) {
        Logging.d(tag, "Audio State: audio mode: " + modeToString(audioManager.getMode()) + ", has mic: " + hasMicrophone() + ", mic muted: " + audioManager.isMicrophoneMute() + ", music active: " + audioManager.isMusicActive() + ", speakerphone: " + audioManager.isSpeakerphoneOn() + ", BT SCO: " + audioManager.isBluetoothScoOn());
    }

    private static boolean isVolumeFixed(AudioManager audioManager) {
        return VERSION.SDK_INT < 21 ? false : audioManager.isVolumeFixed();
    }

    private static void logAudioStateVolume(String tag, AudioManager audioManager) {
        int[] streams = new int[]{0, 3, 2, 4, 5, 1};
        Logging.d(tag, "Audio State: ");
        boolean fixedVolume = isVolumeFixed(audioManager);
        Logging.d(tag, "  fixed volume=" + fixedVolume);
        if (!fixedVolume) {
            int[] var4 = streams;
            int var5 = streams.length;

            for(int var6 = 0; var6 < var5; ++var6) {
                int stream = var4[var6];
                StringBuilder info = new StringBuilder();
                info.append("  " + streamTypeToString(stream) + ": ");
                info.append("volume=").append(audioManager.getStreamVolume(stream));
                info.append(", max=").append(audioManager.getStreamMaxVolume(stream));
                logIsStreamMute(tag, audioManager, stream, info);
                Logging.d(tag, info.toString());
            }
        }

    }

    private static void logIsStreamMute(String tag, AudioManager audioManager, int stream, StringBuilder info) {
        if (VERSION.SDK_INT >= 23) {
            info.append(", muted=").append(audioManager.isStreamMute(stream));
        }

    }

    private static void logAudioDeviceInfo(String tag, AudioManager audioManager) {
        if (VERSION.SDK_INT >= 23) {
            AudioDeviceInfo[] devices = audioManager.getDevices(3);
            if (devices.length != 0) {
                Logging.d(tag, "Audio Devices: ");
                AudioDeviceInfo[] var3 = devices;
                int var4 = devices.length;

                for(int var5 = 0; var5 < var4; ++var5) {
                    AudioDeviceInfo device = var3[var5];
                    StringBuilder info = new StringBuilder();
                    info.append("  ").append(deviceTypeToString(device.getType()));
                    info.append(device.isSource() ? "(in): " : "(out): ");
                    if (device.getChannelCounts().length > 0) {
                        info.append("channels=").append(Arrays.toString(device.getChannelCounts()));
                        info.append(", ");
                    }

                    if (device.getEncodings().length > 0) {
                        info.append("encodings=").append(Arrays.toString(device.getEncodings()));
                        info.append(", ");
                    }

                    if (device.getSampleRates().length > 0) {
                        info.append("sample rates=").append(Arrays.toString(device.getSampleRates()));
                        info.append(", ");
                    }

                    info.append("id=").append(device.getId());
                    Logging.d(tag, info.toString());
                }

            }
        }
    }

    static String modeToString(int mode) {
        switch (mode) {
            case 0:
                return "MODE_NORMAL";
            case 1:
                return "MODE_RINGTONE";
            case 2:
                return "MODE_IN_CALL";
            case 3:
                return "MODE_IN_COMMUNICATION";
            default:
                return "MODE_INVALID";
        }
    }

    private static String streamTypeToString(int stream) {
        switch (stream) {
            case 0:
                return "STREAM_VOICE_CALL";
            case 1:
                return "STREAM_SYSTEM";
            case 2:
                return "STREAM_RING";
            case 3:
                return "STREAM_MUSIC";
            case 4:
                return "STREAM_ALARM";
            case 5:
                return "STREAM_NOTIFICATION";
            default:
                return "STREAM_INVALID";
        }
    }

    private static String deviceTypeToString(int type) {
        switch (type) {
            case 0:
                return "TYPE_UNKNOWN";
            case 1:
                return "TYPE_BUILTIN_EARPIECE";
            case 2:
                return "TYPE_BUILTIN_SPEAKER";
            case 3:
                return "TYPE_WIRED_HEADSET";
            case 4:
                return "TYPE_WIRED_HEADPHONES";
            case 5:
                return "TYPE_LINE_ANALOG";
            case 6:
                return "TYPE_LINE_DIGITAL";
            case 7:
                return "TYPE_BLUETOOTH_SCO";
            case 8:
                return "TYPE_BLUETOOTH_A2DP";
            case 9:
                return "TYPE_HDMI";
            case 10:
                return "TYPE_HDMI_ARC";
            case 11:
                return "TYPE_USB_DEVICE";
            case 12:
                return "TYPE_USB_ACCESSORY";
            case 13:
                return "TYPE_DOCK";
            case 14:
                return "TYPE_FM";
            case 15:
                return "TYPE_BUILTIN_MIC";
            case 16:
                return "TYPE_FM_TUNER";
            case 17:
                return "TYPE_TV_TUNER";
            case 18:
                return "TYPE_TELEPHONY";
            case 19:
                return "TYPE_AUX_LINE";
            case 20:
                return "TYPE_IP";
            case 21:
                return "TYPE_BUS";
            case 22:
                return "TYPE_USB_HEADSET";
            default:
                return "TYPE_UNKNOWN";
        }
    }

    private static boolean hasMicrophone() {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature("android.hardware.microphone");
    }
}
