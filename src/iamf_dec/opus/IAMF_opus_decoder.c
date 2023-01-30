#include "bitstreamrw.h"
#include "IAMF_codec.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "opus_multistream2_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAOPUS"

typedef struct IAOpusContext {
    void   *dec;
} IAOpusContext;

/**
 *  IAC-OPUS Specific
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |  Version = 1  | Channel Count |           Pre-skip            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     Input Sample Rate (Hz)                    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Output Gain (Q7.8 in dB)    | Mapping Family|
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  The fields in the ID Header:
 *
 *  unsigned int(8) Version;
 *  unsigned int(8) OutputChannelCount;
 *  unsigned int(16) PreSkip;
 *  unsigned int(32) InputSampleRate;
 *  signed int(16) OutputGain;
 *  unsigned int(8) ChannelMappingFamily;
 *
 *
 *  Constraints:
 *
 *  1, ChannelMappingFamily = 0
 *  2, Channel Count should be set to 2
 *  3, Output Gain shall not be used. In other words, it shall be set to 0dB
 *  4, The byte order of each field in ID Header shall be converted to big endian
 *
 * */
static IAErrCode ia_opus_init (IACodecContext *ths)
{
    IAOpusContext *ctx = (IAOpusContext *)ths->priv;
    IAErrCode ec = IA_OK;
    int ret = 0;
    uint8_t *config = ths->cspec;

    if (!ths->cspec || ths->clen <= 0) {
        return IA_ERR_BAD_ARG;
    }

    /* config += 8; */ // file err?

    ths->delay = get_uint16be (config, 2);
    ths->sample_rate = get_uint32be (config, 4);
    ths->channel_mapping_family = get_uint8 (config, 10);


    ctx->dec = opus_multistream2_decoder_create(ths->sample_rate,
               ths->streams, ths->coupled_streams,
               AUDIO_FRAME_FLOAT | AUDIO_FRAME_PLANE, &ret);
    if (!ctx->dec) {
        ia_loge("fail to open opus decoder.");
        ec = IA_ERR_INVALID_STATE;
    }

    return ec;
}

static int ia_opus_decode_list (IACodecContext *ths,
                                uint8_t *buf[], uint32_t len[],
                                uint32_t count,
                                void *pcm, const uint32_t frame_size)
{
    IAOpusContext *ctx = (IAOpusContext *)ths->priv;
    OpusMS2Decoder *dec = (OpusMS2Decoder *)ctx->dec;

    if (count != ths->streams) {
        return IA_ERR_BAD_ARG;
    }
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
