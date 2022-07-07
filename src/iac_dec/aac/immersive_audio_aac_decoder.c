#include <stdlib.h>

#include "aac_multistream_decoder.h"
#include "immersive_audio_codec.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_types.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IA_AAC"

static IAErrCode ia_aac_close (IACodecContext *ths);

typedef struct IA_AAC_Context {
    AACMSDecoder   *dec;
    short          *out;
} IA_AAC_Context;

typedef struct ESDesc {
    uint8_t     tag;
    uint32_t    size;
    uint8_t    *payload;
}ESDesc;

static int read_es_descriptor (uint8_t *data, uint32_t size, ESDesc *e)
{
    uint8_t ch;
    int ret = 1;

    e->tag = data[0];
    e->size = 0;
    for (int cnt = 0; cnt < 4; cnt++) {
        ch = data[ret++];
        e->size <<= 7;
        e->size |= (ch & 0x7f);
        if (!(ch & 0x80)) {
            break;
        }
    }

    e->payload = &data[ret];

    if (ret > size)
        ret = IA_ERR_BAD_ARG;

    return ret;
}

static IAErrCode ia_aac_init (IACodecContext *ths)
{
    IA_AAC_Context *ctx = (IA_AAC_Context *)ths->priv;
    uint8_t *config = ths->cspec;
    int len = ths->clen;
    int ret = 0;

    if (!ths->cspec || ths->clen <=0)
        return IA_ERR_BAD_ARG;

    if (ths->flags & IA_FLAG_CODEC_CONFIG_ISOBMFF) {
        int idx = 0;
        ESDesc esd;

        if (config[4] == 'e' && config[5] == 's' && config[6] == 'd' &&
                config[7] == 's') {
            config += 12;
            len -= 12;
        } else
            return IA_ERR_BAD_ARG;

        if (len <= 0) return IA_ERR_BAD_ARG;

        ret = read_es_descriptor (config, len, &esd);
        if (ret < 0 || esd.tag != 0x03) return IA_ERR_BAD_ARG; // MP4ESDescrTag
        idx += (ret + 3);

        ret = read_es_descriptor (config+idx, len-idx, &esd);
        if (ret < 0 || esd.tag != 0x04) return IA_ERR_BAD_ARG; // MP4DecConfigDescrTag
        idx += ret;

        if (config[idx] != 0x40 /* Audio ISO/IEC 14496-3 */
                || (config[idx + 1] >> 2 & 0x3f) != 5) /* AudioStream */
            return IA_ERR_BAD_ARG;
        idx += 13;
        ret = read_es_descriptor (config+idx, len-idx, &esd);
        if (ret < 0 || esd.tag != 0x05) return IA_ERR_BAD_ARG; // MP4DecSpecificDescrTag

        ths->cspec = esd.payload;
        ths->clen = esd.size;
        ia_logi("aac codec spec info size %d", esd.size);
    }

    if (ths->flags & IA_FLAG_SUBSTREAM_CODEC_SPECIFIC) {
        ctx->dec = aac_multistream_decoder_open(ths->cspec, ths->clen,
                                                ths->streams, ths->coupled_streams,
                                                AUDIO_FRAME_PLANE, &ret);
        if (!ctx->dec) return IA_ERR_INVALID_STATE;

        ctx->out = (short *)
            malloc (sizeof(short) * MAX_AAC_FRAME_SIZE * (ths->streams + ths->coupled_streams));
        if (!ctx->out) {
            ia_aac_close(ths);
            return IA_ERR_ALLOC_FAIL;
        }
    }

    return IA_OK;
}

static int ia_aac_decode_list (IACodecContext *ths,
                               uint8_t *buffer[], uint32_t size[], uint32_t count,
                               void* pcm, uint32_t frame_size)
{
    IA_AAC_Context *ctx = (IA_AAC_Context *)ths->priv;
    AACMSDecoder *dec = (AACMSDecoder *)ctx->dec;
    int ret = IA_OK;

    if (count != ths->streams)
        return IA_ERR_BAD_ARG;

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

IAErrCode ia_aac_close (IACodecContext *ths)
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
    .decode_list    = ia_aac_decode_list,
    .close          = ia_aac_close,
};
