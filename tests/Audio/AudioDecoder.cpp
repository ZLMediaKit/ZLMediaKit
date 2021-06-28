#include "AudioDecoder.h"


AudioDecoder::AudioDecoder():handle_(nullptr)
        , samplerate_(0)
        , channels_(2)
        , samplebit_(16)
{

}

AudioDecoder::~AudioDecoder(){

}

bool AudioDecoder::init(unsigned char * ADTSHead ,int headLen ){
    if( handle_ == nullptr ){
        handle_ = NeAACDecOpen();
    }

    if( handle_ == nullptr ){
        return false;
    }

    long err = NeAACDecInit( handle_ , ( unsigned char *)ADTSHead , headLen ,  &samplerate_, &channels_ );

    if( err != 0 ){
        return false;
    }

    return true;
}

int  AudioDecoder::inputData( unsigned char * data,int len , unsigned char ** outBuffer ){

    NeAACDecFrameInfo frameInfo;
    *outBuffer = static_cast<unsigned char *>(NeAACDecDecode(handle_, &frameInfo, (unsigned char *) data, len));

    if( frameInfo.error != 0){
        //ErroL << NeAACDecGetErrorMessage(frameInfo.error) << endl;
        return -1;
    }

    return frameInfo.samples*frameInfo.channels;
}