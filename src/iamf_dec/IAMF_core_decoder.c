#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "fixedp11_5.h"
#include "IAMF_codec.h"
#include "IAMF_core_decoder.h"
#include "IAMF_debug.h"
#include "IAMF_utils.h"
#include "IAMF_types.h"
#include "bitstream.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_CORE"

extern const IACodec ia_opus_decoder;

extern const IACodec ia_aac_decoder;

extern const IACodec iamf_pcm_decoder;

struct IACoreDecoder {
    const IACodec  *cdec[IA_CODEC_COUNT];
    IACodecID       cid;
    IACodecContext *ctx;

    int             ambisonics;
    void           *matrix;
    void           *buffer;
};

typedef struct FloatMatrix {
    float      *matrix;
    int         row;
    int         column;
} FloatMatrix;


static int ia_core_decoder_codec_check (IACoreDecoder *ths, IACodecID cid)
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
    case IA_CODEC_PCM:
        ths->cdec[cid] = &iamf_pcm_decoder;
        ec = IA_OK;
        break;
    default:
        ec = IA_ERR_BAD_ARG;
        ia_loge ("%s : Invalid codec id %d", ia_error_code_string(ec), cid);
        break;
    }

    return ec;
}

static int
iamf_core_decoder_convert (IACoreDecoder *ths, float *out, float *in, uint32_t frame_size)
{
    FloatMatrix    *matrix = ths->matrix;
    for (int s=0; s<frame_size; ++s) {
        for (int r=0; r<matrix->row; ++r) {
            out[r*frame_size + s] = .0f;
            for (int l=0; l<matrix->column; ++l) {
                out[r*frame_size + s] += in[l*frame_size + s] * matrix->matrix[r*matrix->column + l];
            }
        }
    }
    return IAMF_OK;
}


IACoreDecoder *ia_core_decoder_open (IACodecID cid)
{
    IACoreDecoder *ths = 0;
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
    ia_cd_codec_register (ths, IA_CODEC_PCM);

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


void ia_core_decoder_close (IACoreDecoder *ths)
{
    if (ths) {
        if (ths->ctx) {
            ths->cdec[ths->cid]->close(ths->ctx);
            if (ths->ctx->priv) {
                free (ths->ctx->priv);
            }
            free (ths->ctx);
        }

        if (ths->matrix) {
            if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION) {
                FloatMatrix *fm = ths->matrix;
                if (fm->matrix)
                    free (fm->matrix);
            }
            free (ths->matrix);
        }

        if (ths->buffer)
            free (ths->buffer);

        free (ths);
    }
}


IAErrCode ia_core_decoder_init (IACoreDecoder *ths)
{
    IACodecContext *ctx = ths->ctx;
    return ths->cdec[ths->cid]->init(ctx);
}


IAErrCode ia_core_decoder_set_codec_conf (IACoreDecoder *ths,
        uint8_t *spec, uint32_t len)
{
    IACodecContext *ctx = ths->ctx;
    ctx->cspec = spec;
    ctx->clen = len;
    return IA_OK;
}


IAErrCode ia_core_decoder_set_streams_info (IACoreDecoder *ths,
        uint32_t mode, uint8_t channels, uint8_t streams, uint8_t coupled_streams,
        uint8_t mapping[], uint32_t mapping_size)
{
    IACodecContext *ctx = ths->ctx;
    ths->ambisonics = mode;
    ctx->channels = channels;
    ctx->streams = streams;
    ctx->coupled_streams = coupled_streams;
    if (mapping && mapping_size > 0) {
        if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION) {
            FloatMatrix    *matrix = IAMF_MALLOCZ(FloatMatrix, 1);
            int             count;
            BitStream       b;
            float          *factors;

            if (!matrix)
                return IAMF_ERR_ALLOC_FAIL;

            bs(&b, mapping, mapping_size);

            matrix->row = ctx->channels;
            matrix->column = ctx->streams + ctx->coupled_streams;
            count = matrix->row * matrix->column;
            factors = IAMF_MALLOCZ(float, count);
            if (!factors) {
                free (matrix);
                return IAMF_ERR_ALLOC_FAIL;
            }
            matrix->matrix = factors;

            for (int i=0; i<count; ++i) {
                factors[i] = q_to_float(bs_getA16b(&b), 15);
                ia_logi("factor %d : %f", i, factors[i]);
            }
            ths->matrix = matrix;
        }
    }
    return IA_OK;
}


int ia_core_decoder_decode_list (IACoreDecoder *ths,
                                 uint8_t *buffer[], uint32_t size[],
                                 uint32_t count,
                                 float *out, uint32_t frame_size)
{
    int ret = IAMF_OK;
    IACodecContext *ctx = ths->ctx;
    if (ctx->streams != count) {
        return IA_ERR_BUFFER_TOO_SMALL;
    }
    if (ths->ambisonics != STREAM_MODE_AMBISONICS_PROJECTION)
        return ths->cdec[ths->cid]->decode_list(ctx, buffer, size, count,
                                                out, frame_size);
    else {
        if (!ths->buffer) {
            float *block = IAMF_MALLOC(float, (ctx->coupled_streams + ctx->streams) * frame_size);
            if (!block)
                return IAMF_ERR_ALLOC_FAIL;
            ths->buffer = block;
        }
        ret = ths->cdec[ths->cid]->decode_list(ctx, buffer, size, count, ths->buffer, frame_size);
        if (ret > 0) {
            iamf_core_decoder_convert(ths, out, ths->buffer, ret);
        }
        return ret;
    }
}

