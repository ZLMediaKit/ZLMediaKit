
#include "AudioPlayer.h"

#include "Util/util.h"
#include "Util/logger.h"

using namespace toolkit;

AudioPlayer::AudioPlayer( int sampleRate ,int  channels ,int  sampleBit):
         sampleRate_(sampleRate)
        ,channels_(channels)
        ,sampleBit_(sampleBit)
{

}

AudioPlayer::~AudioPlayer(){

}

int AudioPlayer::getPCMData(char *buf, int bufsize) {

    if( buffer_->IsEmpty() ){
        WarnL << "Audio Data Not Ready!";
        return 0;
    }

    PcmPacket pcm;
    buffer_->Pop(pcm);

    if( bufsize > pcm.data.size() ){
        bufsize = pcm.data.size() ;
    }

    memcpy( buf , pcm.data.data()  , bufsize );
    return pcm.data.size();
}

bool AudioPlayer::inputFrame( Frame::Ptr frame ){
    unsigned char *pSampleBuffer = nullptr ;

    if( decoder_ == nullptr ){
        decoder_ = new AudioDecoder;
        bool ret = decoder_->init( (unsigned char *)frame->data() );
        buffer_ = new xRingBuffer<PcmPacket> ( 16 );
    }

    if( audioSrc_ == nullptr ){
        audioSrc_ = new AudioSRC(this) ;
        SDLAudioDevice::Instance().addChannel( audioSrc_ );
    }

    int size = decoder_->inputData( (unsigned char *)frame->data() ,frame->size(), &pSampleBuffer );

    if( size < 0 ){
        ErrorL << "Audio Decode Error!!" << endl;
    }else{
        buf_.append((char*)pSampleBuffer , size);
    }

    size_t offset = 0 ;
    int i = 0;
    int ms = 1000 *  reqSize_ / ( decoder_->getChannels()*decoder_->getSampleRate()*decoder_->getSampleBit() / 8);

    while( buf_.size() - offset > reqSize_  ){
        PcmPacket pcm;
        pcm.data.assign( buf_.data()  + offset , reqSize_ );
        pcm.timeStmp = i*ms + frame->dts();
        buffer_->Push(std::move(pcm));
        ++i;
        offset += reqSize_;
    }

    if(offset){
        buf_.erase(0,offset );
    }

    return true;
}