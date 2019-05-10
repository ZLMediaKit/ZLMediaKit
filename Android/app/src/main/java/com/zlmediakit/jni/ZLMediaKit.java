package com.zlmediakit.jni;

public class ZLMediaKit {
    static public class MediaFrame{

        /**
         * 返回解码时间戳，单位毫秒
         */
        public int dts;

        /**
         * 返回显示时间戳，单位毫秒
         */
        public int pts;

        /**
         * 前缀长度，譬如264前缀为0x00 00 00 01,那么前缀长度就是4
         * aac前缀则为7个字节
         */
        public int prefixSize;

        /**
         * 返回是否为关键帧
         */
        public boolean keyFrame;

        /**
         * 音视频数据
         */
        public byte[] data;

        /**
         * 是音频还是视频
         * typedef enum {
         *     TrackInvalid = -1,
         *     TrackVideo = 0,
         *     TrackAudio,
         *     TrackTitle,
         *     TrackMax = 0x7FFF
         * } TrackType;
         */
        public int trackType;


        /**
         * 编码类型
         * typedef enum {
         *     CodecInvalid = -1,
         *     CodecH264 = 0,
         *     CodecH265,
         *     CodecAAC,
         *     CodecMax = 0x7FFF
         * } CodecId;
         */
        public int codecId;
    }

    static public interface MediaPlayerCallBack{
        void onPlayResult(int code,String msg);
        void onShutdown(int code,String msg);
        void onData(MediaFrame frame);
    };


    static public class MediaPlayer{
        private long _ptr;
        private MediaPlayerCallBack _callback;
        public MediaPlayer(String url,MediaPlayerCallBack callBack){
            _callback = callBack;
            _ptr = createMediaPlayer(url,callBack);
        }
        public void release(){
            if(_ptr != 0){
                releaseMediaPlayer(_ptr);
                _ptr = 0;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            super.finalize();
            release();
        }
    }

    static public native boolean startDemo(String sd_path);
    static public native void releaseMediaPlayer(long ptr);
    static public native long createMediaPlayer(String url,MediaPlayerCallBack callback);

    static {
        System.loadLibrary("zlmediakit_jni");
    }
}
