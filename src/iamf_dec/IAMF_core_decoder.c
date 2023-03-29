/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file IAMF_core_decoder.c
 * @brief Core decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "IAMF_core_decoder.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "IAMF_utils.h"
#include "bitstream.h"
#include "fixedp11_5.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_CORE"

#ifdef CONFIG_OPUS_CODEC
extern const IACodec ia_opus_decoder;
#endif

#ifdef CONFIG_AAC_CODEC
extern const IACodec iamf_aac_decoder;
#endif

#ifdef CONFIG_FLAC_CODEC
extern const IACodec iamf_flac_decoder;
#endif

extern const IACodec iamf_pcm_decoder;

typedef struct FloatMatrix {
  float *matrix;
  int row;
  int column;
} FloatMatrix;

static int ia_core_decoder_codec_check(IACoreDecoder *ths, IACodecID cid) {
  return !!ths->cdec[cid];
}

static int ia_cd_codec_register(IACoreDecoder *ths, IACodecID cid) {
  int ec = IAMF_ERR_UNIMPLEMENTED;

  switch (cid) {
    case IAMF_CODEC_OPUS:
#ifdef CONFIG_OPUS_CODEC
      ths->cdec[cid] = &ia_opus_decoder;
      ec = IAMF_OK;
#endif
      break;
    case IAMF_CODEC_AAC:
#ifdef CONFIG_AAC_CODEC
      ths->cdec[cid] = &iamf_aac_decoder;
      ec = IAMF_OK;
#endif
      break;
    case IAMF_CODEC_FLAC:
#ifdef CONFIG_FLAC_CODEC
      ths->cdec[cid] = &iamf_flac_decoder;
      ec = IAMF_OK;
#endif
      break;

    case IAMF_CODEC_PCM:
      ths->cdec[cid] = &iamf_pcm_decoder;
      ec = IAMF_OK;
      break;
    default:
      ec = IAMF_ERR_BAD_ARG;
      ia_loge("%s : Invalid codec id %d", ia_error_code_string(ec), cid);
      break;
  }

  return ec;
}

static int iamf_core_decoder_convert(IACoreDecoder *ths, float *out, float *in,
                                     uint32_t frame_size) {
  FloatMatrix *matrix = ths->matrix;
  for (int s = 0; s < frame_size; ++s) {
    for (int r = 0; r < matrix->row; ++r) {
      out[r * frame_size + s] = .0f;
      for (int l = 0; l < matrix->column; ++l) {
        out[r * frame_size + s] +=
            in[l * frame_size + s] * matrix->matrix[r * matrix->column + l];
      }
    }
  }
  return IAMF_OK;
}

IACoreDecoder *ia_core_decoder_open(IACodecID cid) {
  IACoreDecoder *ths = 0;
  IACodecContext *ctx = 0;
  int ec = IAMF_OK;

  ths = IAMF_MALLOCZ(IACoreDecoder, 1);
  if (!ths) {
    ec = IAMF_ERR_ALLOC_FAIL;
    ia_loge("%s : IACoreDecoder handle.", ia_error_code_string(ec));
    return 0;
  }
#ifdef CONFIG_OPUS_CODEC
  ia_cd_codec_register(ths, IAMF_CODEC_OPUS);
#endif

#ifdef CONFIG_AAC_CODEC
  ia_cd_codec_register(ths, IAMF_CODEC_AAC);
#endif

#ifdef CONFIG_FLAC_CODEC
  ia_cd_codec_register(ths, IAMF_CODEC_FLAC);
#endif

  ia_cd_codec_register(ths, IAMF_CODEC_PCM);

  if (!ia_core_decoder_codec_check(ths, cid)) {
    ec = IAMF_ERR_UNIMPLEMENTED;
    ia_loge("%s : Unimplment %s decoder.", ia_error_code_string(ec),
            iamf_codec_name(cid));
    goto termination;
  }

  ctx = IAMF_MALLOCZ(IACodecContext, 1);
  if (!ctx) {
    ec = IAMF_ERR_ALLOC_FAIL;
    ia_loge("%s : IACodecContext handle.", ia_error_code_string(ec));
    goto termination;
  }

  ths->ctx = ctx;

  ctx->priv = IAMF_MALLOCZ(char, ths->cdec[cid]->priv_size);
  if (!ctx->priv) {
    ec = IAMF_ERR_ALLOC_FAIL;
    ia_loge("%s : private data.", ia_error_code_string(ec));
    goto termination;
  }

  ths->cid = cid;
termination:
  if (ec != IAMF_OK) {
    ia_core_decoder_close(ths);
    ths = 0;
  }
  return ths;
}

void ia_core_decoder_close(IACoreDecoder *ths) {
  if (ths) {
    if (ths->ctx) {
      ths->cdec[ths->cid]->close(ths->ctx);
      if (ths->ctx->priv) {
        free(ths->ctx->priv);
      }
      free(ths->ctx);
    }

    if (ths->matrix) {
      if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION) {
        FloatMatrix *fm = ths->matrix;
        if (fm->matrix) free(fm->matrix);
      }
      free(ths->matrix);
    }

    if (ths->buffer) free(ths->buffer);

    free(ths);
  }
}

int ia_core_decoder_init(IACoreDecoder *ths) {
  IACodecContext *ctx = ths->ctx;
  return ths->cdec[ths->cid]->init(ctx);
}

int ia_core_decoder_set_codec_conf(IACoreDecoder *ths, uint8_t *spec,
                                   uint32_t len) {
  IACodecContext *ctx = ths->ctx;
  ctx->cspec = spec;
  ctx->clen = len;
  return IAMF_OK;
}

int ia_core_decoder_set_streams_info(IACoreDecoder *ths, uint32_t mode,
                                     uint8_t channels, uint8_t streams,
                                     uint8_t coupled_streams, uint8_t mapping[],
                                     uint32_t mapping_size) {
  IACodecContext *ctx = ths->ctx;
  ths->ambisonics = mode;
  ctx->channels = channels;
  ctx->streams = streams;
  ctx->coupled_streams = coupled_streams;
  if (mapping && mapping_size > 0) {
    if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION) {
      FloatMatrix *matrix = IAMF_MALLOCZ(FloatMatrix, 1);
      int count;
      BitStream b;
      float *factors;

      if (!matrix) return IAMF_ERR_ALLOC_FAIL;

      bs(&b, mapping, mapping_size);

      matrix->row = ctx->channels;
      matrix->column = ctx->streams + ctx->coupled_streams;
      count = matrix->row * matrix->column;
      factors = IAMF_MALLOCZ(float, count);
      if (!factors) {
        free(matrix);
        return IAMF_ERR_ALLOC_FAIL;
      }
      matrix->matrix = factors;

      for (int i = 0; i < count; ++i) {
        factors[i] = q_to_float(bs_getA16b(&b), 15);
        ia_logi("factor %d : %f", i, factors[i]);
      }
      ths->matrix = matrix;
    }
  }
  return IAMF_OK;
}

int ia_core_decoder_decode_list(IACoreDecoder *ths, uint8_t *buffer[],
                                uint32_t size[], uint32_t count, float *out,
                                uint32_t frame_size) {
  int ret = IAMF_OK;
  IACodecContext *ctx = ths->ctx;
  if (ctx->streams != count) {
    return IAMF_ERR_BUFFER_TOO_SMALL;
  }
  if (ths->ambisonics != STREAM_MODE_AMBISONICS_PROJECTION)
    return ths->cdec[ths->cid]->decode_list(ctx, buffer, size, count, out,
                                            frame_size);
  else {
    if (!ths->buffer) {
      float *block = IAMF_MALLOC(
          float, (ctx->coupled_streams + ctx->streams) * frame_size);
      if (!block) return IAMF_ERR_ALLOC_FAIL;
      ths->buffer = block;
    }
    ret = ths->cdec[ths->cid]->decode_list(ctx, buffer, size, count,
                                           ths->buffer, frame_size);
    if (ret > 0) {
      iamf_core_decoder_convert(ths, out, ths->buffer, ret);
    }
    return ret;
  }
}
