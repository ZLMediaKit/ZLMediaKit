#ifndef ZLMEDIAKIT_G711TOAAC_H
#define ZLMEDIAKIT_G711TOAAC_H


#include "AAC.h"

namespace mediakit{

/**
 * aac音频通道
 * AAC audio channel
 
 * [AUTO-TRANSLATED:0d58b638]
 */
class G711ToAACTrack : public AACTrack {

    void* _AACEncoderHandle = nullptr;

    unsigned char	_ucAudioCodec;			// Law_uLaw  Law_ALaw Law_PCM16 Law_G726
	unsigned char	_ucAudioChannel;			//1
	unsigned int	_u32AudioSamplerate;		//8000
	unsigned int	_u32PCMBitSize;			//16

private:
    Track::Ptr clone() const override;
public:
    using Ptr = std::shared_ptr<G711ToAACTrack>;

    G711ToAACTrack() = default;

    G711ToAACTrack(int sample_rate, int channels, int sample_bit);
    
    G711ToAACTrack(const std::string &aac_cfg);

    ~G711ToAACTrack();

    CodecId getCodecId() const override;

    bool inputFrame(const Frame::Ptr &frame) override;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_G711TOAAC_H