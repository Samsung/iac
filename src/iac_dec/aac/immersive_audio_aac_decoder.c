#include <stdlib.h>

#include "immersive_audio_codec.h"
#include "immersive_audio_types.h"
#include "aac_multistream_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IA_AAC"


typedef struct IA_AAC_Context {
    AACMSDecoder   *dec;
    short          *out;
} IA_AAC_Context;


static IAErrCode ia_aac_init (IACodecContext *ths)
{
    IAErrCode ec = IA_OK;
    return ec;
}

static IAErrCode ia_aac_init_final (IACodecContext *ths)
{
    IA_AAC_Context *ctx = (IA_AAC_Context *)ths->priv;
    IAErrCode ec = IA_OK;
    int ret = 0;

    if (ths->flags & IA_FLAG_SUBSTREAM_CODEC_SPECIFIC) {
        ctx->dec = aac_multistream_decoder_open(ths->streams,
                ths->coupled_streams, AUDIO_FRAME_PLANE, &ret);
        if (!ctx->dec) {
            ec = IA_ERR_INVALID_STATE; // TODO - convert AAC_STATE to IAErrCode
        }
    }

    return ec;
}

static int ia_aac_decode_list (IACodecContext *ths,
                               uint8_t *buffer[], uint32_t size[], uint32_t count,
                               void* pcm, uint32_t frame_size)
{
    IA_AAC_Context *ctx = (IA_AAC_Context *)ths->priv;
    AACMSDecoder *dec = (AACMSDecoder *)ctx->dec;
    int ret = IA_OK;

    if (count != ths->streams)
        return -1;

    if (!ctx->out) {
        ctx->out = (short *)
            malloc (sizeof(short) * frame_size * (ths->streams + ths->coupled_streams));
        if (!ctx->out)
            return IA_ERR_ALLOC_FAIL;
    }

    ret = aac_multistream_decode_list (dec, buffer, size, ctx->out, frame_size);
    if (ret > 0) {
        float *out = (float *)pcm;
        uint32_t samples = ret * (ths->streams + ths->coupled_streams);
        for (int i=0; i<samples; ++i) {
            out[i] = (1/32768.f) * ctx->out[i];
        }
    }

    return ret;
}

static IAErrCode ia_aac_close (IACodecContext *ths)
{
    IA_AAC_Context *ctx = (IA_AAC_Context *)ths->priv;
    AACMSDecoder *dec = (AACMSDecoder *)ctx->dec;

    if (dec) {
        aac_multistream_decoder_close(dec);
        ctx->dec = 0;
    }
    if (ctx->out)
        free (ctx->out);

    return IA_OK;
}

const IACodec ia_aac_decoder = {
    .cid            = IA_CODEC_AAC,
    .priv_size      = sizeof (IA_AAC_Context),
    .init           = ia_aac_init,
    .init_final     = ia_aac_init_final,
    .decode_list    = ia_aac_decode_list,
    .close          = ia_aac_close,
};
