package com.zlm.rtc.client;

import org.webrtc.PeerConnection;
import org.webrtc.VideoTrack;

import java.math.BigInteger;

public class ZLMConnection {
    public BigInteger handleId;
    public PeerConnection peerConnection;
    public PeerConnectionClient.SDPObserver sdpObserver;
    public VideoTrack videoTrack;
    public boolean type;
}
