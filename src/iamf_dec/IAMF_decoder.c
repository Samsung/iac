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
 * @file IAMF_decoder.c
 * @brief IAMF decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "IAMF_OBU.h"
#include "IAMF_debug.h"
#include "IAMF_decoder.h"
#include "IAMF_decoder_private.h"
#include "IAMF_utils.h"
#include "ae_rdr.h"
#include "bitstream.h"
#include "bitstreamrw.h"
#include "demixer.h"
#include "fixedp11_5.h"
#include "speex_resampler.h"

#define RSHIFT(a) (1 << (a))
#define INAVLID_INDEX -1
#define INVALID_ID (uint64_t)(-1)
#define OUTPUT_SAMPLERATE 48000
#define SPEEX_RESAMPLER_QUALITY 4

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_DEC"
#define STR(str) _STR(str)
#define _STR(str) #str
#define SR 0
#if SR
extern void iamf_rec_stream_log(int eid, int chs, float *in, int size);
extern void iamf_ren_stream_log(int eid, int chs, float *out, int size);
extern void iamf_mix_stream_log(int chs, float *out, int size);
extern void iamf_stream_log_free();
#endif

/* ----------------------------- Utils ----------------------------- */

static void swap(void **p1, void **p2) {
  void *p = *p2;
  *p2 = *p1;
  *p1 = p;
}

/* ----------------------------- Internal methods ------------------ */

static int16_t FLOAT2INT16(float x) {
#define MIN32(a, b) ((a) < (b) ? (a) : (b)) /**< Minimum 32-bit value.   */
#define MAX32(a, b) ((a) > (b) ? (a) : (b)) /**< Maximum 32-bit value.   */
#define float2int(x) lrintf(x)

  x = x * 32768.f;
  x = MAX32(x, -32768);
  x = MIN32(x, 32767);
  return (int16_t)float2int(x);
}

static void iamf_decoder_plane2stride_out_short(void *dst, const float *src,
                                                int frame_size, int channels) {
  int16_t *short_dst = (int16_t *)dst;

  ia_logd("channels %d", channels);
  for (int c = 0; c < channels; ++c) {
    if (src) {
      for (int i = 0; i < frame_size; i++) {
        short_dst[i * channels + c] = FLOAT2INT16(src[frame_size * c + i]);
      }
    } else {
      for (int i = 0; i < frame_size; i++) {
        short_dst[i * channels + c] = 0;
      }
    }
  }
}
static void ia_decoder_stride2plane_out_float(void *dst, const float *src,
                                              int frame_size, int channels) {
  float *float_dst = (float *)dst;

  ia_logd("channels %d", channels);
  for (int i = 0; i < frame_size; i++) {
    if (src) {
      for (int c = 0; c < channels; ++c) {
        float_dst[c * frame_size + i] = src[channels * i + c];
      }
    } else {
      for (int c = 0; c < channels; ++c) {
        float_dst[c * frame_size + i] = 0;
      }
    }
  }
}

static void ia_decoder_plane2stride_out_float(void *dst, const float *src,
                                              int frame_size, int channels) {
  float *float_dst = (float *)dst;

  ia_logd("channels %d", channels);
  for (int c = 0; c < channels; ++c) {
    if (src) {
      for (int i = 0; i < frame_size; i++) {
        float_dst[i * channels + c] = src[frame_size * c + i];
      }
    } else {
      for (int i = 0; i < frame_size; i++) {
        float_dst[i * channels + c] = 0;
      }
    }
  }
}

static int iamf_sound_system_valid(IAMF_SoundSystem ss) {
  return ss >= SOUND_SYSTEM_A && ss <= SOUND_SYSTEM_EXT_312;
}

static int iamf_sound_system_channels_count_without_lfe(IAMF_SoundSystem ss) {
  static int ss_channels[] = {2, 5, 7, 9, 10, 10, 13, 22, 7, 11, 9, 5};
  return ss_channels[ss];
}

static int iamf_sound_system_lfe1(IAMF_SoundSystem ss) {
  return ss != SOUND_SYSTEM_A;
}

static int iamf_sound_system_lfe2(IAMF_SoundSystem ss) {
  return ss == SOUND_SYSTEM_F || ss == SOUND_SYSTEM_H;
}

static uint32_t iamf_sound_system_get_rendering_id(IAMF_SoundSystem ss) {
  static IAMF_SOUND_SYSTEM ss_rids[] = {BS2051_A, BS2051_B, BS2051_C, BS2051_D,
                                        BS2051_E, BS2051_F, BS2051_G, BS2051_H,
                                        BS2051_I, BS2051_J, IAMF_712, IAMF_312};
  return ss_rids[ss];
}

static IAMF_SoundMode iamf_sound_mode_combine(IAMF_SoundMode a,
                                              IAMF_SoundMode b) {
  int out = IAMF_SOUND_MODE_MULTICHANNEL;
  if (a == IAMF_SOUND_MODE_NONE)
    out = b;
  else if (b == IAMF_SOUND_MODE_NONE)
    out = a;
  else if (a == b)
    out = a;
  else if (a == IAMF_SOUND_MODE_BINAURAL || b == IAMF_SOUND_MODE_BINAURAL)
    out = IAMF_SOUND_MODE_NA;

  return out;
}

static uint32_t iamf_layer_layout_get_rendering_id(int layer_layout) {
  static IAMF_SOUND_SYSTEM l_rids[] = {
      IAMF_MONO, IAMF_STEREO, IAMF_51,  IAMF_512, IAMF_514,
      IAMF_71,   IAMF_712,    IAMF_714, IAMF_312, IAMF_BINAURAL};
  return l_rids[layer_layout];
}

static int iamf_layer_layout_lfe1(int layer_layout) {
  return layer_layout > IA_CHANNEL_LAYOUT_STEREO &&
         layer_layout < IA_CHANNEL_LAYOUT_BINAURAL;
}

static IAMF_SoundSystem iamf_layer_layout_convert_sound_system(int layout) {
  static IAMF_SoundSystem layout2ss[] = {-1,
                                         SOUND_SYSTEM_A,
                                         SOUND_SYSTEM_B,
                                         SOUND_SYSTEM_C,
                                         SOUND_SYSTEM_D,
                                         SOUND_SYSTEM_I,
                                         SOUND_SYSTEM_EXT_712,
                                         SOUND_SYSTEM_J,
                                         SOUND_SYSTEM_EXT_712};
  if (ia_channel_layout_type_check(layout)) return layout2ss[layout];
  return -1;
}

static int iamf_layout_lfe1(IAMF_Layout *layout) {
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    return iamf_sound_system_lfe1(layout->sound_system.sound_system);
  }
  return 0;
}

static int iamf_layout_lfe2(IAMF_Layout *layout) {
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    return iamf_sound_system_lfe2(layout->sound_system.sound_system);
  }
  return 0;
}

static int iamf_layout_channels_count(IAMF_Layout *layout) {
  int ret = 0;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    ret = iamf_sound_system_channels_count_without_lfe(
        layout->sound_system.sound_system);
    ret += iamf_sound_system_lfe1(layout->sound_system.sound_system);
    ret += iamf_sound_system_lfe2(layout->sound_system.sound_system);
    ia_logd("sound system %x, channels %d", layout->sound_system.sound_system,
            ret);
  } else if (layout->type == IAMF_LAYOUT_TYPE_BINAURAL) {
    ret = 2;
    ia_logd("binaural channels %d", ret);
  }

  return ret;
}

static int iamf_layout_lfe_check(IAMF_Layout *layout) {
  int ret = 0;
  ret += iamf_layout_lfe1(layout);
  ret += iamf_layout_lfe2(layout);
  return !!ret;
}

static void iamf_layout_reset(IAMF_Layout *layout) {
  if (layout) {
    if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL &&
        layout->sp_labels.sp_label) {
      free(layout->sp_labels.sp_label);
    }
    memset(layout, 0, sizeof(IAMF_Layout));
  }
}

static int iamf_layout_copy(IAMF_Layout *dst, IAMF_Layout *src) {
  memcpy(dst, src, sizeof(IAMF_Layout));
  if (src->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
    dst->sp_labels.sp_label =
        IAMF_MALLOCZ(uint8_t, src->sp_labels.num_loudspeakers);
    if (dst->sp_labels.sp_label) {
      for (int i = 0; i < src->sp_labels.num_loudspeakers; ++i)
        dst->sp_labels.sp_label[i] = src->sp_labels.sp_label[i];
    }
  }
  return IAMF_OK;
}

static int iamf_layout_copy2(IAMF_Layout *dst, TargetLayout *src) {
  dst->type = src->type;
  if (src->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    SoundSystemLayout *layout = SOUND_SYSTEM_LAYOUT(src);
    dst->sound_system.sound_system = layout->sound_system;
  }
  return IAMF_OK;
}

static void iamf_layout_dump(IAMF_Layout *layout) {
  if (layout) {
    ia_logt("layout type %d", layout->type);
    if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
      ia_logt("number sp labels %d", layout->sp_labels.num_loudspeakers);
      for (int i = 0; i < layout->sp_labels.num_loudspeakers; ++i)
        ia_logt("sp label %d : %d", i, layout->sp_labels.sp_label[i] & U8_MASK);
    } else if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
      ia_logt("sound system %d", layout->sound_system.sound_system);
    }
  }
}

static void iamf_layout_info_free(LayoutInfo *layout) {
  if (layout) {
    if (layout->sp.sp_layout.predefined_sp)
      free(layout->sp.sp_layout.predefined_sp);
    iamf_layout_reset(&layout->layout);
    free(layout);
  }
}

static IAMF_SoundSystem iamf_layout_get_sound_system(IAMF_Layout *layout) {
  IAMF_SoundSystem ss = SOUND_SYSTEM_A;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION)
    ss = layout->sound_system.sound_system;
  return ss;
}

static IAMF_SoundMode iamf_layout_get_sound_mode(IAMF_Layout *layout) {
  IAMF_SoundMode mode = IAMF_SOUND_MODE_NONE;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    if (layout->sound_system.sound_system == SOUND_SYSTEM_A)
      mode = IAMF_SOUND_MODE_STEREO;
    else
      mode = IAMF_SOUND_MODE_MULTICHANNEL;
  } else if (layout->type == IAMF_LAYOUT_TYPE_BINAURAL)
    mode = IAMF_SOUND_MODE_BINAURAL;

  return mode;
}

static void iamf_recon_channels_order_update(IAChannelLayoutType layout,
                                             IAMF_ReconGain *re) {
  int chs = 0;
  static IAReconChannel recon_channel_order[] = {
      IA_CH_RE_L,  IA_CH_RE_C,   IA_CH_RE_R,   IA_CH_RE_LS,
      IA_CH_RE_RS, IA_CH_RE_LTF, IA_CH_RE_RTF, IA_CH_RE_LB,
      IA_CH_RE_RB, IA_CH_RE_LTB, IA_CH_RE_RTB, IA_CH_RE_LFE};

  static IAChannel channel_layout_map[IA_CHANNEL_LAYOUT_COUNT][IA_CH_RE_COUNT] =
      {{IA_CH_MONO, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID},
       {IA_CH_L2, IA_CH_INVALID, IA_CH_R2, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID},
       {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_LFE},
       {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HL, IA_CH_HR,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
       {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HFL, IA_CH_HFR,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE},
       {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_LFE},
       {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HL, IA_CH_HR,
        IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
       {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HFL, IA_CH_HFR,
        IA_CH_BL7, IA_CH_BR7, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE},
       {IA_CH_L3, IA_CH_C, IA_CH_R3, IA_CH_INVALID, IA_CH_INVALID, IA_CH_TL,
        IA_CH_TR, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_LFE}};

#define RECON_CHANNEL_FLAG(c) RSHIFT(c)

  for (int c = 0; c < IA_CH_RE_COUNT; ++c) {
    if (re->flags & RECON_CHANNEL_FLAG(recon_channel_order[c]))
      re->order[chs++] = channel_layout_map[layout][recon_channel_order[c]];
  }
}

static int iamf_channel_layout_get_new_channels(IAChannelLayoutType last,
                                                IAChannelLayoutType cur,
                                                IAChannel *new_chs,
                                                uint32_t count) {
  uint32_t chs = 0;

  /**
   * In ChannelGroup for Channel audio: The order conforms to following rules:
   *
   * @ Coupled Substream(s) comes first and followed by non-coupled
   * Substream(s).
   * @ Coupled Substream(s) for surround channels comes first and followed by
   * one(s) for top channels.
   * @ Coupled Substream(s) for front channels comes first and followed by
   * one(s) for side, rear and back channels.
   * @ Coupled Substream(s) for side channels comes first and followed by one(s)
   * for rear channels.
   * @ Center channel comes first and followed by LFE and followed by the other
   * one.
   * */

  if (last == IA_CHANNEL_LAYOUT_INVALID) {
    chs = ia_audio_layer_get_channels(cur, new_chs, count);
  } else {
    uint32_t s1 = ia_channel_layout_get_category_channels_count(
        last, IA_CH_CATE_SURROUND);
    uint32_t s2 =
        ia_channel_layout_get_category_channels_count(cur, IA_CH_CATE_SURROUND);
    uint32_t t1 =
        ia_channel_layout_get_category_channels_count(last, IA_CH_CATE_TOP);
    uint32_t t2 =
        ia_channel_layout_get_category_channels_count(cur, IA_CH_CATE_TOP);

    if (s1 < 5 && 5 <= s2) {
      new_chs[chs++] = IA_CH_L5;
      new_chs[chs++] = IA_CH_R5;
      ia_logd("new channels : l5/r5(l7/r7)");
    }
    if (s1 < 7 && 7 <= s2) {
      new_chs[chs++] = IA_CH_SL7;
      new_chs[chs++] = IA_CH_SR7;
      ia_logd("new channels : sl7/sr7");
    }
    if (t2 != t1 && t2 == 4) {
      new_chs[chs++] = IA_CH_HFL;
      new_chs[chs++] = IA_CH_HFR;
      ia_logd("new channels : hfl/hfr");
    }
    if (t2 - t1 == 4) {
      new_chs[chs++] = IA_CH_HBL;
      new_chs[chs++] = IA_CH_HBR;
      ia_logd("new channels : hbl/hbr");
    } else if (!t1 && t2 - t1 == 2) {
      if (s2 < 5) {
        new_chs[chs++] = IA_CH_TL;
        new_chs[chs++] = IA_CH_TR;
        ia_logd("new channels : tl/tr");
      } else {
        new_chs[chs++] = IA_CH_HL;
        new_chs[chs++] = IA_CH_HR;
        ia_logd("new channels : hl/hr");
      }
    }

    if (s1 < 3 && 3 <= s2) {
      new_chs[chs++] = IA_CH_C;
      new_chs[chs++] = IA_CH_LFE;
      ia_logd("new channels : c/lfe");
    }
    if (s1 < 2 && 2 <= s2) {
      new_chs[chs++] = IA_CH_L2;
      ia_logd("new channel : l2");
    }
  }

  if (chs > count) {
    ia_loge("too much new channels %d, we only need less than %d channels", chs,
            count);
    chs = IAMF_ERR_BUFFER_TOO_SMALL;
  }
  return chs;
}

static IAChannel iamf_output_gain_channel_map(IAChannelLayoutType type,
                                              IAOutputGainChannel gch) {
  IAChannel ch = IA_CH_INVALID;
  switch (gch) {
    case IA_CH_GAIN_L: {
      switch (type) {
        case IA_CHANNEL_LAYOUT_MONO:
          ch = IA_CH_MONO;
          break;
        case IA_CHANNEL_LAYOUT_STEREO:
          ch = IA_CH_L2;
          break;
        case IA_CHANNEL_LAYOUT_312:
          ch = IA_CH_L3;
          break;
        default:
          break;
      }
    } break;

    case IA_CH_GAIN_R: {
      switch (type) {
        case IA_CHANNEL_LAYOUT_STEREO:
          ch = IA_CH_R2;
          break;
        case IA_CHANNEL_LAYOUT_312:
          ch = IA_CH_R3;
          break;
        default:
          break;
      }
    } break;

    case IA_CH_GAIN_LS: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) == 5) {
        ch = IA_CH_SL5;
      }
    } break;

    case IA_CH_GAIN_RS: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) == 5) {
        ch = IA_CH_SR5;
      }
    } break;

    case IA_CH_GAIN_LTF: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) < 5) {
        ch = IA_CH_TL;
      } else {
        ch = IA_CH_HL;
      }
    } break;

    case IA_CH_GAIN_RTF: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) < 5) {
        ch = IA_CH_TR;
      } else {
        ch = IA_CH_HR;
      }
    } break;
    default:
      break;
  }

  return ch;
}

static IACodecID iamf_codec_4cc_get_codecID(uint32_t id) {
#define TAG(a, b, c, d) ((a) | (b) << 8 | (c) << 16 | (d) << 24)

  switch (id) {
    case TAG('m', 'p', '4', 'a'):
    case TAG('e', 's', 'd', 's'):
      return IAMF_CODEC_AAC;

    case TAG('O', 'p', 'u', 's'):
    case TAG('d', 'O', 'p', 's'):
      return IAMF_CODEC_OPUS;

    case TAG('f', 'L', 'a', 'C'):
      return IAMF_CODEC_FLAC;

    case TAG('i', 'p', 'c', 'm'):
      return IAMF_CODEC_PCM;

    default:
      return IAMF_CODEC_UNKNOWN;
  }
}

static int iamf_codec_get_delay(IACodecID cid) {
  if (cid == IAMF_CODEC_AAC)
    return AAC_DELAY;
  else if (cid == IAMF_CODEC_OPUS)
    return OPUS_DELAY;
  return 0;
}

/* ----------------------------- Internal Interfaces--------------- */

static uint32_t iamf_decoder_internal_read_descriptors_OBUs(
    IAMF_DecoderHandle handle, const uint8_t *data, uint32_t size);
static int32_t iamf_decoder_internal_add_descrptor_OBU(
    IAMF_DecoderHandle handle, IAMF_OBU *obu);
static IAMF_StreamDecoder *iamf_stream_decoder_open(IAMF_Stream *stream,
                                                    IAMF_CodecConf *conf);
static int iamf_decoder_internal_deliver(IAMF_DecoderHandle handle,
                                         IAMF_Frame *obj);
static int iamf_stream_scale_decoder_decode(IAMF_StreamDecoder *decoder,
                                            float *pcm);
static int iamf_stream_scale_demixer_configure(IAMF_StreamDecoder *decoder);
static int32_t iamf_stream_scale_decoder_demix(IAMF_StreamDecoder *decoder,
                                               float *src, float *dst,
                                               uint32_t frame_size);
static int iamf_stream_ambisonics_decoder_decode(IAMF_StreamDecoder *decoder,
                                                 float *pcm);

/* >>>>>>>>>>>>>>>>>> DATABASE >>>>>>>>>>>>>>>>>> */

static void iamf_database_reset(IAMF_DataBase *db);
static IAMF_CodecConf *iamf_database_get_codec_conf(IAMF_DataBase *db,
                                                    uint64_t cid);
static ElementItem *iamf_database_element_get_item(IAMF_DataBase *db,
                                                   uint64_t eid);

static void iamf_object_free(void *obj) { IAMF_object_free(IAMF_OBJ(obj)); }

static ObjectSet *iamf_object_set_new(IAMF_Free func) {
  ObjectSet *s = IAMF_MALLOCZ(ObjectSet, 1);
  if (s) {
    s->objFree = func;
  }

  return s;
}

static void iamf_object_set_free(ObjectSet *s) {
  if (s) {
    if (s->objFree) {
      for (int i = 0; i < s->count; ++i) s->objFree(s->items[i]);
      if (s->items) free(s->items);
    }
    free(s);
  }
}

#define CAP_DEFAULT 6
static int iamf_object_set_add(ObjectSet *s, void *item) {
  if (!item) return IAMF_ERR_BAD_ARG;

  if (s->count == s->capacity) {
    void **cap = 0;
    if (!s->count) {
      cap = IAMF_MALLOCZ(void *, CAP_DEFAULT);
    } else {
      cap = IAMF_REALLOC(void *, s->items, s->capacity + CAP_DEFAULT);
    }
    if (!cap) return IAMF_ERR_ALLOC_FAIL;
    s->items = cap;
    s->capacity += CAP_DEFAULT;
  }

  s->items[s->count++] = item;
  return IAMF_OK;
}

static int iamf_object_codec_conf_get_sampling_rate(IAMF_CodecConf *c) {
  uint32_t cid;
  if (!c || !c->decoder_conf || !c->decoder_conf_size ||
      iamf_codec_4cc_get_codecID(c->codec_id) == IAMF_CODEC_UNKNOWN)
    return IAMF_ERR_BAD_ARG;
  cid = iamf_codec_4cc_get_codecID(c->codec_id);
  if (cid == IAMF_CODEC_PCM) {
    if (c->decoder_conf_size < 6) return IAMF_ERR_BAD_ARG;
    return get_int32be(c->decoder_conf, 2);
  } else if (cid == IAMF_CODEC_OPUS) {
    if (c->decoder_conf_size < 8) return IAMF_ERR_BAD_ARG;
    return get_int32be(c->decoder_conf, 4);
  } else if (cid == IAMF_CODEC_AAC) {
    BitStream b;
    int ret, type;
    static int sf[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                       16000, 12000, 11025, 8000,  7350,  0,     0,     0};

    /* DecoderConfigDescriptor (14 bytes) + DecSpecificInfoTag (1 byte) */
    if (c->decoder_conf_size < 16) return IAMF_ERR_BAD_ARG;
    bs(&b, c->decoder_conf + 15, c->decoder_conf_size - 15);

    type = bs_get32b(&b, 5);
    if (type == 31) bs_get32b(&b, 6);

    ret = bs_get32b(&b, 4);
    if (ret == 0xf)
      return bs_get32b(&b, 24);
    else
      return sf[ret];
  } else if (cid == IAMF_CODEC_FLAC) {
    BitStream b;
    int last, type, size;
    bs(&b, c->decoder_conf, c->decoder_conf_size);
    while (1) {
      last = bs_get32b(&b, 1);
      type = bs_get32b(&b, 7);
      size = bs_get32b(&b, 24);
      if (!type) {
        bs_skip(&b, 80);
        return bs_get32b(&b, 20);
      } else
        bs_skip(&b, size * 8);

      if (last) break;
    }
  }
  return -1;
}

static uint32_t iamf_object_element_get_recon_gain_flags(IAMF_Element *e) {
  uint32_t ret = 0;

  if (e->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED && e->channels_conf) {
    for (int i = 0; i < e->channels_conf->nb_layers; ++i) {
      if (e->channels_conf->layer_conf_s &&
          e->channels_conf->layer_conf_s->recon_gain_flag)
        ret |= RSHIFT(i);
    }
  }
  return ret;
}

static ParameterSegment *iamf_object_parameter_get_segment(IAMF_Parameter *obj,
                                                           uint64_t timestamp) {
  ParameterSegment *seg;
  uint64_t ts = 0;

  if (!obj || timestamp >= obj->duration) return 0;

  seg = obj->segments[0];
  for (int i = 1; i < obj->nb_segments; ++i) {
    ts += seg->segment_interval;
    if (ts > timestamp) break;
    seg = obj->segments[i];
  }
  return seg;
}

static int iamf_object_parameter_mix_gain_linear(int t, float s, float e,
                                                 float *g) {
  int n = t - 1;
  float a;

  for (int i = 0; i < t; ++i) {
    a = i / n;
    g[i] = (1.0f - a) * s + a * e;
  }

  return IAMF_OK;
}

static int iamf_object_parameter_mix_gain_quad(int t, int ct, float s, float c,
                                               float e, float *g) {
  int n = t - 1;
  float a, alpha, beta, gamma;

  alpha = n - 2 * ct;
  beta = 2 * ct;

  for (int i = 0; i < t; ++i) {
    gamma = 0 - i;
    a = (sqrt(pow(beta, 2) - 4 * alpha * gamma) - beta) / (2 * alpha);
    g[i] = pow(1 - a, 2) * s + 2 * (1 - a) * a * c + pow(a, 2) * e;
  }

  return IAMF_OK;
}

static uint64_t iamf_database_object_get_timestamp(IAMF_DataBase *db,
                                                   uint64_t id) {
  SyncItem *si = db->sViewer.items;
  uint64_t ts = 0;

  for (int i = 0; i < db->sViewer.count; ++i) {
    if (si[i].id == id) {
      ts = si[i].start;
      break;
    }
  }

  return ts;
}

static ParameterItem *iamf_database_parameter_viewer_get_item(
    ParameterViewer *viewer, uint64_t pid) {
  ParameterItem *pi = 0;
  for (int i = 0; i < viewer->count; ++i) {
    if (viewer->items[i]->id == pid) {
      pi = viewer->items[i];
      break;
    }
  }
  return pi;
}

static int iamf_database_parameter_viewer_add_item(IAMF_DataBase *db,
                                                   ParameterBase *base,
                                                   uint64_t parent_id) {
  ParameterViewer *pv = &db->pViewer;
  ParameterItem *pi = 0;
  ParameterItem **pis = 0;
  ElementItem *ei = 0;
  uint64_t pid, type;

  if (!base) return IAMF_ERR_BAD_ARG;

  pid = base->id;
  type = base->type;
  pi = iamf_database_parameter_viewer_get_item(pv, pid);

  if (pi) return IAMF_OK;

  pis = IAMF_REALLOC(ParameterItem *, pv->items, pv->count + 1);
  if (!pis) {
    return IAMF_ERR_ALLOC_FAIL;
  }
  pv->items = pis;
  pis[pv->count] = 0;

  pi = IAMF_MALLOCZ(ParameterItem, 1);
  if (!pi) {
    return IAMF_ERR_ALLOC_FAIL;
  }

  if (type == IAMF_PARAMETER_TYPE_MIX_GAIN) {
    pi->mix_gain = IAMF_MALLOCZ(MixGain, 1);
    if (!pi->mix_gain) {
      free(pi);
      return IAMF_ERR_ALLOC_FAIL;
    }
  }

  pis[pv->count++] = pi;
  ia_logt("add parameter item %p, its id %lu, and count is %d", pi, pid,
          pv->count);

  pi->id = pid;
  pi->type = base->type;
  pi->parent_id = parent_id;
  pi->param_base = base;
  if (type == IAMF_PARAMETER_TYPE_DEMIXING) {
    ei = iamf_database_element_get_item(db, parent_id);
    ei->demixing = pi;
  } else if (type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
    ei = iamf_database_element_get_item(db, parent_id);
    ei->reconGain = pi;
  }
  return IAMF_OK;
}

static int iamf_database_parameter_viewer_add(IAMF_DataBase *db,
                                              IAMF_Object *obj) {
  ParameterViewer *pv = &db->pViewer;
  IAMF_Parameter *p = (IAMF_Parameter *)obj;
  ParameterItem *pi = 0;

  pi = iamf_database_parameter_viewer_get_item(pv, p->id);
  if (pi) {
    ElementItem *ei = iamf_database_element_get_item(db, pi->parent_id);
    if (pi->parameter) {
      if (ei)
        ia_logt("p(%lu)ts(s:%lu, e: %lu) vs e(%lu)ts(%lu).", pi->id,
                pi->timestamp, pi->timestamp + pi->parameter->duration, ei->id,
                ei->timestamp);
      if (ei && pi->timestamp + pi->parameter->duration > ei->timestamp)
        ia_loge(
            "Parameter (%lu) arrives early. (%lu vs %lu). element item %p,  id "
            "%lu",
            p->id, pi->timestamp + pi->parameter->duration, ei->timestamp, ei,
            ei->id);
      for (int i = 0; i < db->parameters->count; ++i) {
        if (pi->parameter == db->parameters->items[i]) {
          iamf_object_free(db->parameters->items[i]);
          db->parameters->items[i] = obj;
        }
      }
    } else {
      iamf_object_set_add(db->parameters, obj);
    }
    pi->parameter = p;
    if (ei) pi->timestamp = ei->timestamp;
    ia_logd("parameter id %lu, timestamp update %lu", pi->id, pi->timestamp);

    if (pi->type == IAMF_PARAMETER_TYPE_MIX_GAIN) {
      int m;
      MixGainSegment *seg;
      float gain_db, gain1_db, gain2_db;
      float gain_l, gain1_l, gain2_l;
      if (pi->mix_gain->mix_gain_uints) {
        for (int i = 0; i < pi->mix_gain->nb_seg; ++i) {
          if (pi->mix_gain->mix_gain_uints[i].gains)
            free(pi->mix_gain->mix_gain_uints[i].gains);
        }
        free(pi->mix_gain->mix_gain_uints);
      }
      pi->mix_gain->nb_seg = p->nb_segments;
      pi->mix_gain->mix_gain_uints = IAMF_MALLOCZ(MixGainUnit, p->nb_segments);
      if (!pi->mix_gain->mix_gain_uints) {
        ia_loge("Fail to allocate memory for segment mix gains.");
        return IAMF_ERR_ALLOC_FAIL;
      }

      for (int i = 0; i < p->nb_segments; ++i) {
        seg = (MixGainSegment *)p->segments[i];
        pi->mix_gain->mix_gain_uints[i].count = seg->seg.segment_interval;
        pi->mix_gain->mix_gain_uints[i].constant_gain = 1.0f;
        gain_db = q_to_float(seg->mix_gain.start, 8);
        if (seg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_STEP) {
          pi->mix_gain->mix_gain_uints[i].constant_gain = db2lin(gain_db);
          ia_logd("mix gain %f(%f db <%x>)",
                  pi->mix_gain->mix_gain_uints[i].constant_gain, gain_db,
                  seg->mix_gain.control & U16_MASK);
        } else if (seg->mix_gain.animated_type ==
                   PARAMETER_ANIMATED_TYPE_LINEAR) {
          gain_l = db2lin(gain_db);
          gain1_db = q_to_float(seg->mix_gain.end, 8);
          gain1_l = db2lin(gain1_db);
          pi->mix_gain->mix_gain_uints[i].gains =
              IAMF_MALLOCZ(float, seg->seg.segment_interval);
          if (pi->mix_gain->mix_gain_uints[i].gains) {
            iamf_object_parameter_mix_gain_linear(
                seg->seg.segment_interval, gain_l, gain1_l,
                pi->mix_gain->mix_gain_uints[i].gains);
          }
        } else if (seg->mix_gain.animated_type ==
                   PARAMETER_ANIMATED_TYPE_BEZIER) {
          gain_l = db2lin(gain_db);
          gain1_db = q_to_float(seg->mix_gain.end, 8);
          gain1_l = db2lin(gain1_db);
          gain2_db = q_to_float(seg->mix_gain.control, 8);
          gain2_l = db2lin(gain2_db);
          m = round(qf_to_float(seg->mix_gain.control_relative_time, 8) *
                    seg->seg.segment_interval) -
              1;
          pi->mix_gain->mix_gain_uints[i].gains =
              IAMF_MALLOCZ(float, seg->seg.segment_interval);
          if (pi->mix_gain->mix_gain_uints[i].gains) {
            iamf_object_parameter_mix_gain_quad(
                seg->seg.segment_interval, m, gain_l, gain2_l, gain1_l,
                pi->mix_gain->mix_gain_uints[i].gains);
          }
        }
      }
    }
  } else {
    iamf_object_set_add(db->parameters, obj);
    ia_logw("Can not find parameter item for paramter (%lu)", p->id);
  }

  return IAMF_OK;
}

static void iamf_database_parameter_viewer_free(ParameterViewer *v) {
  if (v) {
    if (v->items) {
      for (int i = 0; i < v->count; ++i) {
        if (v->items[i]->mix_gain) {
          if (v->items[i]->mix_gain->mix_gain_uints) {
            for (int s = 0; s < v->items[i]->mix_gain->nb_seg; ++s)
              free(v->items[i]->mix_gain->mix_gain_uints[s].gains);
            free(v->items[i]->mix_gain->mix_gain_uints);
          }
          free(v->items[i]->mix_gain);
        }
        free(v->items[i]);
      }
      free(v->items);
    }
    memset(v, 0, sizeof(ParameterViewer));
  }
}

static ParameterBase *iamf_database_parameter_viewer_get_parmeter_base(
    IAMF_DataBase *db, uint64_t pid) {
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  return pi ? pi->param_base : 0;
}

static int iamf_database_element_viewer_add(IAMF_DataBase *db,
                                            IAMF_Object *obj) {
  int ret = IAMF_OK;
  ElementItem *eItem = 0;
  ElementViewer *v = &db->eViewer;
  IAMF_Element *e = (IAMF_Element *)obj;

  eItem = IAMF_REALLOC(ElementItem, v->items, v->count + 1);
  if (!eItem) return IAMF_ERR_ALLOC_FAIL;

  v->items = eItem;
  ret = iamf_object_set_add(db->element, (void *)obj);
  if (ret != IAMF_OK) {
    return ret;
  }

  eItem = &v->items[v->count++];
  memset(eItem, 0, sizeof(ElementItem));

  eItem->id = e->element_id;
  eItem->element = IAMF_ELEMENT(obj);
  eItem->codecConf = iamf_database_get_codec_conf(db, e->codec_config_id);
  eItem->recon_gain_flags = iamf_object_element_get_recon_gain_flags(e);

  for (int i = 0; i < e->nb_parameters; ++i) {
    iamf_database_parameter_viewer_add_item(db, e->parameters[i], eItem->id);
  }

  return ret;
}

static void iamf_database_element_viewer_reset(ElementViewer *v) {
  if (v) {
    if (v->items) free(v->items);
    memset(v, 0, sizeof(ElementViewer));
  }
}

static int iamf_database_sync_time_update(IAMF_DataBase *db) {
  ElementViewer *ev = &db->eViewer;
  ElementItem *ei = (ElementItem *)ev->items;

  ParameterViewer *pv = &db->pViewer;
  ParameterItem *pi;

  for (int i = 0; i < ev->count; ++i) {
    ei->timestamp = iamf_database_object_get_timestamp(
        db, IAMF_frame_get_obu_type(ei->element->substream_ids[0]));
  }

  for (int i = 0; i < pv->count; ++i) {
    pi = pv->items[i];
    pi->timestamp = iamf_database_object_get_timestamp(db, pi->id);
  }

  return IAMF_OK;
}

static uint64_t iamf_database_sync_viewer_max_start_timestamp(SyncViewer *v) {
  uint64_t ret = 0;
  SyncItem *si;
  if (!v) return 0;
  si = v->items;
  ret = 0;
  for (int i = 0; i < v->count; ++i)
    if (!si[i].type && ret < si[i].start) ret = si[i].start;
  return ret;
}

static void iamf_database_sync_viewer_reset(SyncViewer *v) {
  if (v) {
    if (v->items) free(v->items);
    memset(v, 0, sizeof(SyncViewer));
  }
}

static int iamf_database_sync_viewer_update(IAMF_DataBase *db,
                                            IAMF_Object *obj) {
  IAMF_Sync *s = (IAMF_Sync *)obj;
  SyncItem *si;
  uint64_t start = 0;

  si = IAMF_MALLOCZ(SyncItem, s->nb_obu_ids);
  if (!si) {
    ia_loge("Fail to allocate memory for Sync Items.");
    return IAMF_ERR_ALLOC_FAIL;
  }

  start = iamf_database_sync_viewer_max_start_timestamp(&db->sViewer);
  ia_logi("max end timestamp is %lu", start);
  for (int i = 0; i < s->nb_obu_ids; ++i) {
    si[i].id = s->objs[i].obu_id;
    si[i].type = s->objs[i].obu_data_type;
    si[i].start = start + s->objs[i].relative_offset + s->global_offset;
    ia_logi("Item id %lu: type %d, start time %lu", si[i].id,
            s->objs[i].obu_data_type & U8_MASK, si[i].start);
  }

  if (db->sync) iamf_object_free(db->sync);
  db->sync = obj;

  iamf_database_sync_viewer_reset(&db->sViewer);
  db->sViewer.start_global_time =
      si[0].start - s->objs[0].relative_offset - s->global_offset;
  db->sViewer.items = si;
  db->sViewer.count = s->nb_obu_ids;

  iamf_database_sync_time_update(db);

  return IAMF_OK;
}

static int iamf_database_sync_viewer_check_id(IAMF_DataBase *db, uint64_t id) {
  for (int i = 0; i < db->sViewer.count; ++i) {
    if (db->sViewer.items[i].id == id) return 1;
  }
  return 0;
}

static int iamf_database_mix_presentation_get_label_index(IAMF_DataBase *db,
                                                          const char *label) {
  int idx = INAVLID_INDEX;
  IAMF_MixPresentation *mp;
  for (int i = 0; i < db->mixPresentation->count; ++i) {
    mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
    if (!strcmp(label, mp->mix_presentation_friendly_label)) {
      idx = i;
      break;
    }
  }
  return idx;
}

int iamf_database_init(IAMF_DataBase *db) {
  memset(db, 0, sizeof(IAMF_DataBase));

  db->codecConf = iamf_object_set_new(iamf_object_free);
  db->element = iamf_object_set_new(iamf_object_free);
  db->mixPresentation = iamf_object_set_new(iamf_object_free);
  db->parameters = iamf_object_set_new(iamf_object_free);

  if (!db->codecConf || !db->element || !db->mixPresentation ||
      !db->parameters) {
    iamf_database_reset(db);
    return IAMF_ERR_ALLOC_FAIL;
  }
  return 0;
}

void iamf_database_reset(IAMF_DataBase *db) {
  if (db) {
    if (db->version) iamf_object_free(db->version);

    if (db->sync) iamf_object_free(db->sync);

    if (db->codecConf) iamf_object_set_free(db->codecConf);

    if (db->element) iamf_object_set_free(db->element);

    if (db->mixPresentation) iamf_object_set_free(db->mixPresentation);

    if (db->parameters) iamf_object_set_free(db->parameters);

    if (db->eViewer.items) iamf_database_element_viewer_reset(&db->eViewer);

    if (db->pViewer.items) iamf_database_parameter_viewer_free(&db->pViewer);

    if (db->sViewer.items) iamf_database_sync_viewer_reset(&db->sViewer);

    memset(db, 0, sizeof(IAMF_DataBase));
  }
}

static int iamf_database_add_object(IAMF_DataBase *db, IAMF_Object *obj) {
  int ret = IAMF_OK;
  switch (obj->type) {
    case IAMF_OBU_MAGIC_CODE:
      if (db->version) {
        ia_logw("WARNING : Receive Multiple START CODE OBUs !!!");
        free(db->version);
      }
      db->version = obj;
      break;
    case IAMF_OBU_CODEC_CONFIG:
      ret = iamf_object_set_add(db->codecConf, (void *)obj);
      break;
    case IAMF_OBU_AUDIO_ELEMENT:
      ret = iamf_database_element_viewer_add(db, obj);
      break;
    case IAMF_OBU_MIX_PRESENTATION:
      ret = iamf_object_set_add(db->mixPresentation, (void *)obj);
      break;
    case IAMF_OBU_PARAMETER_BLOCK:
      ret = iamf_database_parameter_viewer_add(db, obj);
      break;
    case IAMF_OBU_SYNC:
      ret = iamf_database_sync_viewer_update(db, obj);
      break;
    default:
      ia_logd("IAMF Object %s (%d) is not needed in database.",
              IAMF_OBU_type_string(obj->type), obj->type);
      IAMF_object_free(obj);
  }
  return ret;
}

IAMF_CodecConf *iamf_database_get_codec_conf(IAMF_DataBase *db, uint64_t cid) {
  IAMF_CodecConf *ret = 0;

  if (db->codecConf) {
    IAMF_CodecConf *c = 0;
    for (uint32_t i = 0; i < db->codecConf->count; ++i) {
      c = (IAMF_CodecConf *)db->codecConf->items[i];
      if (c->codec_conf_id == cid) {
        ret = c;
        break;
      }
    }
  }
  return ret;
}

static IAMF_Element *iamf_database_get_element(IAMF_DataBase *db,
                                               uint64_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  return ei ? ei->element : 0;
}

static IAMF_Element *iamf_database_get_element_by_parameterID(IAMF_DataBase *db,
                                                              uint64_t pid) {
  IAMF_Element *elem = 0;
  IAMF_Element *e = 0;
  for (int i = 0; i < db->eViewer.count; ++i) {
    e = IAMF_ELEMENT(db->eViewer.items[i].element);
    for (int p = 0; p < e->nb_parameters; ++p) {
      if (pid == e->parameters[p]->id) {
        elem = e;
        break;
      }
    }
  }
  return elem;
}

ElementItem *iamf_database_element_get_item(IAMF_DataBase *db, uint64_t eid) {
  ElementItem *ei = 0;
  for (int i = 0; i < db->eViewer.count; ++i) {
    if (db->eViewer.items[i].id == eid) {
      ei = &db->eViewer.items[i];
      break;
    }
  }
  return ei;
}

static IAMF_CodecConf *iamf_database_element_get_codec_conf(IAMF_DataBase *db,
                                                            uint64_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  return ei ? ei->codecConf : 0;
}

static int iamf_database_element_get_substream_index(IAMF_DataBase *db,
                                                     uint64_t element_id,
                                                     uint64_t substream_id) {
  IAMF_Element *obj = iamf_database_get_element(db, element_id);
  int ret = -1;

  if (obj) {
    for (int i = 0; i < obj->nb_substreams; ++i) {
      if (obj->substream_ids[i] == substream_id) {
        ret = i;
        break;
      }
    }
  }
  return ret;
}

static uint64_t iamf_database_element_get_timestamp(IAMF_DataBase *db,
                                                    uint32_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  return ei ? ei->timestamp : INVALID_ID;
}

static uint32_t iamf_database_element_get_recon_gain_flags(IAMF_DataBase *db,
                                                           uint32_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  return ei ? ei->recon_gain_flags : 0;
}

static int iamf_database_element_time_elapse(IAMF_DataBase *db, uint64_t eid,
                                             uint64_t duration) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  SyncItem *si = db->sViewer.items;

  if (!ei) return IAMF_ERR_BAD_ARG;
  ei->timestamp += duration;
  ia_logt("element item %p, id %lu time to %lu", ei, eid, ei->timestamp);

  for (int i = 0; i < ei->element->nb_substreams; ++i) {
    for (int a = 0; a < db->sViewer.count; ++a) {
      if (ei->element->substream_ids[i] == si[a].id) {
        si[a].start = ei->timestamp;
        break;
      }
    }
  }

  return IAMF_OK;
}

static int iamf_database_element_get_demix_mode(IAMF_DataBase *db,
                                                uint64_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  ParameterItem *pi = 0;
  DemixingSegment *seg = 0;
  uint64_t start = 0;
  if (!ei) return IAMF_ERR_BAD_ARG;

  pi = ei->demixing;
  if (!pi) return IAMF_ERR_BAD_ARG;

  if (ei->timestamp > pi->timestamp) start = ei->timestamp - pi->timestamp;
  seg = (DemixingSegment *)iamf_object_parameter_get_segment(pi->parameter,
                                                             start);

  if (seg) return seg->demixing_mode;
  return IAMF_ERR_INTERNAL;
}

static ReconGainList *iamf_database_element_get_recon_gain_list(
    IAMF_DataBase *db, uint64_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  ParameterItem *pi = 0;
  ReconGainSegment *seg = 0;
  uint64_t start = 0;
  if (!ei) return 0;

  pi = ei->reconGain;
  if (!pi) return 0;

  if (ei->timestamp > pi->timestamp) start = ei->timestamp - pi->timestamp;
  seg = (ReconGainSegment *)iamf_object_parameter_get_segment(pi->parameter,
                                                              start);

  if (seg) return &seg->list;

  return 0;
}

static int iamf_database_element_set_mix_gain_parameter(IAMF_DataBase *db,
                                                        uint64_t eid,
                                                        uint64_t pid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);

  if (ei) {
    ei->mixGain = pi;
  }

  return IAMF_OK;
}

static MixGain *iamf_database_element_get_mix_gain(IAMF_DataBase *db,
                                                   uint64_t eid, uint64_t ts,
                                                   int duration) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);

  if (ei && ei->mixGain && ei->mixGain->mix_gain) {
    if (ei->mixGain->timestamp == ts ||
        (!ei->mixGain->mix_gain->nb_seg &&
         ei->mixGain->mix_gain->default_mix_gain != .0f))
      return ei->mixGain->mix_gain;
  }

  return 0;
}

/* <<<<<<<<<<<<<<<<<< DATABASE <<<<<<<<<<<<<<<<<< */

/* <<<<<<<<<<<<<<<<<< STREAM DECODER MIXER RESAMPLER <<<<<<<<<<<<<<<<<< */

static int iamf_stream_set_output_layout(IAMF_Stream *s, LayoutInfo *layout);
static uint32_t iamf_stream_mode_ambisonics(uint32_t ambisonics_mode);
static void iamf_stream_free(IAMF_Stream *s);
static void iamf_stream_decoder_close(IAMF_StreamDecoder *d);
static void iamf_mixer_reset(IAMF_Mixer *m);
static int iamf_stream_scale_decoder_update_recon_gain(
    IAMF_StreamDecoder *decoder, ReconGainList *list);
static SpeexResamplerState *iamf_stream_resampler_open(IAMF_Stream *stream,
                                                       uint32_t in_rate,
                                                       uint32_t out_rate,
                                                       int quality);
static void iamf_stream_resampler_close(SpeexResamplerState *r);

static void iamf_presentation_free(IAMF_Presentation *pst) {
  if (pst) {
    for (int i = 0; i < pst->nb_streams; ++i) {
      if (pst->decoders[i]) {
        iamf_stream_decoder_close(pst->decoders[i]);
      }

      if (pst->streams[i]) {
        iamf_stream_free(pst->streams[i]);
      }
    }
    if (pst->resampler) {
      iamf_stream_resampler_close(pst->resampler);
    }
    free(pst->decoders);
    free(pst->streams);
    iamf_mixer_reset(&pst->mixer);
    free(pst);
  }
}

static IAMF_Stream *iamf_presentation_take_stream(IAMF_Presentation *pst,
                                                  uint64_t eid) {
  IAMF_Stream *stream = 0;

  if (!pst) return 0;

  for (int i = 0; i < pst->nb_streams; ++i) {
    if (pst->streams[i]->element_id == eid) {
      stream = pst->streams[i];
      pst->streams[i] = 0;
      break;
    }
  }

  return stream;
}

static IAMF_StreamDecoder *iamf_presentation_take_decoder(
    IAMF_Presentation *pst, IAMF_Stream *stream) {
  IAMF_StreamDecoder *decoder = 0;
  for (int i = 0; i < pst->nb_streams; ++i) {
    if (pst->decoders[i]->stream == stream) {
      decoder = pst->decoders[i];
      pst->decoders[i] = 0;
      break;
    }
  }

  return decoder;
}

static SpeexResamplerState *iamf_presentation_take_resampler(
    IAMF_Presentation *pst) {
  SpeexResamplerState *resampler = 0;
  resampler = pst->resampler;
  pst->resampler = 0;

  return resampler;
}

static int iamf_presentation_reuse_stream(IAMF_Presentation *dst,
                                          IAMF_Presentation *src,
                                          uint64_t eid) {
  IAMF_Stream *stream = 0;
  IAMF_StreamDecoder *decoder = 0;
  IAMF_Stream **streams;
  IAMF_StreamDecoder **decoders;

  if (!dst || !src) return IAMF_ERR_BAD_ARG;

  stream = iamf_presentation_take_stream(src, eid);
  if (!stream) return IAMF_ERR_INTERNAL;

  decoder = iamf_presentation_take_decoder(src, stream);
  if (!decoder) return IAMF_ERR_INTERNAL;

  streams = IAMF_REALLOC(IAMF_Stream *, dst->streams, dst->nb_streams + 1);
  if (!streams) return IAMF_ERR_INTERNAL;
  dst->streams = streams;

  decoders =
      IAMF_REALLOC(IAMF_StreamDecoder *, dst->decoders, dst->nb_streams + 1);
  if (!decoders) return IAMF_ERR_INTERNAL;
  dst->decoders = decoders;

  dst->streams[dst->nb_streams] = stream;
  dst->decoders[dst->nb_streams] = decoder;
  ++dst->nb_streams;
  ia_logd("reuse stream with element id %lu", eid);

  return 0;
}

/**
 * Output sound mode:
 *
 * |---------------------------------------------------------------------|
 * | Elem B\Elem A     | Mono   | Stereo | Binaural | > 2-ch | Ambisonic |
 * |---------------------------------------------------------------------|
 * | Mono              | Stereo | Stereo | N/A      | M-ch   | SPL       |
 * |---------------------------------------------------------------------|
 * | Stereo            | Stereo | Stereo | N/A      | M-ch   | SPL       |
 * |---------------------------------------------------------------------|
 * | Binaural          | N/A    | N/A    | Binaural | N/A    | N/A       |
 * |---------------------------------------------------------------------|
 * | > 2-ch            | M-ch   | M-ch   | N/A      | M-ch   | SPL       |
 * |---------------------------------------------------------------------|
 * | Ambisonic         | SPL    | SPL    | N/A      | SPL    | SPL       |
 * |---------------------------------------------------------------------|
 *
 * 2-ch : 2 channels
 * M-ch : Multichannel
 * SPL : Same to playback layout, it means:
 *
 * If (Output_sound_mode == ?Same to playback layout?)
 * {
 *    If (Output_sound_system == ?Sound system A (0+2+0)?) { Output_sound_mode =
 * Stereo; } Else { Output_sound_mode = Multichannel; }
 * }
 *
 * */
static IAMF_SoundMode iamf_presentation_get_output_sound_mode(
    IAMF_Presentation *pst) {
  IAMF_Stream *st;
  IAMF_SoundMode mode = IAMF_SOUND_MODE_NONE, sm;

  for (int s = 0; s < pst->nb_streams; ++s) {
    st = pst->streams[s];
    if (st->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED)
      sm = iamf_layout_get_sound_mode(&st->final_layout->layout);
    else {
      ChannelLayerContext *cctx = (ChannelLayerContext *)st->priv;
      int type = cctx->layout;
      if (type == IA_CHANNEL_LAYOUT_MONO || type == IA_CHANNEL_LAYOUT_STEREO)
        sm = IAMF_SOUND_MODE_STEREO;
      else if (type == IA_CHANNEL_LAYOUT_BINAURAL)
        sm = IAMF_SOUND_MODE_BINAURAL;
      else
        sm = IAMF_SOUND_MODE_MULTICHANNEL;
    }
    mode = iamf_sound_mode_combine(mode, sm);
  }

  return mode;
}

void iamf_stream_free(IAMF_Stream *s) {
  if (s) {
    if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
      ChannelLayerContext *ctx = s->priv;
      if (ctx) {
        if (ctx->conf_s) {
          for (int i = 0; i < ctx->nb_layers; ++i) {
            if (ctx->conf_s[i].output_gain) {
              free(ctx->conf_s[i].output_gain);
            }
            if (ctx->conf_s[i].recon_gain) {
              free(ctx->conf_s[i].recon_gain);
            }
          }
          free(ctx->conf_s);
        }
        free(ctx);
      }
    } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
      AmbisonicsContext *ctx = s->priv;
      free(ctx);
    }
    free(s);
  }
}

static IAMF_Stream *iamf_stream_new(IAMF_Element *elem, IAMF_CodecConf *conf,
                                    LayoutInfo *layout) {
  IAMF_Stream *stream = IAMF_MALLOCZ(IAMF_Stream, 1);
  if (!stream) goto stream_fail;

  stream->element_id = elem->element_id;
  stream->scheme = elem->element_type;
  stream->codecConf_id = conf->codec_conf_id;
  stream->codec_id = iamf_codec_4cc_get_codecID(conf->codec_id);
  stream->sampling_rate = iamf_object_codec_conf_get_sampling_rate(conf);
  stream->nb_substreams = elem->nb_substreams;

  ia_logd("codec conf id %lu, codec id 0x%x (%s), sampling rate is %u",
          conf->codec_conf_id, conf->codec_id,
          iamf_codec_name(stream->codec_id), stream->sampling_rate);
  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ChannelLayerContext *ctx = IAMF_MALLOCZ(ChannelLayerContext, 1);
    SubLayerConf *sub_conf;
    ChannelLayerConf *layer_conf;
    ScalableChannelLayoutConf *layers_conf = elem->channels_conf;
    float gain_db;
    int chs = 0;
    IAChannelLayoutType last = IA_CHANNEL_LAYOUT_INVALID;

    if (!ctx) {
      goto stream_fail;
    }

    stream->priv = (void *)ctx;
    ctx->nb_layers = layers_conf->nb_layers;
    if (ctx->nb_layers) {
      sub_conf = IAMF_MALLOCZ(SubLayerConf, ctx->nb_layers);
      if (!sub_conf) {
        goto stream_fail;
      }

      ctx->conf_s = sub_conf;
      for (int i = 0; i < ctx->nb_layers; ++i) {
        sub_conf = &ctx->conf_s[i];
        layer_conf = &layers_conf->layer_conf_s[i];
        sub_conf->layout = layer_conf->loudspeaker_layout;
        sub_conf->nb_substreams = layer_conf->nb_substreams;
        sub_conf->nb_coupled_substreams = layer_conf->nb_coupled_substreams;
        sub_conf->nb_channels =
            sub_conf->nb_substreams + sub_conf->nb_coupled_substreams;

        ia_logi("audio layer %d :", i);
        ia_logi(" > loudspeaker layout %s(%d) .",
                ia_channel_layout_name(sub_conf->layout), sub_conf->layout);
        ia_logi(" > sub-stream count %d .", sub_conf->nb_substreams);
        ia_logi(" > coupled sub-stream count %d .",
                sub_conf->nb_coupled_substreams);

        if (layer_conf->output_gain_flag) {
          sub_conf->output_gain = IAMF_MALLOCZ(IAMF_OutputGain, 1);
          if (!sub_conf->output_gain) {
            ia_loge("Fail to allocate memory for output gain of sub config.");
            goto stream_fail;
          }
          sub_conf->output_gain->flags =
              layer_conf->output_gain_info->output_gain_flag;
          gain_db = q_to_float(layer_conf->output_gain_info->output_gain, 8);
          sub_conf->output_gain->gain = db2lin(gain_db);
          ia_logi(" > output gain flags 0x%02x",
                  sub_conf->output_gain->flags & U8_MASK);
          ia_logi(" > output gain %f (0x%04x), linear gain %f", gain_db,
                  layer_conf->output_gain_info->output_gain & U16_MASK,
                  sub_conf->output_gain->gain);
        } else {
          ia_logi(" > no output gain info.");
        }

        if (layer_conf->recon_gain_flag) {
          sub_conf->recon_gain = IAMF_MALLOCZ(IAMF_ReconGain, 1);
          if (!sub_conf->recon_gain) {
            ia_loge("Fail to allocate memory for recon gain of sub config.");
            goto stream_fail;
          }
          ctx->recon_gain_flags |= RSHIFT(i);
          ia_logi(" > wait recon gain info.");
        } else {
          ia_logi(" > no recon gain info.");
        }

        chs += iamf_channel_layout_get_new_channels(
            last, sub_conf->layout, &ctx->channels_order[chs],
            IA_CH_LAYOUT_MAX_CHANNELS - chs);

        stream->nb_coupled_substreams += sub_conf->nb_coupled_substreams;

        ia_logi(" > the total of %d channels", chs);
        last = sub_conf->layout;
      }
    }
    stream->nb_channels = stream->nb_substreams + stream->nb_coupled_substreams;

    ia_logi("channels %d, streams %d, coupled streams %d.", stream->nb_channels,
            stream->nb_substreams, stream->nb_coupled_substreams);

    ia_logi("all channels order:");
    for (int c = 0; c < stream->nb_channels; ++c)
      ia_logi("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
              ctx->channels_order[c]);

    ctx->layer = ctx->nb_layers - 1;
    iamf_stream_set_output_layout(stream, layout);
    ctx->layout = ctx->conf_s[ctx->layer].layout;
    ctx->channels = ia_channel_layout_get_channels_count(ctx->layout);
    ctx->dmx_mode = -1;

    ia_logi("initialized layer %d, layout %s (%d), layout channel count %d.",
            ctx->layer, ia_channel_layout_name(ctx->layout), ctx->layout,
            ctx->channels);

  } else {
    AmbisonicsConf *aconf = elem->ambisonics_conf;
    AmbisonicsContext *ctx;
    stream->nb_channels = aconf->output_channel_count;
    stream->nb_substreams = aconf->substream_count;
    stream->nb_coupled_substreams = aconf->coupled_substream_count;

    ctx = IAMF_MALLOCZ(AmbisonicsContext, 1);
    if (!ctx) {
      goto stream_fail;
    }

    stream->priv = (void *)ctx;
    ctx->mode = iamf_stream_mode_ambisonics(aconf->ambisonics_mode);
    ctx->mapping = aconf->mapping;
    ctx->mapping_size = aconf->mapping_size;

    iamf_stream_set_output_layout(stream, layout);
    ia_logi("stream mode %d", ctx->mode);
  }
  return stream;

stream_fail:

  if (stream) iamf_stream_free(stream);
  return 0;
}

uint32_t iamf_stream_mode_ambisonics(uint32_t ambisonics_mode) {
  if (ambisonics_mode == AMBISONICS_MODE_MONO)
    return STREAM_MODE_AMBISONICS_MONO;
  else if (ambisonics_mode == AMBISONICS_MODE_PROJECTION)
    return STREAM_MODE_AMBISONICS_PROJECTION;
  return STREAM_MODE_AMBISONICS_NONE;
}

int iamf_stream_set_output_layout(IAMF_Stream *s, LayoutInfo *layout) {
  s->final_layout = layout;

  if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ChannelLayerContext *ctx = (ChannelLayerContext *)s->priv;
    if (ctx) {
      if (ctx->nb_layers == 1) {
        return IAMF_OK;
      }

      // use the layout that matches the playback layout
      for (int i = 0; i < ctx->nb_layers; ++i) {
        if (iamf_layer_layout_convert_sound_system(ctx->conf_s[i].layout) ==
            layout->layout.sound_system.sound_system) {
          ctx->layer = i;
          ia_logi("scalabel channels layer is %d", i);
          return IAMF_OK;
        }
      }

      // select next highest available layout
      int playback_channels = IAMF_layout_sound_system_channels_count(
          layout->layout.sound_system.sound_system);
      for (int i = 0; i < ctx->nb_layers; ++i) {
        int channels =
            ia_channel_layout_get_channels_count(ctx->conf_s[i].layout);
        if (channels > playback_channels) {
          ctx->layer = i;
          ia_logi("scalabel channels layer is %d", i);
          return IAMF_OK;
        }
      }
    }
  }

  return IAMF_OK;
}

static int iamf_stream_enable(IAMF_DecoderHandle handle, IAMF_Element *elem) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Presentation *pst = ctx->presentation;
  uint64_t element_id;
  IAMF_Stream *stream = 0;
  IAMF_Stream **streams;
  IAMF_CodecConf *conf;
  IAMF_StreamDecoder *decoder = 0;
  IAMF_StreamDecoder **decoders;
  Packet *pkt = 0;
  ElementItem *ei;

  ia_logd("enable element id %lu", elem->element_id);
  element_id = elem->element_id;
  conf = iamf_database_element_get_codec_conf(db, element_id);
  ia_logd("codec conf id %lu", conf->codec_conf_id);

  stream = iamf_stream_new(elem, conf, ctx->output_layout);
  if (!stream) goto stream_enable_fail;

  decoder = iamf_stream_decoder_open(stream, conf);
  if (!decoder) goto stream_enable_fail;

  streams = IAMF_REALLOC(IAMF_Stream *, pst->streams, pst->nb_streams + 1);
  if (!streams) goto stream_enable_fail;
  pst->streams = streams;

  decoders =
      IAMF_REALLOC(IAMF_StreamDecoder *, pst->decoders, pst->nb_streams + 1);
  if (!decoders) goto stream_enable_fail;
  pst->decoders = decoders;

  pst->streams[pst->nb_streams] = stream;
  pst->decoders[pst->nb_streams] = decoder;
  ++pst->nb_streams;

  pkt = &decoder->packet;
  ei = iamf_database_element_get_item(db, stream->element_id);

  if (ei) {
    if (ei->demixing) pkt->pmask |= IAMF_PACKET_EXTRA_DEMIX_MODE;
    if (ei->reconGain) pkt->pmask |= IAMF_PACKET_EXTRA_RECON_GAIN;
  }

  return 0;

stream_enable_fail:
  if (decoder) iamf_stream_decoder_close(decoder);

  if (stream) iamf_stream_free(stream);

  return IAMF_ERR_ALLOC_FAIL;
}

SpeexResamplerState *iamf_stream_resampler_open(IAMF_Stream *stream,
                                                uint32_t in_rate,
                                                uint32_t out_rate,
                                                int quality) {
  int err = 0;
  uint32_t channels = stream->final_layout->channels;
  SpeexResamplerState *resampler =
      speex_resampler_init(channels, in_rate, out_rate, quality, &err);
  ia_logi("in sample rate %u, out sample rate %u", in_rate, out_rate);
  if (err != RESAMPLER_ERR_SUCCESS) goto open_fail;
  speex_resampler_skip_zeros(resampler);
  resampler->buffer = IAMF_MALLOCZ(float, MAX_FRAME_SIZE *channels);
  if (!resampler->buffer) goto open_fail;
  return resampler;
open_fail:
  if (resampler) iamf_stream_resampler_close(resampler);
  return 0;
}

void iamf_stream_resampler_close(SpeexResamplerState *r) {
  if (r) {
    if (r->buffer) free(r->buffer);
    speex_resampler_destroy(r);
  }
}

static IACoreDecoder *iamf_stream_sub_decoder_open(
    int mode, int channels, int nb_streams, int nb_coupled_streams,
    uint8_t *mapping, int mapping_size, IAMF_CodecConf *conf) {
  IACodecID cid;
  IACoreDecoder *cDecoder;
  int ret = 0;

  cid = iamf_codec_4cc_get_codecID(conf->codec_id);
  cDecoder = ia_core_decoder_open(cid);

  if (cDecoder) {
    ia_core_decoder_set_codec_conf(cDecoder, conf->decoder_conf,
                                   conf->decoder_conf_size);
    ia_logd(
        "codec %s, mode %d, channels %d, streams %d, coupled streams %d, "
        "mapping size  %d",
        iamf_codec_name(cid), mode, channels, nb_streams, nb_coupled_streams,
        mapping_size);
    ia_core_decoder_set_streams_info(cDecoder, mode, channels, nb_streams,
                                     nb_coupled_streams, mapping, mapping_size);
    ret = ia_core_decoder_init(cDecoder);
    if (ret != IAMF_OK) {
      ia_loge("Fail to initalize core decoder.");
      ia_core_decoder_close(cDecoder);
      cDecoder = 0;
    }
  }

  return cDecoder;
}

static int iamf_stream_decoder_decode_finish(IAMF_StreamDecoder *decoder);

void iamf_stream_decoder_close(IAMF_StreamDecoder *d) {
  if (d) {
    IAMF_Stream *s = d->stream;

    IAMF_FREE(d->packet.sub_packets);
    IAMF_FREE(d->packet.sub_packet_sizes);

    for (int i = 0; i < DEC_BUF_CNT; ++i) {
      IAMF_FREE(d->buffers[i]);
    }

    if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
      if (d->scale) {
        if (d->scale->sub_decoders) {
          for (int i = 0; i < d->scale->nb_layers; ++i)
            ia_core_decoder_close(d->scale->sub_decoders[i]);
          free(d->scale->sub_decoders);
        }
        if (d->scale->demixer) demixer_close(d->scale->demixer);
        free(d->scale);
      }
    } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
      if (d->ambisonics) {
        if (d->ambisonics->decoder)
          ia_core_decoder_close(d->ambisonics->decoder);
        free(d->ambisonics);
      }
    }
    free(d);
  }
}

IAMF_StreamDecoder *iamf_stream_decoder_open(IAMF_Stream *stream,
                                             IAMF_CodecConf *conf) {
  IAMF_StreamDecoder *decoder;
  int channels = 0;

  decoder = IAMF_MALLOCZ(IAMF_StreamDecoder, 1);

  if (!decoder) goto open_fail;

  decoder->stream = stream;
  decoder->frame_size = conf->nb_samples_per_frame;
  decoder->packet.nb_sub_packets = stream->nb_substreams;
  decoder->packet.sub_packets = IAMF_MALLOCZ(uint8_t *, stream->nb_substreams);
  decoder->packet.sub_packet_sizes =
      IAMF_MALLOCZ(uint32_t, stream->nb_substreams);

  if (!decoder->packet.sub_packets || !decoder->packet.sub_packet_sizes)
    goto open_fail;

  ia_logt("check channels.");
  channels = stream->final_layout->channels;
  ia_logd("final target channels vs stream original channels (%d vs %d).",
          channels, stream->nb_channels);
  if (channels < stream->nb_channels) {
    channels = stream->nb_channels;
  }
  for (int i = 0; i < DEC_BUF_CNT; ++i) {
    decoder->buffers[i] = IAMF_MALLOC(float, MAX_FRAME_SIZE *channels);
    if (!decoder->buffers[i]) goto open_fail;
  }

  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ScalableChannelDecoder *scale = IAMF_MALLOCZ(ScalableChannelDecoder, 1);
    ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
    IACoreDecoder **sub_decoders;
    IACoreDecoder *sub;

    if (!scale) goto open_fail;

    decoder->scale = scale;
    scale->nb_layers = ctx->layer + 1;
    sub_decoders = IAMF_MALLOCZ(IACoreDecoder *, scale->nb_layers);
    if (!sub_decoders) goto open_fail;
    scale->sub_decoders = sub_decoders;
    ia_logi("open sub decdoers for channel-based.");
    for (int i = 0; i < scale->nb_layers; ++i) {
      sub = iamf_stream_sub_decoder_open(
          STREAM_MODE_AMBISONICS_NONE, ctx->conf_s[i].nb_channels,
          ctx->conf_s[i].nb_substreams, ctx->conf_s[i].nb_coupled_substreams, 0,
          0, conf);
      if (!sub) goto open_fail;
      sub_decoders[i] = sub;
    }

    ia_logi("open demixer.");
    scale->demixer = demixer_open(conf->nb_samples_per_frame,
                                  iamf_codec_get_delay(stream->codec_id));
    if (!scale->demixer) goto open_fail;
    iamf_stream_scale_demixer_configure(decoder);
  } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
    AmbisonicsDecoder *a = IAMF_MALLOCZ(AmbisonicsDecoder, 1);
    AmbisonicsContext *ctx = (AmbisonicsContext *)stream->priv;
    if (!a) goto open_fail;
    decoder->ambisonics = a;

    ia_logi("open sub decdoers for ambisonics.");
    a->decoder = iamf_stream_sub_decoder_open(
        ctx->mode, stream->nb_channels, stream->nb_substreams,
        stream->nb_coupled_substreams, ctx->mapping, ctx->mapping_size, conf);

    if (!a->decoder) goto open_fail;
  }

  return decoder;

open_fail:
  if (decoder) iamf_stream_decoder_close(decoder);
  return 0;
}

static int iamf_stream_decoder_check_prepared(IAMF_StreamDecoder *decoder) {
  Packet *pkt = &decoder->packet;
  return pkt->pmask == pkt->uflags && pkt->count == pkt->nb_sub_packets;
}

static int iamf_stream_decoder_receive_packet(IAMF_StreamDecoder *decoder,
                                              int substream_index,
                                              IAMF_Frame *packet) {
  ia_logd("stream decoder %lu , recevie sub stream %d",
          decoder->stream->element_id, substream_index);

  if (substream_index > INAVLID_INDEX &&
      substream_index < decoder->packet.nb_sub_packets) {
    if (!decoder->packet.sub_packets[substream_index]) ++decoder->packet.count;
    IAMF_FREE(decoder->packet.sub_packets[substream_index]);
    decoder->packet.sub_packets[substream_index] =
        IAMF_MALLOC(uint8_t, packet->size);
    if (!decoder->packet.sub_packets[substream_index])
      return IAMF_ERR_ALLOC_FAIL;
    memcpy(decoder->packet.sub_packets[substream_index], packet->data,
           packet->size);
    decoder->packet.sub_packet_sizes[substream_index] = packet->size;
    if (!substream_index) {
      decoder->packet.strim = packet->trim_start;
      decoder->packet.etrim = packet->trim_end;
    }
  }
  return 0;
}

static int iamf_stream_decoder_update_parameter(IAMF_StreamDecoder *dec,
                                                IAMF_DataBase *db,
                                                uint64_t pid) {
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  IAMF_Stream *s = dec->stream;
  Packet *pkt = &dec->packet;

  if (pi) {
    if (pi->type == IAMF_PARAMETER_TYPE_DEMIXING &&
        pkt->pmask & IAMF_PACKET_EXTRA_DEMIX_MODE) {
      ChannelLayerContext *ctx = (ChannelLayerContext *)s->priv;
      ctx->dmx_mode = iamf_database_element_get_demix_mode(db, s->element_id);
      ia_logt("update demix mode %d", ctx->dmx_mode);
      if (ctx->dmx_mode >= 0) pkt->uflags |= IAMF_PACKET_EXTRA_DEMIX_MODE;
    } else if (pi->type == IAMF_PARAMETER_TYPE_RECON_GAIN &&
               pkt->pmask & IAMF_PACKET_EXTRA_RECON_GAIN) {
      ReconGainList *recon =
          iamf_database_element_get_recon_gain_list(db, s->element_id);
      ia_logt("update recon %p", recon);
      if (recon) {
        pkt->uflags |= IAMF_PACKET_EXTRA_RECON_GAIN;
        iamf_stream_scale_decoder_update_recon_gain(dec, recon);
      }
    } else if (pi->type == IAMF_PARAMETER_TYPE_MIX_GAIN &&
               pkt->pmask & IAMF_PACKET_EXTRA_MIX_GAIN) {
      pkt->uflags |= IAMF_PACKET_EXTRA_MIX_GAIN;
    }
  }
  return IAMF_OK;
}

static int iamf_stream_decoder_decode(IAMF_StreamDecoder *decoder, float *pcm) {
  int ret = 0;
  IAMF_Stream *stream = decoder->stream;
  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    float *buffer = decoder->buffers[2];
    ret = iamf_stream_scale_decoder_decode(decoder, buffer);
    iamf_stream_scale_decoder_demix(decoder, buffer, pcm, ret);
  } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED)
    ret = iamf_stream_ambisonics_decoder_decode(decoder, pcm);
  return ret;
}

int iamf_stream_decoder_decode_finish(IAMF_StreamDecoder *decoder) {
  for (int i = 0; i < decoder->packet.nb_sub_packets; ++i) {
    IAMF_FREEP(&decoder->packet.sub_packets[i]);
  }
  memset(decoder->packet.sub_packet_sizes, 0,
         sizeof(uint32_t) * decoder->packet.nb_sub_packets);
  decoder->packet.count = 0;
  decoder->packet.dts += decoder->frame_size;
  decoder->packet.uflags = 0;
  return 0;
}

static int iamf_stream_scale_decoder_update_recon_gain(
    IAMF_StreamDecoder *decoder, ReconGainList *list) {
  ReconGain *src;
  IAMF_ReconGain *dst;
  int ret = 0;
  int ri = 0;
  IAMF_Stream *stream = decoder->stream;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;

  if (!list) return IAMF_ERR_BAD_ARG;

  ia_logt("recon gain info : list %p, count %d, recons %p", list, list->count,
          list->recon);
  for (int i = 0; i < ctx->nb_layers; ++i) {
    src = &list->recon[ri];
    dst = ctx->conf_s[i].recon_gain;
    if (dst) {
      ++ri;
      if (i > ctx->nb_layers) {
        continue;
      }
      ia_logd("audio layer %d :", i);
      ia_logd("dst %p, src %p ", dst, src);
      if (dst->flags ^ src->flags) {
        dst->flags = src->flags;
        dst->nb_channels = src->channels;
        iamf_recon_channels_order_update(ctx->conf_s[i].layout, dst);
      }
      for (int c = 0; c < dst->nb_channels; ++c) {
        dst->recon_gain[c] = qf_to_float(src->recon_gain[c], 8);
      }
      ia_logd(" > recon gain flags 0x%04x", dst->flags);
      ia_logd(" > channel count %d", dst->nb_channels);
      for (int c = 0; c < dst->nb_channels; ++c)
        ia_logd(" > > channel %s(%d) : recon gain %f(0x%02x)",
                ia_channel_name(dst->order[c]), dst->order[c],
                dst->recon_gain[c], src->recon_gain[c]);
    }
  }
  ia_logt("recon gain info .");

  if (list->count != ri) {
    ret = IAMF_ERR_INTERNAL;
    ia_loge(
        "%s : the count (%d) of recon gain doesn't match with static meta "
        "(%d).",
        ia_error_code_string(ret), list->count, ri);
  }

  return ret;
}

static int iamf_stream_scale_decoder_decode(IAMF_StreamDecoder *decoder,
                                            float *pcm) {
  IAMF_Stream *stream = decoder->stream;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
  ScalableChannelDecoder *scale = decoder->scale;
  int ret = 0;
  float *out = pcm;
  IACoreDecoder *dec;

  ia_logt("decode sub-packets.");
  if (scale->nb_layers) {
    ia_logt("audio layer only mode.");
    uint32_t substream_offset = 0;

    for (int i = 0; i <= ctx->layer; ++i) {
      ia_logt("audio layer %d.", i);
      dec = scale->sub_decoders[i];
      ia_logd(
          "CG#%d: channels %d, streams %d, decoder %p, out %p, offset %lX, "
          "size %lu",
          i, ctx->conf_s[i].nb_channels, ctx->conf_s[i].nb_substreams, dec, out,
          (float *)out - (float *)pcm,
          sizeof(float) * decoder->frame_size * ctx->conf_s[i].nb_channels);
      for (int k = 0; k < ctx->conf_s[i].nb_substreams; ++k) {
        ia_logd(" > sub-packet %d (%p) size %d", k,
                decoder->packet.sub_packets[substream_offset + k],
                decoder->packet.sub_packet_sizes[substream_offset + k]);
      }
      ret = ia_core_decoder_decode_list(
          dec, &decoder->packet.sub_packets[substream_offset],
          &decoder->packet.sub_packet_sizes[substream_offset],
          ctx->conf_s[i].nb_substreams, out, decoder->frame_size);
      if (ret < 0) {
        ia_loge("sub packet %d decode fail.", i);
        break;
      } else if (ret != decoder->frame_size) {
        ia_loge("decoded frame size is not %d (%d).", decoder->frame_size, ret);
        break;
      }
      out += (ret * ctx->conf_s[i].nb_channels);
      substream_offset += ctx->conf_s[i].nb_substreams;
    }
  }

  return ret;
}

static int32_t iamf_stream_scale_decoder_demix(IAMF_StreamDecoder *decoder,
                                               float *src, float *dst,
                                               uint32_t frame_size) {
  IAMF_Stream *stream = decoder->stream;
  ScalableChannelDecoder *scale = decoder->scale;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;

  Demixer *demixer = scale->demixer;
  IAMF_ReconGain *re = ctx->conf_s[ctx->layer].recon_gain;

  ia_logt("demixer info update :");
  if (re) {
    demixer_set_recon_gain(demixer, re->nb_channels, re->order, re->recon_gain,
                           re->flags);

    ia_logd("channel flags 0x%04x", re->flags & U16_MASK);
    for (int c = 0; c < re->nb_channels; ++c) {
      ia_logd("channel %s(%d) recon gain %f", ia_channel_name(re->order[c]),
              re->order[c], re->recon_gain[c]);
    }
  }
  demixer_set_demixing_mode(scale->demixer, ctx->dmx_mode);
  ia_logd("demixing mode %d", ctx->dmx_mode);

  return demixer_demixing(scale->demixer, dst, src, frame_size);
}

int iamf_stream_scale_demixer_configure(IAMF_StreamDecoder *decoder) {
  IAMF_Stream *stream = decoder->stream;
  ScalableChannelDecoder *scale = decoder->scale;
  Demixer *demixer = scale->demixer;
  IAChannel chs[IA_CH_LAYOUT_MAX_CHANNELS];
  float gains[IA_CH_LAYOUT_MAX_CHANNELS];
  uint8_t flags;
  uint32_t count = 0;
  SubLayerConf *layer_conf;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;

  demixer_set_channel_layout(demixer, ctx->layout);
  demixer_set_channels_order(demixer, ctx->channels_order, ctx->channels);

  for (int l = 0; l <= ctx->layer; ++l) {
    layer_conf = &ctx->conf_s[l];
    if (layer_conf->output_gain) {
      flags = layer_conf->output_gain->flags;
      for (int c = 0; c < IA_CH_GAIN_COUNT; ++c) {
        if (flags & RSHIFT(c)) {
          chs[count] = iamf_output_gain_channel_map(layer_conf->layout, c);
          if (chs[count] != IA_CH_INVALID) {
            gains[count++] = layer_conf->output_gain->gain;
          }
        }
      }
    }
  }

  demixer_set_output_gain(demixer, chs, gains, count);

  ia_logi("demixer info :");
  ia_logi("layout %s(%d)", ia_channel_layout_name(ctx->layout), ctx->layout);
  ia_logi("input channels order :");

  for (int c = 0; c < ctx->channels; ++c) {
    ia_logi("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
            ctx->channels_order[c]);
  }

  ia_logi("output gain info : ");
  for (int c = 0; c < count; ++c) {
    ia_logi("channel %s(%d) gain %f", ia_channel_name(chs[c]), chs[c],
            gains[c]);
  }

  return 0;
}

static int iamf_stream_ambisionisc_order(int channels) {
  if (channels == 4)
    return IAMF_FOA;
  else if (channels == 9)
    return IAMF_SOA;
  else if (channels == 16)
    return IAMF_TOA;
  return 0;
}

int iamf_stream_ambisonics_decoder_decode(IAMF_StreamDecoder *decoder,
                                          float *pcm) {
  AmbisonicsDecoder *amb = decoder->ambisonics;
  int ret = 0;
  IACoreDecoder *dec;

  dec = amb->decoder;
  for (int k = 0; k < decoder->packet.nb_sub_packets; ++k) {
    ia_logd(" > sub-packet %d (%p) size %d", k, decoder->packet.sub_packets[k],
            decoder->packet.sub_packet_sizes[k]);
  }
  ret = ia_core_decoder_decode_list(
      dec, decoder->packet.sub_packets, decoder->packet.sub_packet_sizes,
      decoder->packet.nb_sub_packets, pcm, decoder->frame_size);
  if (ret < 0) {
    ia_loge("ambisonics stream packet decode fail.");
  } else if (ret != decoder->frame_size) {
    ia_loge("decoded frame size is not %d (%d).", decoder->frame_size, ret);
  }

  return ret;
}

static int iamf_stream_render(IAMF_Stream *stream, float *in, float *out,
                              int frame_size) {
  int ret = IAMF_OK;
  int inchs;
  int outchs = stream->final_layout->channels;
  float **sout = IAMF_MALLOCZ(float *, outchs);
  float **sin = 0;
  lfe_filter_t *plfe = 0;

  ia_logd("output channels %d", outchs);
  if (!sout) {
    ret = IAMF_ERR_ALLOC_FAIL;
    goto render_end;
  }

  for (int i = 0; i < outchs; ++i) {
    sout[i] = &out[frame_size * i];
  }

  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
    struct m2m_rdr_t m2m;
    IAMF_SP_LAYOUT lin;
    IAMF_PREDEFINED_SP_LAYOUT pin;

    inchs = ia_channel_layout_get_channels_count(ctx->layout);
    sin = IAMF_MALLOC(float *, inchs);

    for (int i = 0; i < inchs; ++i) {
      sin[i] = &in[frame_size * i];
    }

    lin.sp_type = 0;
    lin.sp_layout.predefined_sp = &pin;
    pin.system = iamf_layer_layout_get_rendering_id(ctx->layout);
    pin.lfe1 = iamf_layer_layout_lfe1(ctx->layout);
    pin.lfe2 = 0;

    IAMF_element_renderer_get_M2M_matrix(&lin, &stream->final_layout->sp, &m2m);
    IAMF_element_renderer_render_M2M(&m2m, sin, sout, frame_size);
  } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
    struct h2m_rdr_t h2m;
    IAMF_HOA_LAYOUT hin;

    inchs = stream->nb_channels;
    sin = IAMF_MALLOCZ(float *, inchs);

    if (!sin) {
      ret = IAMF_ERR_ALLOC_FAIL;
      goto render_end;
    }
    for (int i = 0; i < inchs; ++i) {
      sin[i] = &in[frame_size * i];
    }

    hin.order = iamf_stream_ambisionisc_order(inchs);
    ia_logd("ambisonics order is %d", hin.order);
    if (!hin.order) {
      ret = IAMF_ERR_INTERNAL;
      goto render_end;
    }

    hin.lfe_on = 1;  // turn on LFE of HOA ##SR

    IAMF_element_renderer_get_H2M_matrix(
        &hin, stream->final_layout->sp.sp_layout.predefined_sp, &h2m);
    if (hin.lfe_on && iamf_layout_lfe_check(&stream->final_layout->layout)) {
      plfe = &stream->final_layout->sp.lfe_f;
    }
    IAMF_element_renderer_render_H2M(&h2m, sin, sout, frame_size, plfe);
  }

render_end:

  if (sin) {
    free(sin);
  }
  if (sout) {
    free(sout);
  }
  return ret;
}

void iamf_mixer_reset(IAMF_Mixer *m) {
  if (m->element_ids) free(m->element_ids);
  if (m->trimming_start) free(m->trimming_start);
  if (m->trimming_end) free(m->trimming_end);
  if (m->frames) free(m->frames);

  memset(m, 0, sizeof(IAMF_Mixer));
}

static int iamf_mixer_init(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_CodecConf *cc = 0;
  IAMF_Mixer *mixer = &pst->mixer;
  int cnt = pst->nb_streams;

  if (!cnt) {
    return IAMF_ERR_INTERNAL;
  }

  memset(mixer, 0, sizeof(IAMF_Mixer));
  mixer->nb_elements = cnt;
  mixer->element_ids = IAMF_MALLOCZ(uint64_t, cnt);
  mixer->trimming_start = IAMF_MALLOCZ(int, cnt);
  mixer->trimming_end = IAMF_MALLOCZ(int, cnt);
  mixer->frames = IAMF_MALLOCZ(float *, cnt);
  if (!mixer->element_ids || !mixer->trimming_start || !mixer->trimming_end ||
      !mixer->frames) {
    iamf_mixer_reset(mixer);
    return IAMF_ERR_ALLOC_FAIL;
  }
  for (int i = 0; i < cnt; ++i) {
    mixer->element_ids[i] = pst->streams[i]->element_id;
  }
  mixer->count = 0;
  mixer->channels = iamf_layout_channels_count(&ctx->output_layout->layout);

  cc = iamf_database_element_get_codec_conf(&ctx->db,
                                            pst->streams[0]->element_id);
  if (!cc) {
    iamf_mixer_reset(mixer);
    return IAMF_ERR_INTERNAL;
  } else {
    mixer->samples = cc->nb_samples_per_frame;
  }

  ia_logd("mixer samples %d", mixer->samples);
  return 0;
}

static int iamf_mixer_add_frame(IAMF_Mixer *mixer, uint64_t element_id,
                                float *in, int samples, int trim_start,
                                int trim_end) {
  if (samples != mixer->samples) {
    return IAMF_ERR_BAD_ARG;
  }

  ia_logd("element id %lu frame, trimming start %d, end %d", element_id,
          trim_start, trim_end);
  for (int i = 0; i < mixer->nb_elements; ++i) {
    if (mixer->element_ids[i] == element_id) {
      if (!mixer->frames[i]) {
        ++mixer->count;
      }
      mixer->frames[i] = in;
      mixer->trimming_start[i] = trim_start;
      mixer->trimming_end[i] = trim_end;
      break;
    }
  }
  ia_logd("frame count %d vs element count %d", mixer->count,
          mixer->nb_elements);

  if (mixer->count == mixer->nb_elements) {
    mixer->enable_mix = 1;
  }
  return 0;
}

static int iamf_mixer_mix(IAMF_Mixer *mixer, float *out) {
  uint32_t offset, o_offset;
  int s, e, n;
  ia_logd("samples %d, channels %d", mixer->samples, mixer->channels);
  memset(out, 0, sizeof(float) * mixer->samples * mixer->channels);

  s = mixer->trimming_start[0];
  e = mixer->trimming_end[0];
  for (int i = 1; i < mixer->count; ++i) {
    if (s > mixer->trimming_start[i]) s = mixer->trimming_start[i];
    if (e > mixer->trimming_end[i]) e = mixer->trimming_end[i];
  }

  n = mixer->samples - s - e;

  ia_logd("trim start %d, end %d, output samples %d", s, e, n);
  for (int e = 0; e < mixer->nb_elements; ++e) {
    for (int c = 0; c < mixer->channels; ++c) {
      offset = (c * mixer->samples);
      o_offset = c * n;
      for (int i = 0; i < mixer->samples; ++i) {
        if (i < mixer->trimming_start[e] ||
            mixer->samples - i <= mixer->trimming_end[e])
          continue;
        out[o_offset + i - s] += mixer->frames[e][offset + i];
      }
    }
  }

  mixer->count = 0;
  memset(mixer->frames, 0, sizeof(uint8_t *) * mixer->nb_elements);
  mixer->enable_mix = 0;
  return n;
}

/* >>>>>>>>>>>>>>>>>> STREAM DECODER MIXER >>>>>>>>>>>>>>>>>> */

static void iamf_extra_data_reset(IAMF_extradata *data);

static int32_t iamf_decoder_internal_reset(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;

  iamf_database_reset(&ctx->db);
  iamf_extra_data_reset(&ctx->metadata);
  if (ctx->presentation) iamf_presentation_free(ctx->presentation);
  if (ctx->mix_presentation_label) free(ctx->mix_presentation_label);
  if (ctx->output_layout) iamf_layout_info_free(ctx->output_layout);
  audio_effect_peak_limiter_uninit(&ctx->limiter);
  memset(handle, 0, sizeof(struct IAMF_Decoder));

  return 0;
}

static int32_t iamf_decoder_internal_init(IAMF_DecoderHandle handle,
                                          const uint8_t *data, uint32_t size,
                                          uint32_t *rsize) {
  int32_t ret = 0;
  uint32_t pos = 0, consume = 0;
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_OBU obj;

  if (~ctx->flags & IAMF_FLAGS_MAGIC_CODE) {
    // search magic code obu
    ia_logi("without magic code flag.");
    while (pos < size) {
      consume = IAMF_OBU_split(data, size, &obj);
      if (!consume || obj.type == IAMF_OBU_MAGIC_CODE) {
        if (!consume) {
          ia_loge("consume size 0.");
        } else {
          ia_logt("consume size %d, obu type (%d) %s", consume, obj.type,
                  IAMF_OBU_type_string(obj.type));
          if (obj.type == IAMF_OBU_MAGIC_CODE) {
            ia_logi("type is magic code.");
          }
        }
        break;
      }
      pos += consume;
      consume = 0;
    }
  }

  if (consume || ctx->flags & IAMF_FLAGS_MAGIC_CODE) {
    pos += iamf_decoder_internal_read_descriptors_OBUs(handle, data + pos,
                                                       size - pos);
  }

  if (~ctx->flags & IAMF_FLAGS_CONFIG) {
    ret = IAMF_ERR_NEED_MORE_DATA;
  }

  *rsize = pos;
  ia_logt("read size %d pos", pos);
  return ret;
}

uint32_t iamf_decoder_internal_read_descriptors_OBUs(IAMF_DecoderHandle handle,
                                                     const uint8_t *data,
                                                     uint32_t size) {
  IAMF_OBU obu;
  uint32_t pos = 0, ret = 0, rsize = 0;

  ia_logt("handle %p, data %p, size %d", handle, data, size);
  while (pos < size) {
    ret = IAMF_OBU_split(data + pos, size - pos, &obu);
    if (!ret) {
      ia_logw("consume size is 0.");
      break;
    }
    rsize = ret;
    ia_logt("consume size %d, obu type (%d) %s", ret, obu.type,
            IAMF_OBU_type_string(obu.type));
    if (IAMF_OBU_is_descrptor_OBU(&obu)) {
      ret = iamf_decoder_internal_add_descrptor_OBU(handle, &obu);
      if (ret == IAMF_OK && obu.type == IAMF_OBU_MAGIC_CODE) {
        handle->ctx.flags |= IAMF_FLAGS_MAGIC_CODE;
      }
    } else {
      handle->ctx.flags |= IAMF_FLAGS_CONFIG;
      break;
    }
    pos += rsize;
  }
  return pos;
}

uint32_t iamf_decoder_internal_parameter_prepare(IAMF_DecoderHandle handle,
                                                 uint64_t pid) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_DataBase *db = &ctx->db;
  IAMF_StreamDecoder *dec = 0;
  IAMF_Element *e;

  e = iamf_database_get_element_by_parameterID(&ctx->db, pid);
  if (e) {
    for (int i = 0; i < pst->nb_streams; ++i) {
      if (pst->streams[i]->element_id == e->element_id) {
        dec = pst->decoders[i];
        break;
      }
    }
    if (dec) {
      iamf_stream_decoder_update_parameter(dec, db, pid);
      if (iamf_stream_decoder_check_prepared(dec)) pst->prepared_decoder = dec;
    }
  }

  return IAMF_OK;
}

uint32_t iamf_decoder_internal_parse_OBUs(IAMF_DecoderHandle handle,
                                          const uint8_t *data, uint32_t size) {
  IAMF_OBU obu;
  uint32_t pos = 0, ret = 0;

  ia_logd("handle %p, data %p, size %d", handle, data, size);
  while (pos < size) {
    ret = IAMF_OBU_split(data + pos, size - pos, &obu);
    if (!ret) {
      ia_logt("need more data.");
      break;
    }

    if (obu.type == IAMF_OBU_PARAMETER_BLOCK) {
      uint64_t pid = IAMF_OBU_get_object_id(&obu);
      ia_logd("get parameter with id %lu", pid);
      if (pid != INVALID_ID) {
        IAMF_Element *e = IAMF_ELEMENT(
            iamf_database_get_element_by_parameterID(&handle->ctx.db, pid));
        if (e) {
          IAMF_ParameterParam ext;
          IAMF_ObjectParameter *param = IAMF_OBJECT_PARAM(&ext);
          IAMF_Object *obj;

          ia_logd("the element id for parameter %lu", e->element_id);
          if (e->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
              e->channels_conf) {
            memset(&ext, 0, sizeof(IAMF_ObjectParameter));
            ext.base.type = IAMF_OBU_PARAMETER_BLOCK;
            ext.param_base = iamf_database_parameter_viewer_get_parmeter_base(
                &handle->ctx.db, pid);
            ext.nb_layers = e->channels_conf->nb_layers;
            ext.recon_gain_flags = iamf_database_element_get_recon_gain_flags(
                &handle->ctx.db, e->element_id);
          }
          obj = IAMF_object_new(&obu, param);
          iamf_database_add_object(&handle->ctx.db, obj);
          iamf_decoder_internal_parameter_prepare(handle, pid);
        }
      }
    } else if (obu.type >= IAMF_OBU_AUDIO_FRAME &&
               obu.type < IAMF_OBU_MAGIC_CODE) {
      IAMF_Object *obj = IAMF_object_new(&obu, 0);
      IAMF_Frame *o = (IAMF_Frame *)obj;
      iamf_decoder_internal_deliver(handle, o);
      IAMF_object_free(obj);
    } else if (obu.type == IAMF_OBU_MAGIC_CODE) {
      ia_logi("*********** FOUND NEW MAGIC CODE **********");
      handle->ctx.flags = IAMF_FLAGS_RECONFIG;
      break;
    } else if (obu.type == IAMF_OBU_SYNC) {
      ia_logi("*********** FOUND SYNC OBU **********");
      IAMF_Object *obj = IAMF_object_new(&obu, 0);
      IAMF_Stream *s;
      IAMF_DecoderContext *ctx = &handle->ctx;
      IAMF_Presentation *pst = ctx->presentation;
      ElementItem *ei;

      iamf_database_add_object(&handle->ctx.db, obj);

      if (ctx->global_time < ctx->db.sViewer.start_global_time) {
        ia_logi("global time changed from %lu to %lu", ctx->global_time,
                ctx->db.sViewer.start_global_time);
        ctx->global_time = ctx->db.sViewer.start_global_time;
      }
      // update timestamps of all streams
      for (int i = 0; i < pst->nb_streams; ++i) {
        s = pst->streams[i];
        s->timestamp =
            iamf_database_element_get_timestamp(&ctx->db, s->element_id);

        ei = iamf_database_element_get_item(&ctx->db, s->element_id);
        if (ei && ei->mixGain &&
            iamf_database_sync_viewer_check_id(&ctx->db, ei->mixGain->id)) {
          pst->decoders[i]->packet.pmask |= IAMF_PACKET_EXTRA_MIX_GAIN;
        } else {
          pst->decoders[i]->packet.pmask &= ~IAMF_PACKET_EXTRA_MIX_GAIN;
        }
      }
    }
    pos += ret;

    if (handle->ctx.presentation->prepared_decoder) {
      break;
    }
  }
  return pos;
}

int32_t iamf_decoder_internal_add_descrptor_OBU(IAMF_DecoderHandle handle,
                                                IAMF_OBU *obu) {
  IAMF_DataBase *db;
  IAMF_Object *obj;

  db = &handle->ctx.db;
  obj = IAMF_object_new(obu, 0);
  if (!obj) {
    ia_loge("fail to new object for %s(%d)", IAMF_OBU_type_string(obu->type),
            obu->type);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return iamf_database_add_object(db, obj);
}

int iamf_decoder_internal_deliver(IAMF_DecoderHandle handle, IAMF_Frame *obj) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Presentation *pst = ctx->presentation;
  int idx = -1, i;
  IAMF_Stream *stream;
  IAMF_StreamDecoder *decoder;

  for (i = 0; i < pst->nb_streams; ++i) {
    idx = iamf_database_element_get_substream_index(
        db, pst->streams[i]->element_id, obj->id);
    if (idx > -1) {
      break;
    }
  }

  if (idx > -1) {
    stream = pst->streams[i];
    decoder = pst->decoders[i];

    ia_logd("frame id %lu and its stream (%d) id %lu, and index %d", obj->id, i,
            pst->streams[i]->element_id, idx);
    if (idx == 0) {
      ctx->global_time = stream->timestamp;
      ia_logd("global time change to %lu", ctx->global_time);

      if (obj->trim_start != stream->trimming_start) {
        ia_logd("trimming start %lu to %lu", stream->trimming_start,
                obj->trim_start);
        stream->trimming_start = obj->trim_start;
      }

      if (obj->trim_end != stream->trimming_end) {
        ia_logd("trimming end %lu to %lu", stream->trimming_end, obj->trim_end);
        stream->trimming_end = obj->trim_end;
      }
    }
    iamf_stream_decoder_receive_packet(decoder, idx, obj);

    if (!iamf_stream_decoder_check_prepared(decoder) &&
        decoder->packet.count == decoder->packet.nb_sub_packets) {
      ElementItem *ei = iamf_database_element_get_item(db, stream->element_id);
      if (ei) {
        ParameterItem *pi = ei->demixing;
        if (pi) iamf_stream_decoder_update_parameter(decoder, db, pi->id);
        pi = ei->reconGain;
        if (pi) iamf_stream_decoder_update_parameter(decoder, db, pi->id);
        pi = ei->mixGain;
        if (pi) iamf_stream_decoder_update_parameter(decoder, db, pi->id);
      }
    }

    if (iamf_stream_decoder_check_prepared(decoder)) {
      pst->prepared_decoder = decoder;
    }

  } else {
    for (int e = 0; e < db->eViewer.count; ++e) {
      idx = iamf_database_element_get_substream_index(
          db, db->eViewer.items[i].id, obj->id);
      if (!idx) {
        iamf_database_element_time_elapse(
            db, db->eViewer.items[i].id,
            db->eViewer.items[i].codecConf->nb_samples_per_frame);
      }
    }
  }

  return 0;
}

static int iamf_target_layout_matching_calculation(TargetLayout *target,
                                                   LayoutInfo *layout) {
  SoundSystemLayout *ss;
  int s = 0;
  if (target->type == layout->layout.type) {
    if (layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
      s = 100;
    } else if (target->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
      ss = SOUND_SYSTEM_LAYOUT(target);
      if (ss->sound_system == layout->layout.sound_system.sound_system) {
        s = 100;
      }
    }
  } else {
    s = 50;
    int chs = 0;
    if (target->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
      ss = SOUND_SYSTEM_LAYOUT(target);
      chs = IAMF_layout_sound_system_channels_count(ss->sound_system);
    } else if (target->type == IAMF_LAYOUT_TYPE_BINAURAL)
      chs = IAMF_layout_binaural_channels_count();
    if (layout->channels < chs) {
      s += (chs - layout->channels);
    } else {
      s -= (layout->channels - chs);
    }
  }

  return s;
}

static float iamf_mix_presentation_get_best_loudness(IAMF_MixPresentation *obj,
                                                     LayoutInfo *layout) {
  int score = 0, s, idx = INAVLID_INDEX;
  SubMixPresentation *sub;
  float loudness = .0f, loudness_db;

  if (obj->num_sub_mixes) {
    /* for (int n = 0; n < obj->num_sub_mixes; ++n) { */
    /* sub = &obj->sub_mixes[n]; */
    sub = &obj->sub_mixes[0];  // support only 1 sub mix.

    if (sub->num_layouts) {
      for (int i = 0; i < sub->num_layouts; ++i) {
        s = iamf_target_layout_matching_calculation(sub->layouts[i], layout);
        if (s > score) {
          score = s;
          idx = i;
        }
      }
      if (idx > INAVLID_INDEX) {
        loudness_db = q_to_float(sub->loudness[idx].integrated_loudness, 8);
        // loudness_db = - 24 - loudness_db; // TV mode
        // loudness_db = - 16 - loudness_db; // Mobile mode
        loudness = db2lin(loudness_db);
        ia_logi("selected loudness is %f(%f db) <- 0x%x", loudness, loudness_db,
                sub->loudness[idx].integrated_loudness & U16_MASK);
      } else {
        loudness = 1.0f;
      }
    }
    /* } */
  }

  return loudness;
}

static int iamf_mix_presentation_matching_calculation(IAMF_MixPresentation *obj,
                                                      LayoutInfo *layout) {
  int score = 0, s;
  SubMixPresentation *sub;

  if (obj->num_sub_mixes) {
    /* for (int n = 0; n < obj->num_sub_mixes; ++n) { */
    /* sub = &obj->sub_mixes[n]; */
    sub = &obj->sub_mixes[0];  // support only 1 sub mix.

    if (sub->num_layouts) {
      for (int i = 0; i < sub->num_layouts; ++i) {
        s = iamf_target_layout_matching_calculation(sub->layouts[i], layout);
        if (s > score) score = s;
      }
    }
    /* } */
  }

  return score;
}

static IAMF_MixPresentation *iamf_decoder_get_best_mix_presentation(
    IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_MixPresentation *mp = 0, *obj;

  if (db->mixPresentation->count > 0) {
    if (db->mixPresentation->count == 1) {
      mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[0]);
    } else if (ctx->mix_presentation_label) {
      int idx = iamf_database_mix_presentation_get_label_index(
          db, ctx->mix_presentation_label);
      if (idx == INAVLID_INDEX) idx = 0;
      mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[idx]);
    } else {
      int max_percentage = 0, sub_percentage;

      for (int i = 0; i < db->mixPresentation->count; ++i) {
        obj = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
        sub_percentage =
            iamf_mix_presentation_matching_calculation(obj, ctx->output_layout);
        if (max_percentage < sub_percentage) mp = obj;
      }
    }
  }
  return mp;
}

static int iamf_decoder_enable_mix_presentation(IAMF_DecoderHandle handle,
                                                IAMF_MixPresentation *mixp) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Element *elem;
  IAMF_CodecConf *cc;
  IAMF_Presentation *old = ctx->presentation;
  IAMF_Presentation *pst;
  SubMixPresentation *sub;
  uint64_t pid;
  ParameterItem *pi = 0;
  int ret = IAMF_OK;

  pst = IAMF_MALLOCZ(IAMF_Presentation, 1);
  if (!pst) return IAMF_ERR_ALLOC_FAIL;

  pst->obj = mixp;
  ctx->presentation = pst;

  ia_logd("enable mix presentation id %lu, %p", mixp->mix_presentation_id,
          mixp);

  // There is only one sub mix in the mix presentation for simple and base
  // profiles. so the sub mix is selected the first.
  sub = mixp->sub_mixes;
  for (uint32_t i = 0; i < sub->nb_elements; ++i) {
    elem = iamf_database_get_element(db, sub->conf_s[i].element_id);
    cc = iamf_database_element_get_codec_conf(db, elem->element_id);
    pid = sub->conf_s[i].conf_m.gain.base.id;
    pi = iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
    if (!pi &&
        iamf_database_parameter_viewer_add_item(
            db, &sub->conf_s[i].conf_m.gain.base, INVALID_ID) == IAMF_OK) {
      float gain_db;
      pi = iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
      gain_db = q_to_float(sub->conf_s[i].conf_m.gain.mix_gain, 8);
      pi->mix_gain->default_mix_gain = db2lin(gain_db);
      ia_logi("element %lu : mix gain %f (%f db) <- 0x%x",
              sub->conf_s[i].element_id, pi->mix_gain->default_mix_gain,
              gain_db, sub->conf_s[i].conf_m.gain.mix_gain & U16_MASK);
    }

    iamf_database_element_set_mix_gain_parameter(
        db, elem->element_id, sub->conf_s[i].conf_m.gain.base.id);
    if (!elem || !cc) continue;
    if (elem->obj.flags & cc->obj.flags & IAMF_OBU_FLAG_REDUNDANT) {
      if (iamf_presentation_reuse_stream(pst, old, elem->element_id) !=
          IAMF_OK) {
        ret = iamf_stream_enable(handle, elem);
      }
    } else {
      ret = iamf_stream_enable(handle, elem);
    }
    if (ret != IAMF_OK) return ret;
  }

  SpeexResamplerState *resampler = 0;
  if (old) {
    resampler = iamf_presentation_take_resampler(old);
  }
  if (!resampler) {
    IAMF_Stream *stream = pst->streams[0];
    resampler =
        iamf_stream_resampler_open(stream, stream->sampling_rate,
                                   OUTPUT_SAMPLERATE, SPEEX_RESAMPLER_QUALITY);
    if (!resampler) return IAMF_ERR_INTERNAL;
  }
  pst->resampler = resampler;

  if (old) iamf_presentation_free(old);

  return IAMF_OK;
}

static int iamf_decoder_frame_gain(void *in, void *out, int channels,
                                   int frame_size, MixGain *gain) {
  float *fin = (float *)in;
  float *fout = (float *)out;
  float g;
  int count, offset;
  ia_logt("gain %p, channels %d, samples of frame %d", gain, channels,
          frame_size);

  if (!gain || !gain->mix_gain_uints) {
    count = frame_size * channels;
    if (!gain) {
      for (int i = 0; i < count; ++i) fout[i] = fin[i];
    } else {
      g = gain->default_mix_gain;
      for (int i = 0; i < count; ++i) fout[i] = fin[i] * g;
    }
  } else {
    int idx = 0;
    for (int s = 0; s < gain->nb_seg; ++s) {
      if (!gain->mix_gain_uints[s].gains) {
        count = idx + gain->mix_gain_uints[s].count;
        g = gain->mix_gain_uints[s].constant_gain;
        for (int c = 0; c < channels; ++c) {
          offset = c * frame_size;
          for (int i = idx; i < count; ++i) {
            fout[offset + i] = fin[offset + i] * g;
          }
        }
      } else {
        count = idx + gain->mix_gain_uints[s].count;
        for (int i = idx, j = 0; i < count; ++i) {
          g = gain->mix_gain_uints[s].gains[j];
          for (int c = 0; c < channels; ++c) {
            offset = c * frame_size;
            fout[offset + i] = fin[offset + i] * g;
          }
        }
      }
      idx += gain->mix_gain_uints[s].count;
    }
  }

  return IAMF_OK;
}

static int iamf_loudness_process(float *block, int frame_size, int channels,
                                 float gain) {
  int idx = 0;
  if (!block || frame_size < 0 || channels < 0) return IAMF_ERR_BAD_ARG;

  if (!frame_size || gain == 1.0f) return IAMF_OK;

  for (int c = 0; c < channels; ++c) {
    idx = c * frame_size;
    for (int i = 0; i < frame_size; ++i) {
      block[idx + i] /= gain;
    }
  }

  return IAMF_OK;
}

static int iamf_resample(SpeexResamplerState *resampler, float *in, float *out,
                         int frame_size) {
  int resample_size =
      frame_size * (resampler->out_rate / resampler->in_rate + 1);
  ia_logt("input samples %d", frame_size);
  ia_decoder_plane2stride_out_float(resampler->buffer, in, frame_size,
                                    resampler->nb_channels);
  speex_resampler_process_interleaved_float(
      resampler, (const float *)resampler->buffer, (uint32_t *)&frame_size,
      (float *)in, (uint32_t *)&resample_size);
  ia_decoder_stride2plane_out_float(out, in, resample_size,
                                    resampler->nb_channels);
  ia_logt("read samples %d, output samples %d", frame_size, resample_size);
  return resample_size;
}

static int iamf_decoder_internal_decode(IAMF_DecoderHandle handle,
                                        const uint8_t *data, int32_t size,
                                        uint32_t *rsize, void *pcm) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_StreamDecoder *decoder;
  IAMF_Stream *stream;
  IAMF_Mixer *mixer = &pst->mixer;
  SpeexResamplerState *resampler = pst->resampler;
  int ret = 0;
  uint32_t r = 0;
  int real_frame_size = 0;
  float *in, *out;
  MixGain *gain = NULL;

  ia_logt("handle %p, data %p, size %d", handle, data, size);
  r = iamf_decoder_internal_parse_OBUs(handle, data, size);

  *rsize = r;

  if (~handle->ctx.flags & IAMF_FLAGS_MAGIC_CODE) {
    return IAMF_ERR_INVALID_STATE;
  }

  if (!pst->prepared_decoder) {
    return 0;
  }

  decoder = pst->prepared_decoder;
  stream = decoder->stream;
  in = decoder->buffers[0];
  out = decoder->buffers[1];

  ia_logt("packet flag 0x%x", decoder->packet.pmask);

  ret = iamf_stream_decoder_decode(decoder, in);
  iamf_stream_decoder_decode_finish(decoder);
  pst->prepared_decoder = 0;

  real_frame_size = ret;

  if (ret < 0) {
    ia_loge("fail to decode audio packet. error no. %d", ret);
    stream->timestamp += decoder->frame_size;
    iamf_database_element_time_elapse(db, stream->element_id,
                                      decoder->frame_size);
    return ret;
  }

#if SR
  //////// SR decoding
  iamf_rec_stream_log(stream->element_id, stream->nb_channels, in,
                      real_frame_size);
  ////// SR
#endif

  iamf_stream_render(stream, in, out, real_frame_size);

#if SR
  //////// SR rendering
  iamf_ren_stream_log(stream->element_id, stream->final_layout->channels, out,
                      real_frame_size);
  ////// SR
#endif

  swap((void **)&in, (void **)&out);

  gain = iamf_database_element_get_mix_gain(db, stream->element_id,
                                            stream->timestamp, real_frame_size);
  if (gain) {
    iamf_decoder_frame_gain(in, out, stream->final_layout->channels,
                            real_frame_size, gain);
    swap((void **)&in, (void **)&out);
  }

  // metadata
  if (decoder->stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
      ctx->metadata.param) {
    IAMF_Stream *stream = decoder->stream;
    ChannelLayerContext *cctx = (ChannelLayerContext *)stream->priv;
    if (cctx->dmx_mode >= 0) ctx->metadata.param->dmixp_mode = cctx->dmx_mode;
  }

  iamf_mixer_add_frame(mixer, stream->element_id, in, real_frame_size,
                       stream->trimming_start, stream->trimming_end);

  // timestamp
  stream->timestamp += ret;
  iamf_database_element_time_elapse(db, stream->element_id, ret);

  if (!mixer->enable_mix) {
    return IAMF_OK;
  }
  real_frame_size = iamf_mixer_mix(mixer, out);
  swap((void **)&in, (void **)&out);

  if (resampler->in_rate != resampler->out_rate) {
    real_frame_size = iamf_resample(pst->resampler, in, out, real_frame_size);
    swap((void **)&in, (void **)&out);
  }

  // default mode.
  /* iamf_loudness_process(in, ctx->output_layout->channels, real_frame_size, */
  /* ctx->loudness); */

  audio_effect_peak_limiter_process_block(&ctx->limiter, in, out,
                                          real_frame_size);

  iamf_decoder_plane2stride_out_short(pcm, out, real_frame_size,
                                      ctx->output_layout->channels);
#if SR
  //////// SR mixing
  iamf_mix_stream_log(ctx->output_layout->channels, out, real_frame_size);
  ////// SR
#endif

  ctx->duration += real_frame_size;
  ctx->last_frame_size = real_frame_size;

  return real_frame_size;
}

static LayoutInfo *iamf_layout_info_new_sound_system(IAMF_SoundSystem ss) {
  IAMF_PREDEFINED_SP_LAYOUT *l;
  LayoutInfo *t = 0;

  t = IAMF_MALLOCZ(LayoutInfo, 1);
  if (!t) {
    ia_loge("fail to allocate memory to Layout.");
    return t;
  }

  t->layout.sound_system.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
  t->layout.sound_system.sound_system = ss;
  t->channels = IAMF_layout_sound_system_channels_count(ss);
  t->sp.sp_type = 0;
  l = IAMF_MALLOCZ(IAMF_PREDEFINED_SP_LAYOUT, 1);
  if (!l) {
    ia_loge("fail to allocate memory to Predefined SP Layout.");
    if (t) free(t);
    return 0;
  }
  l->system = iamf_sound_system_get_rendering_id(ss);
  l->lfe1 = iamf_sound_system_lfe1(ss);
  l->lfe2 = iamf_sound_system_lfe2(ss);

  t->sp.sp_layout.predefined_sp = l;

  return t;
}

static LayoutInfo *iamf_layout_info_new_binaural() {
  IAMF_PREDEFINED_SP_LAYOUT *l;
  LayoutInfo *t = 0;

  t = IAMF_MALLOCZ(LayoutInfo, 1);
  if (!t) {
    ia_loge("fail to allocate memory to Layout.");
    return t;
  }

  t->layout.binaural.type = IAMF_LAYOUT_TYPE_BINAURAL;
  t->channels = IAMF_layout_binaural_channels_count();
  t->sp.sp_type = 0;
  l = IAMF_MALLOCZ(IAMF_PREDEFINED_SP_LAYOUT, 1);
  if (!l) {
    ia_loge("fail to allocate memory to Predefined SP Layout.");
    if (t) free(t);
    return 0;
  }
  l->system = IAMF_BINAURAL;
  l->lfe1 = 0;
  l->lfe2 = 0;

  t->sp.sp_layout.predefined_sp = l;

  return t;
}

static void iamf_extra_data_dump(IAMF_extradata *metadata) {
  ia_logt("metadata: target layout >");

  ia_logt("metadata: sound system %u", metadata->output_sound_system);
  ia_logt("metadata: number of samples %u", metadata->number_of_samples);
  ia_logt("metadata: bitdepth %u", metadata->bitdepth);
  ia_logt("metadata: sampling rate %u", metadata->sampling_rate);
  ia_logt("metadata: sound mode %d", metadata->output_sound_mode);
  ia_logt("metadata: number loudness layout %d ",
          metadata->num_loudness_layouts);

  for (int i = 0; i < metadata->num_loudness_layouts; ++i) {
    ia_logt("metadata: loudness layout %d >", i);
    iamf_layout_dump(&metadata->loudness_layout[i]);

    ia_logt("metadata: loudness info %d >", i);
    ia_logt("\tinfo type %u", metadata->loudness[i].info_type & U8_MASK);
    ia_logt("\tintegrated loudness 0x%x",
            metadata->loudness[i].integrated_loudness & U16_MASK);
    ia_logt("\tdigital peak 0x%d",
            metadata->loudness[i].digital_peak & U16_MASK);
    if (metadata->loudness[i].info_type & 1)
      ia_logt("\ttrue peak %d", metadata->loudness[i].true_peak);
  }
  ia_logt("metadata: number parameters %d ", metadata->num_parameters);

  for (int i = 0; i < metadata->num_parameters; ++i) {
    ia_logt("parameter size %d", metadata->param[i].parameter_length);
    ia_logt("parameter type %d", metadata->param[i].parameter_definition_type);
    if (metadata->param[i].parameter_definition_type ==
        IAMF_PARAMETER_TYPE_DEMIXING)
      ia_logt("demix mode %d", metadata->param[i].dmixp_mode);
  }
}

static int iamf_extra_data_init(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_extradata *metadata = &ctx->metadata;

  ia_logt("initialize iamf extra data.");
  metadata->output_sound_system =
      iamf_layout_get_sound_system(&ctx->output_layout->layout);
  metadata->bitdepth = 16;
  metadata->sampling_rate = OUTPUT_SAMPLERATE;
  metadata->output_sound_mode = iamf_presentation_get_output_sound_mode(pst);

  ia_logt("mix presetation %p", ctx->presentation->obj);
  metadata->num_loudness_layouts =
      ctx->presentation->obj->sub_mixes->num_layouts;
  metadata->loudness_layout =
      IAMF_MALLOCZ(IAMF_Layout, metadata->num_loudness_layouts);
  metadata->loudness =
      IAMF_MALLOCZ(IAMF_LoudnessInfo, metadata->num_loudness_layouts);
  if (!metadata->loudness_layout || !metadata->loudness)
    return IAMF_ERR_ALLOC_FAIL;
  for (int i = 0; i < metadata->num_loudness_layouts; ++i) {
    iamf_layout_copy2(&metadata->loudness_layout[i],
                      ctx->presentation->obj->sub_mixes->layouts[i]);
  }
  memcpy(metadata->loudness, pst->obj->sub_mixes->loudness,
         sizeof(IAMF_LoudnessInfo) * metadata->num_loudness_layouts);

  if (pst) {
    ElementItem *ei;
    for (int i = 0; i < pst->obj->sub_mixes->nb_elements; ++i) {
      ei = iamf_database_element_get_item(
          &ctx->db, pst->obj->sub_mixes->conf_s[i].element_id);
      if (ei && ei->demixing) {
        metadata->num_parameters = 1;
        metadata->param = IAMF_MALLOCZ(IAMF_Param, 1);
        if (!metadata->param) return IAMF_ERR_ALLOC_FAIL;
        metadata->param->parameter_length = 8;
        metadata->param->parameter_definition_type =
            IAMF_PARAMETER_TYPE_DEMIXING;
        break;
      }
    }
  }

  iamf_extra_data_dump(metadata);
  return IAMF_OK;
}

static int iamf_extra_data_copy(IAMF_extradata *dst, IAMF_extradata *src) {
  if (!src) return IAMF_ERR_BAD_ARG;

  if (!dst) return IAMF_ERR_INTERNAL;

  dst->output_sound_system = src->output_sound_system;
  dst->number_of_samples = src->number_of_samples;
  dst->bitdepth = src->bitdepth;
  dst->sampling_rate = src->sampling_rate;
  dst->num_loudness_layouts = src->num_loudness_layouts;
  dst->output_sound_mode = src->output_sound_mode;

  if (dst->num_loudness_layouts) {
    dst->loudness_layout = IAMF_MALLOCZ(IAMF_Layout, dst->num_loudness_layouts);
    dst->loudness = IAMF_MALLOCZ(IAMF_LoudnessInfo, dst->num_loudness_layouts);

    if (!dst->loudness_layout || !dst->loudness) return IAMF_ERR_ALLOC_FAIL;
    for (int i = 0; i < dst->num_loudness_layouts; ++i) {
      iamf_layout_copy(&dst->loudness_layout[i], &src->loudness_layout[i]);
      memcpy(&dst->loudness[i], &src->loudness[i], sizeof(IAMF_LoudnessInfo));
    }
  } else {
    dst->loudness_layout = 0;
    dst->loudness = 0;
  }

  dst->num_parameters = src->num_parameters;

  if (dst->num_parameters) {
    dst->param = IAMF_MALLOCZ(IAMF_Param, dst->num_parameters);
    if (!dst->param) return IAMF_ERR_ALLOC_FAIL;
    for (int i = 0; i < src->num_parameters; ++i)
      memcpy(&dst->param[i], &src->param[i], sizeof(IAMF_Param));
  } else {
    dst->param = 0;
  }

  return IAMF_OK;
}

void iamf_extra_data_reset(IAMF_extradata *data) {
  if (data) {
    if (data->loudness_layout) {
      for (int i = 0; i < data->num_loudness_layouts; ++i)
        iamf_layout_reset(&data->loudness_layout[i]);

      free(data->loudness_layout);
    }

    if (data->loudness) free(data->loudness);
    if (data->param) free(data->param);

    memset(data, 0, sizeof(IAMF_extradata));
    data->output_sound_mode = IAMF_SOUND_MODE_NONE;
  }
}
/* ----------------------------- APIs ----------------------------- */

IAMF_DecoderHandle IAMF_decoder_open(void) {
  IAMF_DecoderHandle handle = 0;
  handle = IAMF_MALLOCZ(struct IAMF_Decoder, 1);
  if (handle) {
    IAMF_DataBase *db = &handle->ctx.db;

    handle->ctx.time_precision = OUTPUT_SAMPLERATE;
    handle->ctx.threshold_db = LIMITER_MaximumTruePeak;
    handle->ctx.loudness = 1.0f;
    if (iamf_database_init(db) != IAMF_OK) {
      IAMF_decoder_close(handle);
      handle = 0;
    }
  }
  return handle;
}

int32_t IAMF_decoder_close(IAMF_DecoderHandle handle) {
  if (handle) {
    iamf_decoder_internal_reset(handle);
    free(handle);
  }
#if SR
  iamf_stream_log_free();
#endif

  return 0;
}

int32_t IAMF_decoder_configure(IAMF_DecoderHandle handle, const uint8_t *data,
                               uint32_t size, uint32_t *rsize) {
  int32_t ret = 0;

  ia_logt("handle %p, data %p, size %d", handle, data, size);

  if (!handle || !data || !size) {
    return IAMF_ERR_BAD_ARG;
  }

  if (handle->ctx.flags & IAMF_FLAGS_RECONFIG) {
    IAMF_DecoderContext *ctx = &handle->ctx;

    ia_logi("reconfigure decoder.");
    iamf_database_reset(&ctx->db);
    iamf_database_init(&ctx->db);
    iamf_extra_data_reset(&ctx->metadata);
    handle->ctx.flags &= ~IAMF_FLAGS_RECONFIG;
  } else {
    ia_logd("initialize limiter.");
    audio_effect_peak_limiter_init(
        &handle->ctx.limiter, handle->ctx.threshold_db, OUTPUT_SAMPLERATE,
        iamf_layout_channels_count(&handle->ctx.output_layout->layout),
        LIMITER_AttackSec, LIMITER_ReleaseSec, LIMITER_LookAhead);
  }
  ret = iamf_decoder_internal_init(handle, data, size, rsize);

  if (ret == IAMF_OK) {
    IAMF_MixPresentation *mixp = iamf_decoder_get_best_mix_presentation(handle);
    ia_logi("valid mix presentation %ld",
            mixp ? mixp->mix_presentation_id : -1);
    if (mixp) {
      ret = iamf_decoder_enable_mix_presentation(handle, mixp);
      if (ret == IAMF_OK) {
        iamf_mixer_init(handle);
        iamf_extra_data_init(handle);
      }
      handle->ctx.loudness = iamf_mix_presentation_get_best_loudness(
          mixp, handle->ctx.output_layout);
    } else {
      handle->ctx.flags = IAMF_FLAGS_RECONFIG;
      ret = IAMF_ERR_INVALID_PACKET;
      ia_loge("Fail to find the mix presentation obu, try again.");
    }
  }

  return ret;
}

int IAMF_decoder_decode(IAMF_DecoderHandle handle, const uint8_t *data,
                        int32_t size, uint32_t *rsize, void *pcm) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  ia_logd("handle %p, data %p, size %d", handle, data, size);
  if (!(ctx->flags & IAMF_FLAGS_CONFIG)) {
    return IAMF_ERR_INVALID_STATE;
  }
  return iamf_decoder_internal_decode(handle, data, size, rsize, pcm);
}

IAMF_Labels *IAMF_decoder_get_mix_presentation_labels(
    IAMF_DecoderHandle handle) {
  IAMF_DataBase *db;
  IAMF_MixPresentation *mp;
  IAMF_Labels *labels = 0;

  if (!handle) {
    ia_loge("Invalid input argments.");
    return 0;
  }

  db = &handle->ctx.db;
  labels = IAMF_MALLOCZ(IAMF_Labels, 1);
  if (!labels) goto label_fail;

  labels->count = db->mixPresentation->count;
  labels->labels = IAMF_MALLOCZ(char *, labels->count);
  if (!labels->labels) goto label_fail;

  for (int i = 0; i < labels->count; ++i) {
    mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
    labels->labels[i] = IAMF_MALLOC(char, mp->label_size);
    if (!labels->labels[i]) goto label_fail;
    memcpy(labels->labels[i], mp->mix_presentation_friendly_label,
           mp->label_size);
  }

  return labels;

label_fail:
  if (labels) {
    if (labels->labels) {
      for (int i = 0; i < labels->count; ++i)
        if (labels->labels[i]) free(labels->labels[i]);
      free(labels->labels);
    }
    free(labels);
  }

  return 0;
}

int IAMF_decoder_output_layout_set_sound_system(IAMF_DecoderHandle handle,
                                                IAMF_SoundSystem ss) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  if (!iamf_sound_system_valid(ss)) {
    return IAMF_ERR_BAD_ARG;
  }

  ia_logd("sound system %d, channels %d", ss,
          IAMF_layout_sound_system_channels_count(ss));

  if (ctx->output_layout) iamf_layout_info_free(ctx->output_layout);
  ctx->output_layout = iamf_layout_info_new_sound_system(ss);

  return 0;
}

int IAMF_decoder_output_layout_set_binaural(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;

  ia_logd("binaural channels %d", IAMF_layout_binaural_channels_count());

  if (ctx->output_layout) iamf_layout_info_free(ctx->output_layout);
  ctx->output_layout = iamf_layout_info_new_binaural();

  return 0;
}

int IAMF_decoder_set_mix_presentation_label(IAMF_DecoderHandle handle,
                                            const char *label) {
  IAMF_DecoderContext *ctx;
  size_t slen = 0;

  if (!handle || !label) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;

  if (ctx->mix_presentation_label &&
      !strcmp(ctx->mix_presentation_label, label))
    return IAMF_OK;

  if (ctx->mix_presentation_label) free(ctx->mix_presentation_label);

  slen = strlen(label);
  ctx->mix_presentation_label = IAMF_MALLOC(char, slen + 1);
  strcpy(ctx->mix_presentation_label, label);
  ctx->mix_presentation_label[slen] = 0;

  return IAMF_OK;
}

int IAMF_layout_sound_system_channels_count(IAMF_SoundSystem ss) {
  int ret = 0;
  if (!iamf_sound_system_valid(ss)) {
    return IAMF_ERR_BAD_ARG;
  }
  ret = iamf_sound_system_channels_count_without_lfe(ss);
  ret += iamf_sound_system_lfe1(ss);
  ret += iamf_sound_system_lfe2(ss);
  ia_logd("sound system %x, channels %d", ss, ret);
  return ret;
}

int IAMF_layout_binaural_channels_count() { return 2; }

static int append_codec_string(char *buffer, int codec_index) {
  char codec[10] = "";
  switch (codec_index) {
    case 1:
      strcpy(codec, "Opus");
      break;
    case 2:
      strcpy(codec, "mp4a.40.2");
      break;
    case 3:
      strcpy(codec, "ipcm");
      break;
    case 4:
      strcpy(codec, "fLaC");
      break;
    default:
      return 0;
  }
  strcat(buffer, "iamf.");
  strcat(buffer, STR(IAMF_VERSION));
  strcat(buffer, ".");
  strcat(buffer, STR(IAMF_PROFILE));
  strcat(buffer, ".");
  strcat(buffer, codec);
  return 0;
}
char *IAMF_decoder_get_codec_capability() {
  int flag = 0, index = 0, max_len = 1024;
  int first = 1;
  char *list = IAMF_MALLOCZ(char, max_len);

#ifdef CONFIG_OPUS_CODEC
  flag |= 0x1;
#endif

#ifdef CONFIG_AAC_CODEC
  flag |= 0x2;
#endif

  // IPCM
  flag |= 0x4;

#ifdef CONFIG_FLAC_CODEC
  flag |= 0x8;
#endif

  while (flag) {
    index++;
    if (flag & 0x1) {
      if (!first) {
        strcat(list, ";");
      }
      append_codec_string(list, index);
      first = 0;
    }
    flag >>= 1;
  }

  return list;
}

int IAMF_decoder_peak_limiter_set_threshold(IAMF_DecoderHandle handle,
                                            float db) {
  if (!handle) return IAMF_ERR_BAD_ARG;
  handle->ctx.threshold_db = db;
  return IAMF_OK;
}

float IAMF_decoder_peak_limiter_get_threshold(IAMF_DecoderHandle handle) {
  if (!handle) return LIMITER_MaximumTruePeak;
  return handle->ctx.threshold_db;
}

int IAMF_decoder_set_pts(IAMF_DecoderHandle handle, uint32_t pts,
                         uint32_t time_base) {
  IAMF_DecoderContext *ctx;
  if (!handle) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  ctx->pts = pts;
  ctx->pts_time_base = time_base;
  ctx->duration = 0;
  ia_logd("set pts %u/%u", pts, time_base);

  return IAMF_OK;
}

int IAMF_decoder_get_last_metadata(IAMF_DecoderHandle handle, uint32_t *pts,
                                   IAMF_extradata *metadata) {
  IAMF_DecoderContext *ctx;
  uint64_t d;
  if (!handle || !pts || !metadata) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  d = (uint64_t)ctx->pts_time_base * (ctx->duration - ctx->last_frame_size) /
      ctx->time_precision;
  *pts = ctx->pts + (uint32_t)d;
  ia_logd("pts %u/%u, last duration %u/%u", *pts, ctx->pts_time_base,
          ctx->duration - ctx->last_frame_size, ctx->time_precision);

  iamf_extra_data_copy(metadata, &ctx->metadata);
  metadata->number_of_samples = ctx->last_frame_size;
  iamf_extra_data_dump(metadata);
  return IAMF_OK;
}
