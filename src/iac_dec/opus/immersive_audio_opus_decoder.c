#include "bitstreamrw.h"
#include "immersive_audio_codec.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_types.h"
#include "opus_multistream2_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAOPUS"

typedef struct IAOpusContext {
    void*   dec;
} IAOpusContext;

/**
 * Opus Specific Box
 *
 *  class ChannelMappingTable (unsigned int(8) OutputChannelCount){
 *      unsigned int(8) StreamCount;
 *      unsigned int(8) CoupledCount;
 *      unsigned int(8 * OutputChannelCount) ChannelMapping;
 *  }
 *
 ******************************************************************************
 *
 *  aligned(8) class OpusSpecificBox extends Box('dOps'){
 *      unsigned int(8) Version;
 *      unsigned int(8) OutputChannelCount;
 *      unsigned int(16) PreSkip;
 *      unsigned int(32) InputSampleRate;
 *      signed int(16) OutputGain;
 *      unsigned int(8) ChannelMappingFamily;
 *      if (ChannelMappingFamily != 0) {
 *          ChannelMappingTable(OutputChannelCount);
 *      }
 *  }
 *
 * */
static IAErrCode ia_opus_init (IACodecContext *ths)
{
    IAOpusContext *ctx = (IAOpusContext *)ths->priv;
    IAErrCode ec = IA_OK;
    int ret = 0;
    uint8_t *config = ths->cspec;

    if (!ths->cspec || ths->clen <= 0)
        return IA_ERR_BAD_ARG;

    if (ths->flags & IA_FLAG_CODEC_CONFIG_ISOBMFF)
        config += 8;

    ths->delay = get_uint16be (config, 2);
    ths->sample_rate = get_uint32be (config, 4);
    ths->channel_mapping_family = get_uint8 (config, 10);
    // TODO - ~IA_FLAG_SUBSTREAM_CODEC_SPECIFIC
    // if (~ths->flags & IA_FLAG_SUBSTREAM_CODEC_SPECIFIC) { }


    if (ths->flags & IA_FLAG_SUBSTREAM_CODEC_SPECIFIC) {
        ctx->dec = opus_multistream2_decoder_create(ths->sample_rate,
                ths->streams, ths->coupled_streams,
                AUDIO_FRAME_FLOAT | AUDIO_FRAME_PLANE, &ret);
        if (!ctx->dec) {
            ec = IA_ERR_INVALID_STATE;
        }
    }

    return ec;
}

static int ia_opus_decode_list (IACodecContext *ths,
                                uint8_t *buf[], uint32_t len[],
                                uint32_t count,
                                void* pcm, const uint32_t frame_size)
{
    IAOpusContext *ctx = (IAOpusContext *)ths->priv;
    OpusMS2Decoder *dec = (OpusMS2Decoder *)ctx->dec;

    if (count != ths->streams)
        return IA_ERR_BAD_ARG;
    return opus_multistream2_decode_list (dec, buf, len, pcm, frame_size);
}

static IAErrCode ia_opus_close (IACodecContext *ths)
{
    IAOpusContext *ctx = (IAOpusContext *)ths->priv;
    OpusMS2Decoder *dec = (OpusMS2Decoder *)ctx->dec;

    if (dec) {
        opus_multistream2_decoder_destroy(dec);
        ctx->dec = 0;
    }
    return IA_OK;
}

const IACodec ia_opus_decoder = {
    .cid            = IA_CODEC_OPUS,
    .priv_size      = sizeof (IAOpusContext),
    .init           = ia_opus_init,
    .decode_list    = ia_opus_decode_list,
    .close          = ia_opus_close,
};
