#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "immersive_audio_codec.h"
#include "immersive_audio_core_decoder.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_utils.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IACORE"

extern const IACodec ia_opus_decoder;

extern const IACodec ia_aac_decoder;


struct IACoreDecoder {
    const IACodec *cdec[IA_CODEC_COUNT];
    IACodecID cid;

    IACodecContext *ctx;
};


static int ia_core_decoder_codec_check (IACoreDecoder* ths, IACodecID cid)
{
    return !!ths->cdec[cid];
}


static IAErrCode ia_cd_codec_register (IACoreDecoder *ths, IACodecID cid)
{
    IAErrCode ec = IA_ERR_UNIMPLEMENTED;

    switch (cid) {
        case IA_CODEC_OPUS:
            ths->cdec[cid] = &ia_opus_decoder;
            ec = IA_OK;
            break;
        case IA_CODEC_AAC:
            ths->cdec[cid] = &ia_aac_decoder;
            ec = IA_OK;
            break;
        default:
            ec = IA_ERR_BAD_ARG;
            ia_loge ("%s : Invalid codec id %d", ia_error_code_string(ec), cid);
            break;
    }

    return ec;
}


IACoreDecoder* ia_core_decoder_open (IACodecID cid)
{
    IACoreDecoder* ths = 0;
    IACodecContext *ctx = 0;
    IAErrCode ec = IA_OK;

    ths = (IACoreDecoder *) ia_mallocz (sizeof(IACoreDecoder));
    if (!ths) {
        ec = IA_ERR_ALLOC_FAIL;
        ia_loge("%s : IACoreDecoder handle.", ia_error_code_string (ec));
        return 0;
    }

    ia_cd_codec_register (ths, IA_CODEC_OPUS);
    ia_cd_codec_register (ths, IA_CODEC_AAC);

    if (!ia_core_decoder_codec_check (ths, cid)) {
        ec = IA_ERR_UNIMPLEMENTED;
        ia_loge ("%s : Unimplment %s decoder.", ia_error_code_string (ec),
                ia_codec_name(cid));
        goto termination;
    }

    ctx = (IACodecContext *) ia_mallocz (sizeof(IACodecContext));
    if (!ctx) {
        ec = IA_ERR_ALLOC_FAIL;
        ia_loge("%s : IACodecContext handle.", ia_error_code_string(ec));
        goto termination;
    }

    ths->ctx = ctx;

    ctx->priv = ia_mallocz (ths->cdec[cid]->priv_size);
    if (!ctx->priv) {
        ec = IA_ERR_ALLOC_FAIL;
        ia_loge("%s : private data.", ia_error_code_string(ec));
        goto termination;
    }

    ths->cid = cid;
termination:
    if (ec != IA_OK) {
        ia_core_decoder_close (ths);
        ths = 0;
    }
    return ths;
}


IAErrCode ia_core_decoder_init (IACoreDecoder* ths,
                                uint8_t* spec, uint32_t len, uint32_t flags)
{
    IACodecContext *ctx = ths->ctx;
    ctx->flags = flags;
    ctx->cspec = spec;
    ctx->clen = len;
    return ths->cdec[ths->cid]->init(ctx);
}


void ia_core_decoder_close (IACoreDecoder* ths)
{
    if (ths) {
        if (ths->ctx) {
            ths->cdec[ths->cid]->close(ths->ctx);
            if (ths->ctx->priv)
                free (ths->ctx->priv);
            free (ths->ctx);
        }
        free (ths);
    }
}


IAErrCode ia_core_decoder_set_streams_info (IACoreDecoder* ths,
                                            uint8_t streams,
                                            uint8_t coupled_streams)
{
    IACodecContext *ctx = ths->ctx;
    ctx->ambisonics = 0;
    ctx->streams = streams;
    ctx->coupled_streams = coupled_streams;
    if (ctx->flags & IA_FLAG_SUBSTREAM_CODEC_SPECIFIC)
        ths->cdec[ths->cid]->init_final(ctx);
    return IA_OK;
}


int ia_core_decoder_decode_list (IACoreDecoder* ths,
                                 uint8_t* buffer[], uint32_t size[],
                                 uint32_t count,
                                 float* out, uint32_t frame_size)
{
    IACodecContext *ctx = ths->ctx;
    if (ctx->streams != count)
        return IA_ERR_BUFFER_TOO_SMALL;
    return ths->cdec[ths->cid]->decode_list(ctx, buffer, size, count,
                                            out, frame_size);
}

