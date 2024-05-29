package com.zlm.rtc.client;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;

import org.webrtc.EglBase;
import org.webrtc.GlRectDrawer;
import org.webrtc.VideoFrame;
import org.webrtc.VideoFrameDrawer;
import org.webrtc.VideoSink;
import org.webrtc.audio.JavaAudioDeviceModule;
import org.webrtc.voiceengine.WebRtcAudioRecord;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * @author leo
 * @version 1.0
 * @className VideoFileRenderer
 * @description TODO
 * @date 2022/9/27 11:12
 **/

class VideoFileRecorder implements VideoSink, JavaAudioDeviceModule.SamplesReadyCallback, WebRtcAudioRecord.WebRtcAudioRecordSamplesReadyCallback {
    private static final String TAG = "VideoFileRenderer";


    private String mOutFilePath;
    private HandlerThread renderThread;
    private Handler renderThreadHandler;
    private HandlerThread audioThread;
    private Handler audioThreadHandler;
    private int outputFileWidth = -1;
    private int outputFileHeight = -1;
    private ByteBuffer[] encoderOutputBuffers;
    private ByteBuffer[] audioInputBuffers;
    private ByteBuffer[] audioOutputBuffers;
    private EglBase eglBase;
    private EglBase.Context sharedContext;
    private VideoFrameDrawer frameDrawer;

    // TODO: these ought to be configurable as well
    private static final String MIME_TYPE = "video/avc";    // H.264 Advanced Video Coding
    private static final int FRAME_RATE = 15;               // 30fps
    private static final int IFRAME_INTERVAL = 5;           // 5 seconds between I-frames

    private MediaMuxer mediaMuxer;
    private MediaCodec encoder;
    private MediaCodec.BufferInfo bufferInfo, audioBufferInfo;
    private int trackIndex = -1;
    private int audioTrackIndex;
    private boolean withAudio = false;
    private boolean isEnableRecord = false;

    private GlRectDrawer drawer;
    private Surface surface;
    private MediaCodec audioEncoder;

    VideoFileRecorder() {
        Log.i(TAG, "=====================>VideoFileRecorder");
        renderThread = new HandlerThread(TAG + "RenderThread");
        renderThread.start();
        renderThreadHandler = new Handler(renderThread.getLooper());
        isEnableRecord = false;
    }


    public void start(String outputFile, final EglBase.Context sharedContext, boolean withAudio) throws IOException {
        Log.i(TAG, "=====================>start");
        isEnableRecord = true;
        trackIndex = -1;
        outputFileWidth = -1;
        this.sharedContext = sharedContext;
        this.withAudio = withAudio;

        if (this.withAudio) {
            audioThread = new HandlerThread(TAG + "AudioThread");
            audioThread.start();
            audioThreadHandler = new Handler(audioThread.getLooper());
        } else {
            audioThread = null;
            audioThreadHandler = null;
        }
        bufferInfo = new MediaCodec.BufferInfo();
        this.mOutFilePath = outputFile;
        mediaMuxer = new MediaMuxer(outputFile,
                MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        audioTrackIndex = this.withAudio ? -1 : 0;
    }


    /**
     * Release all resources. All already posted frames will be rendered first.
     */
    public void release() {
        isEnableRecord = false;
        if (audioThreadHandler != null) {
            audioThreadHandler.post(() -> {
                if (audioEncoder != null) {
                    audioEncoder.stop();
                    audioEncoder.release();
                }
                audioThread.quit();
            });
        }

        if (renderThreadHandler != null) {
            renderThreadHandler.post(() -> {
                if (encoder != null) {
                    encoder.stop();
                    encoder.release();
                }
                eglBase.release();
                mediaMuxer.stop();
                mediaMuxer.release();
                renderThread.quit();

            });
        }
    }

    public boolean isRecording() {
        return isEnableRecord;
    }

    private void initVideoEncoder() {
        MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE, outputFileWidth, outputFileHeight);

        // Set some properties.  Failing to specify some of these can cause the MediaCodec
        // configure() call to throw an unhelpful exception.
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_BIT_RATE, 6000000);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, FRAME_RATE);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);

        // Create a MediaCodec encoder, and configure it with our format.  Get a Surface
        // we can use for input and wrap it with a class that handles the EGL work.
        try {
            encoder = MediaCodec.createEncoderByType(MIME_TYPE);
            encoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            renderThreadHandler.post(() -> {
                eglBase = EglBase.create(sharedContext, EglBase.CONFIG_RECORDABLE);
                surface = encoder.createInputSurface();
                eglBase.createSurface(surface);
                eglBase.makeCurrent();
                drawer = new GlRectDrawer();
            });
        } catch (Exception e) {
            Log.wtf(TAG, e);
        }
    }

    @Override
    public void onFrame(VideoFrame frame) {
        if (!isEnableRecord) return;
        Log.e(TAG, "onFrame");
        frame.retain();
        if (outputFileWidth == -1) {
            outputFileWidth = frame.getRotatedWidth();
            outputFileHeight = frame.getRotatedHeight();
            initVideoEncoder();
        }
        renderThreadHandler.post(() -> renderFrameOnRenderThread(frame));
    }

    private void renderFrameOnRenderThread(VideoFrame frame) {
        if (frameDrawer == null) {
            frameDrawer = new VideoFrameDrawer();
        }
        frameDrawer.drawFrame(frame, drawer, null, 0, 0, outputFileWidth, outputFileHeight);
        frame.release();
        drainEncoder();
        eglBase.swapBuffers();
    }


    private boolean encoderStarted = false;
    private volatile boolean muxerStarted = false;
    private long videoFrameStart = 0;

    private void drainEncoder() {
        if (!encoderStarted) {
            encoder.start();
            encoderOutputBuffers = encoder.getOutputBuffers();
            encoderStarted = true;
            return;
        }
        while (true) {
            try {
                int encoderStatus = encoder.dequeueOutputBuffer(bufferInfo, 10000);
                if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                    break;
                } else if (encoderStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                    // not expected for an encoder
                    encoderOutputBuffers = encoder.getOutputBuffers();
                    Log.e(TAG, "encoder output buffers changed");
                } else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    // not expected for an encoder
                    MediaFormat newFormat = encoder.getOutputFormat();

                    Log.e(TAG, "encoder output format changed: " + newFormat);
                    trackIndex = mediaMuxer.addTrack(newFormat);
                    if (audioTrackIndex != -1 && !muxerStarted) {
                        mediaMuxer.start();
                        Log.e(TAG, "mediaMuxer start");
                        muxerStarted = true;
                    }
                    if (!muxerStarted)
                        break;
                } else if (encoderStatus < 0) {
                    Log.e(TAG, "unexpected result fr om encoder.dequeueOutputBuffer: " + encoderStatus);
                } else { // encoderStatus >= 0
                    try {
                        ByteBuffer encodedData = encoderOutputBuffers[encoderStatus];
                        if (encodedData == null) {
                            Log.e(TAG, "encoderOutputBuffer " + encoderStatus + " was null");
                            break;
                        }
                        // It's usually necessary to adjust the ByteBuffer values to match BufferInfo.
                        encodedData.position(bufferInfo.offset);
                        encodedData.limit(bufferInfo.offset + bufferInfo.size);
                        if (videoFrameStart == 0 && bufferInfo.presentationTimeUs != 0) {
                            videoFrameStart = bufferInfo.presentationTimeUs;
                        }
                        bufferInfo.presentationTimeUs -= videoFrameStart;
                        if (muxerStarted)
                            mediaMuxer.writeSampleData(trackIndex, encodedData, bufferInfo);
                        isEnableRecord = isEnableRecord && (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) == 0;
                        encoder.releaseOutputBuffer(encoderStatus, false);
                        if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                            break;
                        }
                    } catch (Exception e) {
                        Log.wtf(TAG, e);
                        break;
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "encoder error, " + e);
                break;
            }
        }
    }

    private long presTime = 0L;

    private void drainAudio() {
        if (audioBufferInfo == null)
            audioBufferInfo = new MediaCodec.BufferInfo();
        while (true) {
            int encoderStatus = audioEncoder.dequeueOutputBuffer(audioBufferInfo, 10000);
            if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                break;
            } else if (encoderStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                // not expected for an encoder
                audioOutputBuffers = audioEncoder.getOutputBuffers();
                Log.w(TAG, "encoder output buffers changed");
            } else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                // not expected for an encoder
                MediaFormat newFormat = audioEncoder.getOutputFormat();

                Log.w(TAG, "encoder output format changed: " + newFormat);
                audioTrackIndex = mediaMuxer.addTrack(newFormat);
                if (trackIndex != -1 && !muxerStarted) {
                    mediaMuxer.start();
                    muxerStarted = true;
                }
                if (!muxerStarted)
                    break;
            } else if (encoderStatus < 0) {
                Log.e(TAG, "unexpected result fr om encoder.dequeueOutputBuffer: " + encoderStatus);
            } else { // encoderStatus >= 0
                try {
                    ByteBuffer encodedData = audioOutputBuffers[encoderStatus];
                    if (encodedData == null) {
                        Log.e(TAG, "encoderOutputBuffer " + encoderStatus + " was null");
                        break;
                    }
                    // It's usually necessary to adjust the ByteBuffer values to match BufferInfo.
                    encodedData.position(audioBufferInfo.offset);
                    encodedData.limit(audioBufferInfo.offset + audioBufferInfo.size);
                    if (muxerStarted)
                        mediaMuxer.writeSampleData(audioTrackIndex, encodedData, audioBufferInfo);
                    isEnableRecord = isEnableRecord && (audioBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) == 0;
                    audioEncoder.releaseOutputBuffer(encoderStatus, false);
                    if ((audioBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        break;
                    }
                } catch (Exception e) {
                    Log.wtf(TAG, e);
                    break;
                }
            }
        }
    }

    @Override
    public void onWebRtcAudioRecordSamplesReady(JavaAudioDeviceModule.AudioSamples audioSamples) {
        if (!isEnableRecord) return;
        Log.e(TAG, "onWebRtcAudioRecordSamplesReady " + isEnableRecord);
        if (!isEnableRecord)
            return;
        if (audioThreadHandler != null) {
            audioThreadHandler.post(() -> {
                if (audioEncoder == null) try {
                    audioEncoder = MediaCodec.createEncoderByType("audio/mp4a-latm");
                    MediaFormat format = new MediaFormat();
                    format.setString(MediaFormat.KEY_MIME, "audio/mp4a-latm");
                    format.setInteger(MediaFormat.KEY_CHANNEL_COUNT, audioSamples.getChannelCount());
                    format.setInteger(MediaFormat.KEY_SAMPLE_RATE, audioSamples.getSampleRate());
                    format.setInteger(MediaFormat.KEY_BIT_RATE, 64 * 1024);
                    format.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
                    audioEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                    audioEncoder.start();
                    audioInputBuffers = audioEncoder.getInputBuffers();
                    audioOutputBuffers = audioEncoder.getOutputBuffers();
                } catch (IOException exception) {
                    Log.wtf(TAG, exception);
                }
                int bufferIndex = audioEncoder.dequeueInputBuffer(0);
                if (bufferIndex >= 0) {
                    ByteBuffer buffer = audioInputBuffers[bufferIndex];
                    buffer.clear();
                    byte[] data = audioSamples.getData();
                    buffer.put(data);
                    audioEncoder.queueInputBuffer(bufferIndex, 0, data.length, presTime, 0);
                    presTime += data.length * 125 / 12; // 1000000 microseconds / 48000hz / 2 bytes
                }
                drainAudio();
            });
        }

    }

    @Override
    public void onWebRtcAudioRecordSamplesReady(WebRtcAudioRecord.AudioSamples samples) {
        onWebRtcAudioRecordSamplesReady(new JavaAudioDeviceModule.AudioSamples(samples.getAudioFormat(),
                samples.getChannelCount(), samples.getSampleRate(), samples.getData()));
    }
}