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
 * @file IAMF_encoder.c
 * @brief The iamf encoding framework
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "IAMF_encoder.h"

#include "IAMF_encoder_private.h"
#include "ae_rdr.h"
#include "bitstreamrw.h"
#include "fixedp11_5.h"
#include "math.h"
#include "obuwrite.h"
#include "pcmconvert.h"

//#define INTER_FILE_DUMP 1
#define MAX_BITS_PER_SAMPLE 4
#define MAX_DESCRIPTOR_OBU_SIZE 4096

static int default_dmix_index = 1;
static int default_w_index = 0;
static int default_recon_gain[12] = {255, 255, 255, 255, 255, 255,
                                     255, 255, 255, 255, 255, 255};

union trans2char {
  float f;
  unsigned char c[4];
};

static void channel_based_ia_encoder_close(ChannelBasedEnc *ce);
static void scene_based_ia_encoder_close(SceneBasedEnc *se);

static int bs_setbits_leb128(bitstream_t *bs, uint32_t num) {
  unsigned char coded_data_leb[128];
  int coded_size = 0;
  if (uleb_encode(num, sizeof(num), coded_data_leb, &coded_size) != 0) {
    return 0;
  }
  for (int i = 0; i < coded_size; i++) {
    bs_setbits(bs, coded_data_leb[i], 8);  // leb128()
  }
  return coded_size;
}

static int bs_setbits_string(bitstream_t *bs, unsigned char *str) {
  for (int i = 0; i < strlen(str); i++) {
    bs_setbits(bs, str[i], 8);
  }
  bs_setbits(bs, '\0', 8);
  return (strlen(str) + 1) * 8;
}

static void mono2stereo(int16_t *in, int16_t *out, int channels,
                        int frame_size) {
  for (int i = 0; i < frame_size; i++) {
    float pcm_l = (float)(in[i]) * 0.70710678f;
    float pcm_r = (float)(in[i]) * 0.70710678f;
    out[channels * i] = (int16_t)(pcm_l);
    out[channels * i + 1] = (int16_t)(pcm_r);
  }
}

static void mono2stereo2(void *src, void *dst, int channels, int frame_size,
                         int bits_per_sample, int sample_format) {
  if (sample_format == 1) {  // little-endian
    if (bits_per_sample == 16) {
      int16_t *in = (int16_t *)src;
      int16_t *out = (int16_t *)dst;
      for (int i = 0; i < frame_size; i++) {
        float pcm_l = (float)(in[i]) * 0.70710678f;
        float pcm_r = (float)(in[i]) * 0.70710678f;
        out[channels * i] = (int16_t)(pcm_l);
        out[channels * i + 1] = (int16_t)(pcm_r);
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      unsigned char *out = (unsigned char *)dst;
      for (int i = 0; i < frame_size; i++) {
        int32_t a = in[i * 3];
        int32_t b = in[i * 3 + 1];
        int32_t c = in[i * 3 + 2];
        int32_t d = 0;
        d = d | (c << 16) | (b << 8) | a;
        if (d & 0x800000) {
          d = d | 0xFF000000;
        }
        int32_t pcm_l = (int32_t)((float)(d)*0.70710678f);
        int32_t pcm_r = pcm_l;
        out[channels * i * 3] = pcm_l & 0x000000FF;
        out[channels * i * 3 + 1] = (pcm_l >> 8) & 0x000000FF;
        out[channels * i * 3 + 2] = (pcm_l >> 16) & 0x000000FF;
        out[(channels * i + 1) * 3] = pcm_r & 0x000000FF;
        out[(channels * i + 1) * 3 + 1] = (pcm_r >> 8) & 0x000000FF;
        out[(channels * i + 1) * 3 + 2] = (pcm_r >> 16) & 0x000000FF;
      }
    } else if (bits_per_sample == 32) {
      int32_t *in = (int32_t *)src;
      int32_t *out = (int32_t *)dst;
      for (int i = 0; i < frame_size; i++) {
        float pcm_l = (float)(in[i]) * 0.70710678f;
        float pcm_r = (float)(in[i]) * 0.70710678f;
        out[channels * i] = (int32_t)(pcm_l);
        out[channels * i + 1] = (int32_t)(pcm_r);
      }
    }
  } else if (sample_format == 0) {  // big-endian
    if (bits_per_sample == 16) {
      int16_t *in = (int16_t *)src;
      int16_t *out = (int16_t *)dst;
      for (int i = 0; i < frame_size; i++) {
        // float pcm_l = (float)(BigToLittle16(in[i])) * 0.70710678f;
        // float pcm_r = (float)(BigToLittle16(in[i])) * 0.70710678f;
        // out[channels * i] = BigToLittle16((int16_t)(pcm_l));
        // out[channels * i + 1] = BigToLittle16((int16_t)(pcm_r));]]
        int16_t a = BigToLittle16((int16_t)(in[i]));
        int16_t b = a;
        float pcm_l = (float)(a)*0.70710678f;
        float pcm_r = (float)(b)*0.70710678f;
        a = (int16_t)(pcm_l);
        b = (int16_t)(pcm_r);
        out[channels * i] = BigToLittle16(a);
        out[channels * i + 1] = BigToLittle16(b);
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      unsigned char *out = (unsigned char *)dst;
      for (int i = 0; i < frame_size; i++) {
        int32_t a = in[i * 3];
        int32_t b = in[i * 3 + 1];
        int32_t c = in[i * 3 + 2];
        int32_t d = 0;
        d = d | (a << 16) | (b << 8) | c;
        if (d & 0x800000) {
          d = d | 0xFF000000;
        }
        int32_t pcm_l = (int32_t)((float)(d)*0.70710678f);
        int32_t pcm_r = pcm_l;
        out[channels * i * 3] = (pcm_l >> 16) & 0x000000FF;
        out[channels * i * 3 + 1] = (pcm_l >> 8) & 0x000000FF;
        out[channels * i * 3 + 2] = pcm_l & 0x000000FF;
        out[(channels * i + 1) * 3] = (pcm_r >> 16) & 0x000000FF;
        out[(channels * i + 1) * 3 + 1] = (pcm_r >> 8) & 0x000000FF;
        out[(channels * i + 1) * 3 + 2] = pcm_r & 0x000000FF;
      }
    } else if (bits_per_sample == 32) {
      int32_t *in = (int32_t *)src;
      int32_t *out = (int32_t *)dst;
      for (int i = 0; i < frame_size; i++) {
        float pcm_l = (float)(BigToLittle32(in[i])) * 0.70710678f;
        float pcm_r = (float)(BigToLittle32(in[i])) * 0.70710678f;
        out[channels * i] = BigToLittle32((int32_t)(pcm_l));
        out[channels * i + 1] = BigToLittle32((int32_t)(pcm_r));
      }
    }
  }
}

LoudGainMeasure *immersive_audio_encoder_loudgain_create(
    const unsigned char *channel_layout_map, int sample_rate, int frame_size) {
  LoudGainMeasure *lm = (LoudGainMeasure *)malloc(sizeof(LoudGainMeasure));
  if (!lm) return NULL;
  memset(lm, 0x00, sizeof(LoudGainMeasure));
  memcpy(lm->channel_layout_map, channel_layout_map, IA_CHANNEL_LAYOUT_COUNT);
  lm->frame_size = frame_size;
  // int channel_loudness[IA_CHANNEL_LAYOUT_COUNT] = { 1, 2, 6, 8, 10, 8, 10,
  // 12, 6, 2 };
  channelLayout channellayout[IA_CHANNEL_LAYOUT_COUNT] = {
      CHANNELMONO, CHANNELSTEREO, CHANNEL51,  CHANNEL512, CHANNEL514,
      CHANNEL71,   CHANNEL712,    CHANNEL714, CHANNEL312, CHANNELBINAURAL};
  // int channel_loudness[MAX_CHANNELS] = { 2,6,8,12, }; ///////TODO change if
  // channels are changed. channelLayout channellayout[MAX_CHANNELS] = {
  // CHANNELSTEREO ,CHANNEL312 ,CHANNEL512 ,CHANNEL714 , };///////TODO change if
  // channels are changed.
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = lm->channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    AudioLoudMeterInit(&(lm->loudmeter[layout]));
    lm->loudmeter[layout].initParams(&(lm->loudmeter[layout]), 0.4f, 0.75f,
                                     3.0f);
    lm->loudmeter[layout].prepare(&(lm->loudmeter[layout]), sample_rate,
                                  enc_get_layout_channel_count(layout),
                                  channellayout[layout]);
    lm->loudmeter[layout].startIntegrated(&(lm->loudmeter[layout]));

    lm->entire_peaksqr_gain[layout] = 0.0;
    lm->entire_truepeaksqr_gain[layout] = 0.0;
  }
  for (int i = 0; i < MAX_CHANNELS; i++) {
    audio_true_peak_meter_init(&(lm->peakmeter[i]));
    audio_true_peak_meter_reset_states(&(lm->peakmeter[i]));
  }
  lm->msize25pct = sample_rate / 10 / frame_size;

  lm->measure_end = 0;
  return lm;
}

int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm,
                                             float *inbuffer,
                                             int channel_layout) {
  int ret = 0;
  resultData_s result;
  result = lm->loudmeter[channel_layout].processFrameLoudness(
      &(lm->loudmeter[channel_layout]), inbuffer, lm->msize25pct,
      lm->frame_size);
  return 0;
}

int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float *inbuffer,
                                         int channel_layout, int begin_ch,
                                         int nch) {
  int ret = 0;
  float dsig, tsig;
  for (int fr = 0; fr < lm->frame_size; fr++) {
    for (int ch = begin_ch; ch < begin_ch + nch; ch++) {
      dsig = inbuffer[ch * lm->frame_size + fr];
      if (lm->entire_peaksqr_gain[channel_layout] < dsig * dsig)  ///////TODO
      {
        lm->entire_peaksqr_gain[channel_layout] = dsig * dsig;
      }

      tsig = audio_true_peak_meter_next_true_peak(&(lm->peakmeter[ch]), dsig);
      if (lm->entire_truepeaksqr_gain[channel_layout] < tsig * tsig) {
        lm->entire_truepeaksqr_gain[channel_layout] = tsig * tsig;
      }
    }
  }
  lm->gaindown_flag[channel_layout] = 1;
  return 0;
}

int immersive_audio_encoder_gain_measure2(LoudGainMeasure *lm, float *inbuffer,
                                          int channel_layout, int ch,
                                          int index) {
  int ret = 0;
  float dsig, tsig;
  for (int fr = 0; fr < lm->frame_size; fr++) {
    dsig = inbuffer[ch * lm->frame_size + fr];
    if (lm->entire_peaksqr_gain[channel_layout] < dsig * dsig)  ///////TODO
    {
      lm->entire_peaksqr_gain[channel_layout] = dsig * dsig;
    }

    tsig = audio_true_peak_meter_next_true_peak(&(lm->peakmeter[index]), dsig);
    if (lm->entire_truepeaksqr_gain[channel_layout] < tsig * tsig) {
      lm->entire_truepeaksqr_gain[channel_layout] = tsig * tsig;
    }
  }
  lm->gaindown_flag[channel_layout] = 1;
  return 0;
}

int immersive_audio_encoder_loudgain_destory(LoudGainMeasure *lm) {
  if (!lm) return -1;
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = lm->channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    AudioLoudMeterDeinit(&(lm->loudmeter[layout]));
  }
  for (int i = 0; i < MAX_CHANNELS; i++) {
    audio_true_peak_meter_deinit(&(lm->peakmeter[i]));
  }

  free(lm);
  return 0;
}

int write_recon_gain(ChannelBasedEnc *ce, unsigned char *buffer,
                     int type)  // 0 common, 1 base, 2 advance, ret write size
{
#undef MHDR_LEN
#define MHDR_LEN 255
  unsigned char bitstr[MHDR_LEN] = {
      0,
  };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));
  unsigned char coded_data_leb[10];
  int coded_size = 0;

  int layout = ce->channel_layout_map[type];
  if (ce->recon_gain_flag) {
    uint16_t recon_gain_flag = 0;
    int max_recon_gain_fields = 12;
#if 0
    int max_recon_gain_fields_msb = 14;
    for (int i = 0; i < max_recon_gain_fields_msb; i++)
    {
      // scalable_map is based on wav channel order.
      // get_recon_gain_flags_map convert wav channel order to vorbis channel order.
      /*
      b1(LSB) b2      b3      b4      b5      b6      b7      b8      b9      b10      b11      b12(MSB)
      L	      C       R	      Ls(Lss) Rs(Rss)	Ltf	    Rtf     Lb(Lrs)	Rb(Rrs)	Ltb(Ltr) Rtb(Rtr) LFE
      */
      int channel = get_recon_gain_flags_map_msb[layout][i];
      if (channel >= 0 && ce->upmixer->scalable_map[layout][channel] == 1)
      {
        recon_gain_flag = recon_gain_flag | (0x01 << i);
      }
    }
#else
    for (int i = 0; i < max_recon_gain_fields; i++) {
      // scalable_map is based on wav channel order.
      // get_recon_gain_flags_map convert wav channel order to vorbis channel
      // order.
      /*
      b1(LSB) b2      b3      b4      b5      b6      b7      b8      b9 b10 b11
      b12(MSB)
      L	      C       R	      Ls(Lss) Rs(Rss)	Ltf	    Rtf     Lb(Lrs)
      Rb(Rrs)	Ltb(Ltr) Rtb(Rtr) LFE
      */
      int channel = get_recon_gain_flags_map[layout][i];
      if (channel >= 0 && ce->upmixer->scalable_map[layout][channel] == 1) {
        recon_gain_flag = recon_gain_flag | (0x01 << i);
      }
    }
#endif
    if (uleb_encode(recon_gain_flag, sizeof(recon_gain_flag), coded_data_leb,
                    &coded_size) != 0) {  // Channel_Group_Size (leb128())
      return 0;
    }
    for (int i = 0; i < coded_size; i++) {
      bs_setbits(&bs, coded_data_leb[i], 8);
    }
    for (int i = 0; i < max_recon_gain_fields; i++) {
      // channel range is 0 ~ nch for ce->mdhr.scalablefactor[layout],
      int channel_index = get_recon_gain_value_map[layout][i];
      int channel = get_recon_gain_flags_map[layout][i];

      if (channel_index >= 0 && channel > 0 &&
          ce->upmixer->scalable_map[layout][channel] == 1) {
        bs_setbits(&bs, ce->mdhr.scalablefactor[layout][channel_index], 8);
      }
    }
  }
  memcpy(buffer, bitstr, bs.m_posBase);
  return bs.m_posBase;
}

int immersive_audio_encoder_ctl_va_list(IAMF_Encoder *ie, int element_id,
                                        int request, va_list ap) {
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }
  ChannelBasedEnc *ce = &(ae->channel_based_enc);

  int ret = IAMF_OK;
  switch (request) {
    case IA_SET_RECON_GAIN_FLAG_REQUEST: {
      uint32_t recon_gain_flag;
      recon_gain_flag = va_arg(ap, uint32_t);
      if (recon_gain_flag < 0) {
        goto bad_arg;
      }
      ce->recon_gain_flag = recon_gain_flag;
      ce->upmixer->recon_gain_flag = recon_gain_flag;
    } break;
    case IA_SET_SCALE_FACTOR_MODE_REQUEST: {
      uint32_t scalefactor_mode;
      scalefactor_mode = va_arg(ap, uint32_t);
      if (scalefactor_mode < 0) {
        goto bad_arg;
      }
      ce->scalefactor_mode = scalefactor_mode;
    } break;
    case IA_SET_STANDALONE_REPRESENTATION_REQUEST: {
      uint32_t is_standalone;
      is_standalone = va_arg(ap, uint32_t);
      if (is_standalone < 0) {
        goto bad_arg;
      }
      ie->is_standalone = is_standalone;
      ia_logw("is_standalone: %d\n", ie->is_standalone);
    } break;
    case IA_SET_OUTPUT_GAIN_FLAG_REQUEST: {
      uint32_t output_gain_flag;
      output_gain_flag = va_arg(ap, uint32_t);
      if (output_gain_flag < 0) {
        goto bad_arg;
      }
      ce->output_gain_flag = output_gain_flag;
    } break;
    case IA_SET_BITRATE_REQUEST: {
      ae->encode_ctl(ae, request, ap);
      ae->encode_ctl2(ae, request, ap);
    } break;
    case IA_SET_BANDWIDTH_REQUEST: {
      ae->encode_ctl(ae, request, ap);
      ae->encode_ctl2(ae, request, ap);
    } break;
    case IA_SET_VBR_REQUEST: {
      ae->encode_ctl(ae, request, ap);
      ae->encode_ctl2(ae, request, ap);
    } break;
    case IA_SET_COMPLEXITY_REQUEST: {
      ae->encode_ctl(ae, request, ap);
      ae->encode_ctl2(ae, request, ap);
    } break;
    case IA_GET_LOOKAHEAD_REQUEST: {
      ae->encode_ctl(ae, request, ap);
    } break;
    default:
      ret = IAMF_ERR_UNIMPLEMENTED;
      break;
  }
  return ret;
bad_arg:
  return IAMF_ERR_BAD_ARG;
}

int IAMF_encoder_ctl(IAMF_Encoder *et, int element_id, int request, ...) {
  int ret;
  va_list ap;
  va_start(ap, request);
  ret = immersive_audio_encoder_ctl_va_list(et, element_id, request, ap);
  va_end(ap);
  return ret;
}

static int get_scalable_format(AudioElementEncoder *ae,
                               IAChannelLayoutType channel_layout_in,
                               const IAChannelLayoutType *channel_layout_cb) {
  ae->channels = enc_get_layout_channel_count(channel_layout_in);
  unsigned char channel_layout_map[IA_CHANNEL_LAYOUT_COUNT] = {
      IA_CHANNEL_LAYOUT_COUNT,
  };
  int channel_groups = 0;
  for (channel_groups = 0; channel_groups < IA_CHANNEL_LAYOUT_COUNT;
       channel_groups++) {
    channel_layout_map[channel_groups] = channel_layout_cb[channel_groups];
    if (channel_layout_cb[channel_groups] == IA_CHANNEL_LAYOUT_COUNT) break;
  }
  channel_layout_map[channel_groups] = channel_layout_in;
  channel_groups++;
  channel_layout_map[channel_groups] = IA_CHANNEL_LAYOUT_COUNT;

  int last_s_channels = 0, last_h_channels = 0;

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    if (get_surround_channels(layout) < last_s_channels ||
        get_height_channels(layout) < last_h_channels ||
        (get_surround_channels(layout) == last_s_channels &&
         get_height_channels(layout) == last_h_channels)) {
      ia_loge("The combination is illegal!!!, please confirm the rules:\n");
      ia_loge(
          "Adjacent channel layouts of a scalable format(where CLn1 is the "
          "precedent channel layout and CLn is the next one)\n");
      ia_loge("are only allowed as below, where CLn = S(n).W(n).H(n)\n");
      ia_loge(
          ">>>> S(n-1) <= S(n) and W(n 1) <= W(n) and H(n 1) <= H(n) "
          "except: S(n-1) = S(n) and W(n-1) = W(n) and H(n-1) = H(n) \n");
      ia_loge(
          " NOTE: S(Surround Channel), W(Subwoofer Channel), "
          "H(Height Channel)\n");
      return 0;
    }
    last_s_channels = get_surround_channels(layout);
    last_h_channels = get_height_channels(layout);
  }

#if 1
  int idx = 0, ret = 0;
  int last_cl_layout = CHANNEL_LAYOUT_INVALID;
  uint8_t new_channels[256];
  fprintf(stderr, "\nTransmission Channels Order: \n");
  fprintf(stderr, "---\n");
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    ret = enc_get_new_channels2(last_cl_layout, layout, new_channels);
    int channel_c = enc_has_c_channel(ret, new_channels);
    ////print new channels
    if (ret > 0) {
      for (int ch = 0; ch < ret; ++ch) {
        fprintf(stderr, "%s\n", enc_get_channel_name(new_channels[ch]));
      }
    }
    /////////////////////
    for (int j = 0; j < ret; j++) {
      ae->ia_core_encoder[i].enc_stream_map[j] = j;
      ae->ia_core_decoder[i].dec_stream_map[j] = j;
    }
    if (channel_c >= 0) {
      if (last_cl_layout == IA_CHANNEL_LAYOUT_MONO) {
        ae->ia_core_encoder[i].stream_count = (ret - 2) / 2 + 2 + 1;
      } else
        ae->ia_core_encoder[i].stream_count = (ret - 2) / 2 + 2;
      ae->ia_core_encoder[i].coupled_stream_count = (ret - 2) / 2;
      ae->ia_core_encoder[i].channel = ret;

      ae->ia_core_decoder[i].stream_count = ae->ia_core_encoder[i].stream_count;
      ae->ia_core_decoder[i].coupled_stream_count =
          ae->ia_core_encoder[i].coupled_stream_count;
      ae->ia_core_decoder[i].channel = ae->ia_core_encoder[i].channel;
    } else {
      if (ret == 1) {
        ae->ia_core_encoder[i].stream_count = 1;
        ae->ia_core_encoder[i].coupled_stream_count = 0;
        ae->ia_core_encoder[i].channel = 1;
      } else {
        ae->ia_core_encoder[i].stream_count = ret / 2;
        ae->ia_core_encoder[i].coupled_stream_count = ret / 2;
        ae->ia_core_encoder[i].channel = ret;
      }

      ae->ia_core_decoder[i].stream_count = ae->ia_core_encoder[i].stream_count;
      ae->ia_core_decoder[i].coupled_stream_count =
          ae->ia_core_encoder[i].coupled_stream_count;
      ae->ia_core_decoder[i].channel = ae->ia_core_encoder[i].channel;
    }
    fprintf(stderr, "---\n");
    idx += ret;

    last_cl_layout = layout;
  }
#endif
  if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    ChannelBasedEnc *sbe = &(ae->channel_based_enc);
    memcpy(sbe->channel_layout_map, channel_layout_map,
           IA_CHANNEL_LAYOUT_COUNT);
  }

  return channel_groups;
}

//
int IAMF_encoder_dmpd_start(IAMF_Encoder *ie, int element_id) {
  fprintf(stderr, "\nDownMix Parameter Determination start...\n");

  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }
  ChannelBasedEnc *ce = &(ae->channel_based_enc);

  int channel_layout_in = 0;
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    channel_layout_in = lay_out;
  }
  ce->asc = ia_asc_start(channel_layout_in, ie->frame_size,
                         &(ce->queue_dm[QUEUE_DMPD]));
  ce->heq = ia_heq_start(channel_layout_in, ie->input_sample_rate,
                         &(ce->queue_wg[QUEUE_DMPD]));
  return 0;
}

int IAMF_encoder_dmpd_process(IAMF_Encoder *ie, int element_id,
                              const int16_t *pcm, int frame_size) {
  if (!(ie->sample_format == 1 && ie->bits_per_sample == 16)) return 0;
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }
  ChannelBasedEnc *ce = &(ae->channel_based_enc);
  ia_asc_process(ce->asc, pcm, frame_size);
  ia_heq_process(ce->heq, pcm, frame_size);
  return 0;
}

int IAMF_encoder_dmpd_stop(IAMF_Encoder *ie, int element_id) {
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }
  ChannelBasedEnc *ce = &(ae->channel_based_enc);
  ia_asc_stop(ce->asc);
  ia_heq_stop(ce->heq);
  ce->asc = NULL;
  ce->heq = NULL;
  fprintf(stderr, "DownMix Parameter Determination stop!!!\n\n");
  return 0;
}
//

extern encode_creator_t dep_encoders[];
extern encode_creator_t dep_encoders2[];
extern decode_creator_t dep_decoders[];

static const char *dep_codec_name[] = {"unknow", "opus", "aac", "flac", "pcm"};

static int iamf_require_recon_gain(int codec_id) {
  if (codec_id == IAMF_CODEC_OPUS || codec_id == IAMF_CODEC_AAC)
    return 1;
  else if (codec_id == IAMF_CODEC_FLAC || codec_id == IAMF_CODEC_PCM)
    return 0;
  else
    return 1;
}

static void channel_based_ia_encoder_open(AudioElementEncoder *ae) {
  ChannelBasedEnc *ce = &(ae->channel_based_enc);
  ce->input_sample_rate = ae->input_sample_rate;
  ce->frame_size = ae->frame_size;

  ce->recon_gain_flag = 0;
  ce->scalefactor_mode = 2;

  ce->downmixer_ld = downmix_create(ce->channel_layout_map, ae->frame_size);
  ce->downmixer_rg = downmix_create(ce->channel_layout_map, ae->frame_size);
  ce->downmixer_enc = downmix_create(ce->channel_layout_map, ae->frame_size);
  ce->loudgain = immersive_audio_encoder_loudgain_create(
      ce->channel_layout_map, ae->input_sample_rate, ae->frame_size);
  ce->mdhr.dialog_onoff = 1;
  ce->mdhr.dialog_level = 0;
  ce->mdhr.drc_profile = 0;
  ce->mdhr.len_of_4chauxstrm = 0;
  ce->mdhr.lfe_onoff = 1;
  ce->mdhr.lfe_gain = 0;
  ce->mdhr.len_of_6chauxstrm = 0;
  ce->mdhr.dmix_matrix_type = 1;
  ce->mdhr.weight_type = 1;
  ce->mdhr.major_version = 1;
  ce->mdhr.minor_version = 1;
  ce->mdhr.coding_type = 0;
  ce->mdhr.nsamples_of_frame = 0;

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    ce->mdhr.LKFSch[i] = 1;
    ce->mdhr.dmixgain_f[i] = 1.0;
    ce->mdhr.dmixgain_db[i] = 0;
    ce->mdhr.chsilence[i] = 0xFFFFFFFF;
    for (int j = 0; j < 12; j++) ce->mdhr.scalablefactor[i][j] = 0xFF;
  }

  ce->upmixer =
      upmix_create(0, ce->channel_layout_map, ae->frame_size, ae->preskip_size);
  ce->upmixer->mdhr_l = ce->mdhr;
  ce->upmixer->mdhr_c = ce->mdhr;

  scalablefactor_init();
  ce->sf = scalablefactor_create(ce->channel_layout_map, ae->frame_size);

  memset(&(ce->fc), 0x00, sizeof(ce->fc));

  for (int i = 0; i < QUEUE_STEP_MAX; i++) {
    QueueInit(&(ce->queue_dm[i]), kUInt8, 1, 1);
    QueueInit(&(ce->queue_wg[i]), kUInt8, 1, 1);
  }

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    QueueInit(&(ce->queue_r[lay_out]), kFloat, ae->frame_size,
              enc_get_layout_channel_count(lay_out));
    QueueInit(&(ce->queue_m[lay_out]), kFloat, ae->frame_size,
              enc_get_layout_channel_count(lay_out));
    QueueInit(&(ce->queue_d[lay_out]), kInt16, ae->frame_size,
              enc_get_layout_channel_count(lay_out));
    QueueInit(&(ce->queue_rg[lay_out]), kUInt8,
              enc_get_layout_channel_count(lay_out), 1);
  }
  if (ae->codec_id == IAMF_CODEC_OPUS) {
    ce->the_preskip_frame = 1;
  } else if (ae->codec_id == IAMF_CODEC_AAC) {
    ce->the_preskip_frame = 3;
  }

  if (ae->channel_groups == 1) {
    int layout = ce->channel_layout_map[0];
    uint8_t *tchs = NULL;
    int nch = enc_get_layout_channel_count(layout);
    tchs = enc_get_layout_channels(layout);
    for (int i = 0; i < nch; i++) {
      for (int j = 0; j < nch; j++) {
        if (ce->downmixer_ld->channel_order[i] == tchs[j]) {
          ae->ia_core_encoder[0].enc_stream_map[i] = j;
          break;
        }
      }
    }
  } else {
    if (iamf_require_recon_gain(ae->codec_id)) {
      ce->recon_gain_flag = 1;
      ce->upmixer->recon_gain_flag = 1;
      ae->encode_init2(ae);
      ae->decode_init(ae);
    }
    ce->output_gain_flag = 1;
  }

#ifdef INTER_FILE_DUMP
  ia_intermediate_file_writeopen(ce, FILE_DOWNMIX_M, "ALL");
  ia_intermediate_file_writeopen(ce, FILE_DOWNMIX_S, "ALL");
  ia_intermediate_file_writeopen(ce, FILE_GAIN_DOWN, "ALL");
  ia_intermediate_file_writeopen(ce, FILE_UPMIX, "ALL");
  ia_intermediate_file_writeopen(ce, FILE_DECODED, "ALL");
#endif
}

static void scene_based_ia_encoder_open(AudioElementEncoder *ae) { return; }

IAMF_Encoder *IAMF_encoder_create(int32_t Fs, int bits_per_sample,
                                  int sample_format,
                                  int codec_id,  // 1:opus, 2:aac
                                  int frame_size, int *error) {
  IAMF_Encoder *ie = (IAMF_Encoder *)malloc(sizeof(IAMF_Encoder));
  if (!ie) return NULL;
  memset(ie, 0x00, sizeof(IAMF_Encoder));

  // AudioFrameSize audio_frame_size[IAMF_CODEC_COUNT] = {
  // AUDIO_FRAME_SIZE_INVALID ,AUDIO_FRAME_SIZE_OPUS, AUDIO_FRAME_SIZE_AAC,
  // AUDIO_FRAME_SIZE_PCM };
  AudioPreskipSize audio_preskip_size[IAMF_CODEC_COUNT] = {
      AUDIO_PRESKIP_SIZE_INVALID, AUDIO_PRESKIP_SIZE_OPUS,
      AUDIO_PRESKIP_SIZE_AAC, AUDIO_PRESKIP_SIZE_FLAC, AUDIO_PRESKIP_SIZE_PCM};

  ie->input_sample_rate = Fs;
  ie->bits_per_sample = bits_per_sample;
  ie->sample_format = sample_format;
  ie->codec_id = codec_id;
  ie->frame_size = frame_size;
  if (codec_id == IAMF_CODEC_OPUS) {
    if (Fs <= 48000)
      ie->preskip_size = audio_preskip_size[codec_id] / (48000 / Fs);
  } else
    ie->preskip_size = audio_preskip_size[codec_id];

  ie->root_node = insert_obu_root_node();
  ie->descriptor_config.codec_config.codec_config_id =
      insert_obu_node(ie->root_node, OBU_IA_Codec_Config, -1);
  ie->is_descriptor_changed = 1;
  return ie;
}

static void write_opus_specific_info(IAMF_Encoder *ie) {
  bitstream_t bs;
  bs_init(&bs, ie->descriptor_config.codec_config.decoder_config,
          sizeof(ie->descriptor_config.codec_config.decoder_config));

  bs_setbits(&bs, 0, 8);                       // version
  bs_setbits(&bs, 2, 8);                       // OutputChannelCount
  bs_setbits(&bs, ie->preskip_size, 16);       // PreSkip
  bs_setbits(&bs, ie->input_sample_rate, 32);  // InputSampleRate
  bs_setbits(&bs, 0, 16);                      // OutputGain
  bs_setbits(&bs, 0, 8);                       // ChannelMappingFamily

  ie->descriptor_config.codec_config.size_of_decoder_config = bs.m_posBase;
}

static int samplingFrequencyIndex[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,  7350,  0,     0,     0};
#if 0
static void write_aac_specific_info(IAMF_Encoder *ie) {
  bitstream_t bs;
  bs_init(&bs, ie->descriptor_config.codec_config.decoder_config,
          sizeof(ie->descriptor_config.codec_config.decoder_config));

  struct {
    int es;
    int dc;   // DecoderConfig
    int dsi;  // DecSpecificInfo
    int sl;   // SLConfig
  } dsize;

  enum { TAG_ES = 3, TAG_DC = 4, TAG_DSI = 5, TAG_SLC = 6 };

  // calc sizes
#define DESCSIZE(x) (x + 5 /*.tag+.size*/)
  dsize.sl = 1;
  dsize.dsi = 2;  // extra data size
  dsize.dc = 13 + DESCSIZE(dsize.dsi);
  dsize.es = 3 + DESCSIZE(dsize.dc) + DESCSIZE(dsize.sl);

  // output esds atom data
  // version/flags ?
  bs_setbits(&bs, 0, 32);
  // mp4es
  bs_setbits(&bs, TAG_ES, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, dsize.es, 8);
  // ESID
  bs_setbits(&bs, 0, 16);
  // flags(url(bit 6); ocr(5); streamPriority (0-4)):
  bs_setbits(&bs, 0, 8);

  bs_setbits(&bs, TAG_DC, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, dsize.dc, 8);
  bs_setbits(&bs, 0x40 /*MPEG-4 audio */, 8);
  bs_setbits(&bs, (5 << 2) /* AudioStream */ | 1 /* reserved = 1 */, 8);
  // decode buffer size bytes
#if 0

#else
  bs_setbits(&bs, 0, 8);
  bs_setbits(&bs, 0x18, 8);
  bs_setbits(&bs, 0, 8);
#endif
  // bitrate
  bs_setbits(&bs, 0, 32);
  bs_setbits(&bs, 0, 32);

  bs_setbits(&bs, TAG_DSI, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, dsize.dsi, 8);

  // AudioSpecificConfig
  /*
  object type; f(5)
  frequency index; f(4)
  channel configuration; f(4)
  GASpecificConfig{
  frameLengthFlag; f(1)
  dependsOnCoreCoder; f(1)
  extensionFlag; f(1)
  }
  */
  bs_setbits(&bs, 2, 5);  // object_type
  bs_setbits(&bs, 3, 4);  // sampling_index
  bs_setbits(&bs, 2, 4);  // chan_config
  bs_setbits(&bs, 0, 1);
  bs_setbits(&bs, 0, 1);
  bs_setbits(&bs, 0, 1);

  bs_setbits(&bs, TAG_SLC, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, 0x80, 8);
  bs_setbits(&bs, dsize.sl, 8);
  // "predefined" (no idea)
  bs_setbits(&bs, 2, 8);

  ie->descriptor_config.codec_config.size_of_decoder_config = bs.m_posBase;
}
#else
static void write_aac_specific_info(IAMF_Encoder *ie) {
  bitstream_t bs;
  bs_init(&bs, ie->descriptor_config.codec_config.decoder_config,
          sizeof(ie->descriptor_config.codec_config.decoder_config));

  struct {
    int es;
    int dc;   // DecoderConfig
    int dsi;  // DecSpecificInfo
    int sl;   // SLConfig
  } dsize;

  enum { TAG_ES = 3, TAG_DC = 4, TAG_DSI = 5, TAG_SLC = 6 };

  // calc sizes
#define DESCSIZE(x) (x + 5 /*.tag+.size*/)
  dsize.sl = 1;
  dsize.dsi = 2;  // extra data size
  dsize.dc = 13 + DESCSIZE(dsize.dsi);
  dsize.es = 3 + DESCSIZE(dsize.dc) + DESCSIZE(dsize.sl);

  bs_setbits(&bs, TAG_DC, 8);
  bs_setbits(&bs, 0x40 /*MPEG-4 audio */, 8);
  bs_setbits(&bs, (5 << 2) /* AudioStream */ | 1 /* reserved = 1 */, 8);
  // decode buffer size bytes
#if 0

#else
  bs_setbits(&bs, 0, 8);
  bs_setbits(&bs, 0x18, 8);
  bs_setbits(&bs, 0, 8);
#endif
  // bitrate
  bs_setbits(&bs, 0, 32);
  bs_setbits(&bs, 0, 32);

  bs_setbits(&bs, TAG_DSI, 8);
  // AudioSpecificConfig
  /*
  object type; f(5)
  frequency index; f(4)
  channel configuration; f(4)
  GASpecificConfig{
  frameLengthFlag; f(1)
  dependsOnCoreCoder; f(1)
  extensionFlag; f(1)
  }
  */
  int sampling_index = 0;
  for (; sampling_index < 16; sampling_index++) {
    if (samplingFrequencyIndex[sampling_index] == ie->input_sample_rate) break;
  }
  bs_setbits(&bs, 2, 5);               // object_type
  bs_setbits(&bs, sampling_index, 4);  // sampling_index
  bs_setbits(&bs, 2, 4);               // chan_config
  bs_setbits(&bs, 0, 1);
  bs_setbits(&bs, 0, 1);
  bs_setbits(&bs, 0, 1);

  ie->descriptor_config.codec_config.size_of_decoder_config = bs.m_posBase;
}
#endif

#define FLAC_STREAMINFO_SIZE 34
#define FLAC_MAX_CHANNELS 8
#define FLAC_MIN_BLOCKSIZE 16
#define FLAC_MAX_BLOCKSIZE 65535
#define FLAC_MIN_FRAME_SIZE 11
static void write_flac_specific_info(IAMF_Encoder *ie) {
  bitstream_t bs;
  bs_init(&bs, ie->descriptor_config.codec_config.decoder_config,
          sizeof(ie->descriptor_config.codec_config.decoder_config));
  unsigned md5sum[16] = {
      0,
  };
  memset(md5sum, 0x00, 16);
  bs_setbits(&bs, 1 << 7 | FLAC_METADATA_TYPE_STREAMINFO, 8);
  bs_setbits(&bs, FLAC_STREAMINFO_SIZE, 24);
  bs_setbits(&bs, ie->frame_size, 16);
  bs_setbits(&bs, ie->frame_size, 16);
  bs_setbits(&bs, 0, 24);
  bs_setbits(&bs, 0, 24);
  bs_setbits(&bs, ie->input_sample_rate, 20);
  bs_setbits(&bs, (2 - 1), 3);
  bs_setbits(&bs, ie->bits_per_sample - 1, 5);
  bs_setbits(&bs, (0 & 0xFFFFFF000LL) >> 12, 24);
  bs_setbits(&bs, (0 & 0x000000FFFLL), 12);
  for (int i = 0; i < 16; i++) {
    bs_setbits(&bs, md5sum[i], 8);
  }
  ie->descriptor_config.codec_config.size_of_decoder_config = bs.m_posBase;
}

static void write_lpcm_specific_info(IAMF_Encoder *ie) {
#if 0
  /*
  class decoder_config(lpcm) {
  unsigned int (32) sample_rate;
  unsigned int (8) sample_size;
  }
  */
  bitstream_t bs;
  bs_init(&bs, ie->descriptor_config.codec_config.decoder_config,
          sizeof(ie->descriptor_config.codec_config.decoder_config));
  bs_setbits(&bs, ie->input_sample_rate, 32);  // sample_rate
  bs_setbits(&bs, 16, 8);                      // sample_size
#else
  /*
  class decoder_config(ipcm) {
  unsigned int (8) sample_format_flags;
  unsigned int (8) sample_size;
  unsigned int (32) sample_rate;
  }
  */
  bitstream_t bs;
  bs_init(&bs, ie->descriptor_config.codec_config.decoder_config,
          sizeof(ie->descriptor_config.codec_config.decoder_config));
  bs_setbits(&bs, ie->sample_format, 8);       // sample_format_flags
  bs_setbits(&bs, ie->bits_per_sample, 8);     // sample_size
  bs_setbits(&bs, ie->input_sample_rate, 32);  // sample_rate

#endif
  ie->descriptor_config.codec_config.size_of_decoder_config = bs.m_posBase;
}

static void update_ia_descriptor(IAMF_Encoder *ie) {
  if (!ie->is_descriptor_changed) return;
  int element_index = 0;
  // 1. One Start Code OBU
  if (!ie->audio_element_enc->next) {  // simple profile

    if (ie->descriptor_config.magic_code.profile_version == 0 &&
        ie->descriptor_config.magic_code.ia_code == MKTAG('i', 'a', 'm', 'f')) {
      ie->descriptor_config.magic_code.obu_redundant_copy = 1;
    }
    ie->descriptor_config.magic_code.ia_code = MKTAG('i', 'a', 'm', 'f');
    ie->descriptor_config.magic_code.version = 0;

    ie->descriptor_config.magic_code.profile_version = 0;
  } else {  // base profile

    if (ie->descriptor_config.magic_code.profile_version == 16 &&
        ie->descriptor_config.magic_code.ia_code == MKTAG('i', 'a', 'm', 'f')) {
      ie->descriptor_config.magic_code.obu_redundant_copy = 1;
    }
    ie->descriptor_config.magic_code.ia_code = MKTAG('i', 'a', 'm', 'f');
    ie->descriptor_config.magic_code.version = 0;
    ie->descriptor_config.magic_code.profile_version = 16;
  }

  // 2. All Codec Config OBUs
  AudioElementEncoder *ae = ie->audio_element_enc;

  if (ie->codec_id == IAMF_CODEC_OPUS) {
    ie->descriptor_config.codec_config.obu_redundant_copy =
        (ie->descriptor_config.codec_config.codec_id ==
         MKTAG('O', 'p', 'u', 's'))
            ? 1
            : 0;
    ie->descriptor_config.codec_config.codec_id = MKTAG('O', 'p', 'u', 's');
    write_opus_specific_info(ie);
    ie->descriptor_config.codec_config.num_samples_per_frame = ie->frame_size;
    ie->descriptor_config.codec_config.roll_distance = -4;
  } else if (ie->codec_id == IAMF_CODEC_AAC) {
    ie->descriptor_config.codec_config.obu_redundant_copy =
        (ie->descriptor_config.codec_config.codec_id ==
         MKTAG('m', 'p', '4', 'a'))
            ? 1
            : 0;
    ie->descriptor_config.codec_config.codec_id = MKTAG('m', 'p', '4', 'a');
    write_aac_specific_info(ie);
    ie->descriptor_config.codec_config.num_samples_per_frame = ie->frame_size;
    ie->descriptor_config.codec_config.roll_distance = -1;
  } else if (ie->codec_id == IAMF_CODEC_FLAC) {
    ie->descriptor_config.codec_config.obu_redundant_copy =
        (ie->descriptor_config.codec_config.codec_id ==
         MKTAG('f', 'L', 'a', 'C'))
            ? 1
            : 0;
    ie->descriptor_config.codec_config.codec_id = MKTAG('f', 'L', 'a', 'C');
    write_flac_specific_info(ie);
    ie->descriptor_config.codec_config.num_samples_per_frame = ie->frame_size;
  } else if (ie->codec_id == IAMF_CODEC_PCM) {
    ie->descriptor_config.codec_config.obu_redundant_copy =
        (ie->descriptor_config.codec_config.codec_id ==
         MKTAG('i', 'p', 'c', 'm'))
            ? 1
            : 0;
    ie->descriptor_config.codec_config.codec_id = MKTAG('i', 'p', 'c', 'm');
    write_lpcm_specific_info(ie);
    ie->descriptor_config.codec_config.num_samples_per_frame = ie->frame_size;
    ie->descriptor_config.codec_config.roll_distance = -1;
  }

  // 3. All Mix Presentation OBUs

  // 4. All Audio Element OBUs
  ae = ie->audio_element_enc;
  element_index = 0;
  while (ae) {
    AudioElement *audio_element =
        &(ie->descriptor_config.audio_element[element_index]);
    audio_element->audio_element_id = ae->element_id;
    audio_element->audio_element_type = ae->element_type;
    audio_element->obu_redundant_copy = ae->redundant_copy;

    audio_element->codec_config_id =
        ie->descriptor_config.codec_config.codec_config_id;

    audio_element->num_substreams = ae->num_substreams;
    for (int i = 0; i < ae->num_substreams; i++) {
      audio_element->audio_substream_id[i] = ae->audio_substream_id[i];
    }

    audio_element->num_parameters = ae->num_parameters;
    for (int i = 0; i < ae->num_parameters; i++) {
      audio_element->param_definition_type[i] = ae->param_definition_type[i];
      audio_element->param_definition[i] = ae->param_definition[i];
    }
    if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
      int pre_ch = 0;
      int cl_index = 0;
      int num_layers = 0;
      for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
        int layout = ae->channel_based_enc.channel_layout_map[i];
        if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
        num_layers++;

        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .loudspeaker_layout = layout;
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .output_gain_is_present_flag =
            ae->channel_based_enc.output_gain_flag;
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .recon_gain_is_present_flag = ae->channel_based_enc.recon_gain_flag;
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .substream_count = ae->ia_core_encoder[i].stream_count;
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .coupled_substream_count =
            ae->ia_core_encoder[i].coupled_stream_count;
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .loudness.integrated_loudness =
            ae->channel_based_enc.mdhr.LKFSch[layout];
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .loudness.digital_peak =
            ae->channel_based_enc.mdhr.digital_peak[layout];
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .loudness.true_peak = ae->channel_based_enc.mdhr.true_peak[layout];

        /*
        Bit position : Channel Name
        b5(MSB)  : Left channel (L1, L2, L3)
        b4     : Right channel (R2, R3)
        b3     : Left Surround channel (Ls5)
        b2     : Right Surround channel (Rs5)
        b1     : Left Top Front channel (Ltf)
        b0     : Rigth Top Front channel (Rtf)
        */
        uint16_t output_gain_flags = 0;
        for (int j = 0; j < enc_get_layout_channel_count(layout) - pre_ch;
             j++) {
          int cl = ae->channel_based_enc.downmixer_ld->channel_order[cl_index];
          if (ae->channel_based_enc.gaindown_map[cl_index] == 1) {
            int shift = get_output_gain_flags_map[cl];
            if (shift >= 0) {
              output_gain_flags = output_gain_flags | (0x01 << shift);
            }
          }
          cl_index++;
        }
        pre_ch = enc_get_layout_channel_count(layout);

        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .output_gain_flags = output_gain_flags;
        audio_element->scalable_channel_layout_config
            .channel_audio_layer_config[i]
            .output_gain = ae->channel_based_enc.mdhr.dmixgain_db[layout];
      }
      audio_element->scalable_channel_layout_config.num_layers = num_layers;
    } else if (ae->element_type == AUDIO_ELEMENT_SCENE_BASED) {
      audio_element->ambisonics_config = ae->scene_based_enc.ambisonics_config;
    }

    ae = ae->next;
    element_index++;
  }
  ie->descriptor_config.num_audio_elements = element_index;
  ie->is_descriptor_changed = 0;
}

static void update_ia_sync(IAMF_Encoder *ie) {
  AudioElementEncoder *ae = ie->audio_element_enc;
  int num_obu_id = 0;
  while (ae) {
    ie->sync_syntax.global_offset = 0;
    for (int i = 0; i < ae->num_parameters; i++) {
      ie->sync_syntax.obu_id[num_obu_id] = ae->param_definition[i].parameter_id;
      ie->sync_syntax.obu_data_type[num_obu_id] = 1;
      ie->sync_syntax.reinitialize_decoder[num_obu_id] =
          (ae->padding > 0) ? ae->initial_padding : 0;
      ie->sync_syntax.relative_offset[num_obu_id] = 0;
      num_obu_id++;
    }
    for (int i = 0; i < ae->num_substreams; i++) {
      ie->sync_syntax.obu_id[num_obu_id] = ae->audio_substream_id[i];
      ie->sync_syntax.obu_data_type[num_obu_id] = 0;
      ie->sync_syntax.reinitialize_decoder[num_obu_id] = 0;
      ie->sync_syntax.relative_offset[num_obu_id] = 0;
      num_obu_id++;
    }
    ae = ae->next;
  }
  ie->sync_syntax.num_obu_ids = num_obu_id;
  // ie->sync_syntax.concatenation_rule = 0;
}

int IAMF_audio_element_add(IAMF_Encoder *ie, AudioElementType element_type,
                           AudioElementConfig element_config) {
  AudioElementEncoder *audio_element_enc =
      (AudioElementEncoder *)malloc(sizeof(AudioElementEncoder));
  memset(audio_element_enc, 0x00, sizeof(AudioElementEncoder));
  audio_element_enc->element_id =
      insert_obu_node(ie->root_node, OBU_IA_Audio_Element,
                      ie->descriptor_config.codec_config.codec_config_id);
  audio_element_enc->element_type = element_type;
  audio_element_enc->input_sample_rate = ie->input_sample_rate;
  audio_element_enc->bits_per_sample = ie->bits_per_sample;
  audio_element_enc->sample_format = ie->sample_format;
  audio_element_enc->frame_size = ie->frame_size;
  audio_element_enc->preskip_size = ie->preskip_size;
  audio_element_enc->codec_id = ie->codec_id;

  int channel_groups = 1;
  if (element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    audio_element_enc->channel_based_enc.layout_in = element_config.layout_in;
    // exception handle to mono input.
    if (element_config.layout_in == IA_CHANNEL_LAYOUT_MONO)
      channel_groups =
          get_scalable_format(audio_element_enc, IA_CHANNEL_LAYOUT_STEREO,
                              element_config.layout_cb);
    else
      channel_groups =
          get_scalable_format(audio_element_enc, element_config.layout_in,
                              element_config.layout_cb);
  } else if (element_type == AUDIO_ELEMENT_SCENE_BASED) {
    audio_element_enc->channels = element_config.input_channels;
    audio_element_enc->scene_based_enc.ambisonics_config.ambisonics_mode =
        element_config.ambisonics_mode;
    if (element_config.ambisonics_mode == AMBISONICS_MONO) {
      audio_element_enc->scene_based_enc.ambisonics_config
          .ambisonics_mono_config = element_config.ambisonics_mono_config;

      audio_element_enc->ia_core_encoder[0].channel =
          element_config.input_channels;
      audio_element_enc->ia_core_encoder[0].stream_count =
          element_config.ambisonics_mono_config.substream_count;
      audio_element_enc->ia_core_encoder[0].coupled_stream_count = 0;
      for (int i = 0; i < audio_element_enc->ia_core_encoder[0].stream_count;
           i++) {
        audio_element_enc->ia_core_encoder[channel_groups].enc_stream_map[i] =
            element_config.ambisonics_mono_config.channel_mapping[i];
      }

    } else if (element_config.ambisonics_mode == AMBISONICS_PROJECTION) {
      audio_element_enc->scene_based_enc.ambisonics_config
          .ambisonics_projection_config =
          element_config.ambisonics_projection_config;
      audio_element_enc->ia_core_encoder[0].channel =
          element_config.input_channels;
      audio_element_enc->ia_core_encoder[0].stream_count =
          element_config.ambisonics_projection_config.substream_count;
      audio_element_enc->ia_core_encoder[0].coupled_stream_count =
          element_config.ambisonics_projection_config.coupled_substream_count;
    }
  }
  audio_element_enc->channel_groups = channel_groups;
  if (channel_groups == 0) exit(-1);

  //////////////////////////////////////////////////////////////
  // dep codec select.
  for (int i = 0;; i++) {
    if (dep_encoders[i].opcode == ie->codec_id ||
        dep_encoders[i].opcode == -1) {
      audio_element_enc->encode_init = dep_encoders[i].init;
      audio_element_enc->encode_ctl = dep_encoders[i].control;
      audio_element_enc->encode_frame = dep_encoders[i].encode;
      audio_element_enc->encode_close = dep_encoders[i].close;

      audio_element_enc->encode_init2 = dep_encoders2[i].init;
      audio_element_enc->encode_ctl2 = dep_encoders2[i].control;
      audio_element_enc->encode_frame2 = dep_encoders2[i].encode;
      audio_element_enc->encode_close2 = dep_encoders2[i].close;
      break;
    }
  }

  for (int i = 0;; i++) {
    if (dep_decoders[i].opcode == ie->codec_id ||
        dep_decoders[i].opcode == -1) {
      audio_element_enc->decode_init = dep_decoders[i].init;
      audio_element_enc->decode_frame = dep_decoders[i].decode;
      audio_element_enc->decode_close = dep_decoders[i].close;
      break;
    }
  }

  if (audio_element_enc->encode_init == NULL ||
      audio_element_enc->decode_init == NULL) {
    ia_loge("Codec:%d is not supported\n", ie->codec_id);
    free(ie);
    return -1;
  }
  //////////////////////////////////////////////////////////////

  audio_element_enc->encode_init(audio_element_enc);
  ia_loge("Dep Codec: %s\n", dep_codec_name[ie->codec_id]);

  if (element_type == AUDIO_ELEMENT_CHANNEL_BASED)
    channel_based_ia_encoder_open(audio_element_enc);
  else if (element_type == AUDIO_ELEMENT_SCENE_BASED)
    scene_based_ia_encoder_open(audio_element_enc);

  AudioElementEncoder *audio_element_enc_last = NULL;
  if (ie->audio_element_enc == NULL)
    ie->audio_element_enc = audio_element_enc;
  else {
    audio_element_enc_last = ie->audio_element_enc;
    while (audio_element_enc_last->next) {
      audio_element_enc_last = audio_element_enc_last->next;
    }
    audio_element_enc_last->next = audio_element_enc;
  }

  int num_substreams = 0;
  for (int i = 0; i < audio_element_enc->channel_groups; i++) {
    for (int j = 0; j < audio_element_enc->ia_core_encoder[i].stream_count;
         j++) {
      audio_element_enc->audio_substream_id[num_substreams] = insert_obu_node(
          ie->root_node, OBU_IA_Audio_Frame, audio_element_enc->element_id);
      num_substreams++;
    }
  }
  audio_element_enc->num_substreams = num_substreams;

  if (audio_element_enc->channel_groups > 1) {
    int num_parameters = 0;
    audio_element_enc->param_definition[num_parameters].parameter_id =
        insert_obu_node(ie->root_node, OBU_IA_Parameter_Block,
                        audio_element_enc->element_id);
    audio_element_enc->param_definition[num_parameters].parameter_rate =
        ie->input_sample_rate;
    audio_element_enc->param_definition[num_parameters].param_definition_mode =
        0;
    audio_element_enc->param_definition[num_parameters].duration =
        audio_element_enc->frame_size;
    audio_element_enc->param_definition[num_parameters].num_segments = 1;
    audio_element_enc->param_definition[num_parameters]
        .constant_segment_interval = audio_element_enc->frame_size;
    audio_element_enc->param_definition_type[num_parameters] =
        PARAMETER_DEFINITION_DEMIXING_INFO;
    num_parameters++;
    if (audio_element_enc->channel_based_enc.recon_gain_flag) {
      audio_element_enc->param_definition[num_parameters].parameter_id =
          insert_obu_node(ie->root_node, OBU_IA_Parameter_Block,
                          audio_element_enc->element_id);
      audio_element_enc->param_definition[num_parameters].parameter_rate =
          ie->input_sample_rate;
      audio_element_enc->param_definition[num_parameters]
          .param_definition_mode = 0;
      audio_element_enc->param_definition[num_parameters].duration =
          audio_element_enc->frame_size;
      audio_element_enc->param_definition[num_parameters].num_segments = 1;
      audio_element_enc->param_definition[num_parameters]
          .constant_segment_interval = audio_element_enc->frame_size;
      audio_element_enc->param_definition_type[num_parameters] =
          PARAMETER_DEFINITION_RECON_GAIN_INFO;
      num_parameters++;
    }
    audio_element_enc->num_parameters = num_parameters;
  }

  ie->need_place_sync = 1;
  ie->is_descriptor_changed = 1;

  audio_element_enc->audio_element_obu =
      (unsigned char *)malloc(MAX_DESCRIPTOR_OBU_SIZE);
  audio_element_enc->audio_frame_obu = (unsigned char *)malloc(
      audio_element_enc->channels * MAX_BITS_PER_SAMPLE *
      audio_element_enc->frame_size *
      2);  // 1 audio element, allocate twofold to avoid overflow if adding OBU
           // header
  audio_element_enc->parameter_demixing_obu =
      (unsigned char *)malloc(MAX_DESCRIPTOR_OBU_SIZE);
  audio_element_enc->parameter_recon_gain_obu =
      (unsigned char *)malloc(MAX_DESCRIPTOR_OBU_SIZE);

  return audio_element_enc->element_id;
}

void IAMF_audio_element_delete(IAMF_Encoder *ie, int element_id) {
  AudioElementEncoder *audio_element_enc = ie->audio_element_enc;
  AudioElementEncoder *audio_element_enc_last = audio_element_enc;
  while (audio_element_enc) {
    if (audio_element_enc->element_id == element_id) break;
    audio_element_enc_last = audio_element_enc;
    audio_element_enc = audio_element_enc->next;
  }
  if (audio_element_enc == NULL) {
    ia_loge("Can not find the element id in IA Encoder: %d\n", element_id);
    return;
  }

  if (audio_element_enc->element_type == AUDIO_ELEMENT_CHANNEL_BASED)
    channel_based_ia_encoder_close(&(audio_element_enc->channel_based_enc));
  else if (audio_element_enc->element_type == AUDIO_ELEMENT_SCENE_BASED)
    scene_based_ia_encoder_close(&(audio_element_enc->scene_based_enc));

  audio_element_enc->encode_close(audio_element_enc);
  if (audio_element_enc->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    audio_element_enc->encode_close2(audio_element_enc);
    audio_element_enc->decode_close(audio_element_enc);
  }

  delete_obu_node(ie->root_node, OBU_IA_Audio_Element,
                  audio_element_enc->element_id);

  if (audio_element_enc_last == audio_element_enc)
    ie->audio_element_enc = audio_element_enc->next;
  else
    audio_element_enc_last->next = audio_element_enc->next;

  if (audio_element_enc->audio_element_obu) {
    free(audio_element_enc->audio_element_obu);
    audio_element_enc->audio_element_obu = NULL;
  }

  if (audio_element_enc->audio_frame_obu) {
    free(audio_element_enc->audio_frame_obu);
    audio_element_enc->audio_frame_obu = NULL;
  }

  if (audio_element_enc->parameter_demixing_obu) {
    free(audio_element_enc->parameter_demixing_obu);
    audio_element_enc->parameter_demixing_obu = NULL;
  }

  if (audio_element_enc->parameter_recon_gain_obu) {
    free(audio_element_enc->parameter_recon_gain_obu);
    audio_element_enc->parameter_recon_gain_obu = NULL;
  }

  ie->need_place_sync = 1;
  ie->is_descriptor_changed = 1;

  free(audio_element_enc);
  audio_element_enc = NULL;
}

void IAMF_encoder_set_mix_presentation(IAMF_Encoder *ie,
                                       MixPresentation mix_presentation) {
  int mix_presentation_id = ie->descriptor_config.num_mix_presentations;
  memcpy(&(ie->descriptor_config.mix_presentation[mix_presentation_id]),
         &mix_presentation, sizeof(MixPresentation));
  ie->descriptor_config.num_mix_presentations++;
}

void IAMF_encoder_clear_mix_presentation(IAMF_Encoder *ie) {
  ie->descriptor_config.num_mix_presentations = 0;
}

int IAMF_encoder_scalable_loudnessgain_measure(IAMF_Encoder *ie, int element_id,
                                               const void *pcm,
                                               int frame_size) {
  unsigned char *pcm_data = pcm;
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }

  float *pcm_f = (float *)malloc(ae->channels * frame_size * sizeof(float));
  ChannelBasedEnc *ce = &(ae->channel_based_enc);
  if (ce->layout_in == IA_CHANNEL_LAYOUT_MONO) {
    pcm_data = (unsigned char *)malloc(ae->channels * frame_size *
                                       MAX_BITS_PER_SAMPLE);  // max bits
    mono2stereo2(pcm, pcm_data, ae->channels, frame_size, ie->bits_per_sample,
                 ie->sample_format);
  }
  interleaved2interleaved_pcm2float(pcm_data, pcm_f, ae->channels, frame_size,
                                    ie->bits_per_sample, ie->sample_format);
  uint8_t dmix_index = default_dmix_index, w_index = default_w_index;
  QueuePop(&(ce->queue_dm[QUEUE_DMPD]), &dmix_index, 1);
  QueuePop(&(ce->queue_wg[QUEUE_DMPD]), &w_index, 1);

  QueuePush(&(ce->queue_dm[QUEUE_LD]), &dmix_index);
  QueuePush(&(ce->queue_wg[QUEUE_LD]), &w_index);

  /////////////////////////////////////////////////
  // fprintf(stderr, "dmix_index %d , w_index %d \n", dmix_index, w_index);

  downmix2(ce->downmixer_ld, pcm_f, frame_size, dmix_index, w_index);

  unsigned char pre_ch = 0;
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    immersive_audio_encoder_loudness_measure(
        ce->loudgain, ce->downmixer_ld->downmix_m[lay_out], lay_out);

#ifdef INTER_FILE_DUMP
    float temp[IA_FRAME_MAXSIZE * MAX_CHANNELS];
    plane2interleaved_float2float(ce->downmixer_ld->downmix_m[lay_out], temp,
                                  enc_get_layout_channel_count(lay_out),
                                  ie->frame_size);
    ia_intermediate_file_write(ce, FILE_DOWNMIX_M, downmix_m_wav[lay_out], temp,
                               ie->frame_size);

    plane2interleaved_float2float(
        ce->downmixer_ld->downmix_s[lay_out], temp,
        enc_get_layout_channel_count(lay_out) - pre_ch, ie->frame_size);
    ia_intermediate_file_write(ce, FILE_DOWNMIX_S, downmix_s_wav[lay_out], temp,
                               ie->frame_size);
    pre_ch = enc_get_layout_channel_count(lay_out);
#endif
  }

  pre_ch = 0;
  int cl_index = 0;
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT ||
        lay_out == IA_CHANNEL_LAYOUT_BINAURAL)
      break;
    for (int j = 0; j < enc_get_layout_channel_count(lay_out) - pre_ch; j++) {
      int cl = ce->downmixer_ld->channel_order[cl_index];
      if (ce->downmixer_ld->gaindown_map[lay_out][cl] ||
          (lay_out != IA_CHANNEL_LAYOUT_STEREO &&
           cl == enc_channel_l2))  // Mono cases
      {
        ce->gaindown_map[cl_index] = 1;
        immersive_audio_encoder_gain_measure2(
            ce->loudgain, ce->downmixer_ld->downmix_s[lay_out], lay_out, j,
            cl_index);
      }
      cl_index++;
    }
    pre_ch = enc_get_layout_channel_count(lay_out);
  }

  if (ce->layout_in == IA_CHANNEL_LAYOUT_MONO) {
    if (pcm_data) free(pcm_data);
  }
  if (pcm_f) free(pcm_f);
  return 0;
}

int IAMF_encoder_scalable_loudnessgain_stop(IAMF_Encoder *ie, int element_id) {
  int ret = 0;
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }
  ChannelBasedEnc *ce = &(ae->channel_based_enc);
  LoudGainMeasure *lm = ce->loudgain;
  if (lm->measure_end) return ret;
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = lm->channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    lm->loudmeter[layout].stopIntegrated(&(lm->loudmeter[layout]));
    lm->loudmeter[layout].processMomentaryLoudness(&(lm->loudmeter[layout]),
                                                   lm->msize25pct);

    lm->entire_loudness[layout] =
        lm->loudmeter[layout].getIntegratedLoudness(&(lm->loudmeter[layout]));
    lm->entire_peaksqr[layout] =
        lm->loudmeter[layout].getEntirePeakSquare(&(lm->loudmeter[layout]));
    lm->entire_truepeaksqr[layout] =
        lm->loudmeter[layout].getEntireTruePeakSquare(&(lm->loudmeter[layout]));
  }
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = lm->channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    lm->dmixgain_lin[layout] =
        db2lin(-1.0) / sqrt(lm->entire_truepeaksqr_gain[layout]);
    if (lm->dmixgain_lin[layout] > 1) lm->dmixgain_lin[layout] = 1;
    lm->dmixgain_lin[layout] = lin2db(lm->dmixgain_lin[layout]);
  }
  lm->measure_end = 1;

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = lm->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    ia_logw("[%s]entireLoudness: %f LKFS\n", channel_layout_names[lay_out],
            ce->loudgain->entire_loudness[lay_out]);
    ce->mdhr.LKFSch[lay_out] =
        float_to_q(ce->loudgain->entire_loudness[lay_out], 8);
    ce->mdhr.digital_peak[lay_out] =
        float_to_q(lin2db(lm->entire_peaksqr[lay_out]), 8);
    ce->mdhr.true_peak[lay_out] =
        float_to_q(lin2db(lm->entire_truepeaksqr[lay_out]), 8);
  }

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = lm->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    if (lm->gaindown_flag[lay_out] == 0) continue;
    ia_logw("[%s]dmixgain: %f dB\n", channel_layout_names[lay_out],
            ce->loudgain->dmixgain_lin[lay_out]);
    ce->mdhr.dmixgain_db[lay_out] =
        float_to_q(ce->loudgain->dmixgain_lin[lay_out], 8);
    ce->mdhr.dmixgain_f[lay_out] =
        db2lin(q_to_float(ce->mdhr.dmixgain_db[lay_out], 8));
  }

  return ret;
}

static int ret_sound_system_valid(IAMF_SoundSystem ss) {
  return ss >= SOUND_SYSTEM_A && ss <= SOUND_SYSTEM_EXT_312;
}

static int get_sound_system_channels_count_without_lfe(IAMF_SoundSystem ss) {
  static int ss_channels[] = {2, 5, 7, 9, 10, 10, 13, 22, 7, 11, 9, 5};
  return ss_channels[ss];
}

static int get_sound_system_lfe1(IAMF_SoundSystem ss) {
  return ss != SOUND_SYSTEM_A;
}

static int get_sound_system_lfe2(IAMF_SoundSystem ss) {
  return ss == SOUND_SYSTEM_F || ss == SOUND_SYSTEM_H;
}

static int get_layout_channels_count(IAMFLayout *layout) {
  int ret = 0;
  if (layout->layout_type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    ret = get_sound_system_channels_count_without_lfe(layout->sound_system);
    ret += get_sound_system_lfe1(layout->sound_system);
    ret += get_sound_system_lfe2(layout->sound_system);
  } else if (layout->layout_type == IAMF_LAYOUT_TYPE_BINAURAL) {
    ret = 2;
  }

  return ret;
}

static channelLayout iamflayout2channellayout(IAMFLayout *layout) {
  if (layout->layout_type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    channelLayout ret[] = {CHANNELSTEREO,  CHANNEL51,      CHANNEL512,
                           CHANNEL514,     CHANNELUNKNOWN, CHANNELUNKNOWN,
                           CHANNELUNKNOWN, CHANNELUNKNOWN, CHANNEL71,
                           CHANNEL714,     CHANNEL712,     CHANNEL312};
    return ret[layout->sound_system];
  } else if (layout->layout_type == IAMF_LAYOUT_TYPE_BINAURAL) {
    return CHANNELSTEREO;
  }
  return CHANNELUNKNOWN;
}

static uint32_t get_sound_system_get_rendering_id(IAMF_SoundSystem ss) {
  static IAMF_SOUND_SYSTEM ss_rids[] = {BS2051_A, BS2051_B, BS2051_C, BS2051_D,
                                        BS2051_E, BS2051_F, BS2051_G, BS2051_H,
                                        BS2051_I, BS2051_J, IAMF_712, IAMF_312};
  return ss_rids[ss];
}

static uint32_t get_layer_layout_get_rendering_id(int layer_layout) {
  static IAMF_SOUND_SYSTEM l_rids[] = {
      IAMF_MONO, IAMF_STEREO, IAMF_51,  IAMF_512, IAMF_514,
      IAMF_71,   IAMF_712,    IAMF_714, IAMF_312, IAMF_BINAURAL};
  return l_rids[layer_layout];
}

static int get_layer_layout_lfe1(int layer_layout) {
  return layer_layout > IA_CHANNEL_LAYOUT_STEREO &&
         layer_layout < IA_CHANNEL_LAYOUT_BINAURAL;
}

static int layout_lfe_check(IAMFLayout *layout) {
  int ret = 0;
  ret += get_sound_system_lfe1(layout->sound_system);
  ret += get_sound_system_lfe2(layout->sound_system);
  return !!ret;
}

static int get_stream_ambisionisc_order(int channels) {
  if (channels == 4)
    return IAMF_FOA;
  else if (channels == 9)
    return IAMF_SOA;
  else if (channels == 16)
    return IAMF_TOA;
  return 0;
}

static void render_mixgain(float *in, float *out, int nch, int frame_size,
                           float gain_db) {
  for (int i = 0; i < nch; i++) {
    for (int j = 0; j < frame_size; j++) {
      out[i * frame_size + j] += in[i * frame_size + j] * db2lin(gain_db);
    }
  }
}

int IAMF_encoder_target_loudness_measure_start(IAMF_Encoder *ie,
                                               MixPresentation *mixp) {
  int layouts = mixp->num_sub_mixes * mixp->num_layouts;
  LoudnessTarget *loudness_target = NULL;
  if (mixp->num_sub_mixes * mixp->num_layouts > MAX_MEASURED_LAYOUT_NUM) {
    ia_loge("Too manny target layout setting\n");
    return -1;
  }
  int layout = 0;
  for (int i = 0; i < mixp->num_sub_mixes; i++) {
    for (int j = 0; j < mixp->num_layouts; j++) {
      loudness_target = &(ie->descriptor_config.loudness_target[layout]);
      loudness_target->frame_size = ie->frame_size;
      AudioLoudMeterInit(&(loudness_target->loudmeter));
      loudness_target->loudmeter.initParams(&(loudness_target->loudmeter), 0.4f,
                                            0.75f, 3.0f);
      loudness_target->loudmeter.prepare(
          &(loudness_target->loudmeter), ie->input_sample_rate,
          get_layout_channels_count(&(mixp->loudness_layout[j])),
          iamflayout2channellayout(&(mixp->loudness_layout[j])));
      loudness_target->loudmeter.startIntegrated(&(loudness_target->loudmeter));

      loudness_target->msize25pct = ie->input_sample_rate / 10 / ie->frame_size;
      layout++;
    }
  }
  return 0;
}

int IAMF_encoder_target_loudness_measure(IAMF_Encoder *ie,
                                         MixPresentation *mixp,
                                         IAFrame *frame) {
  float *mix_pcm =
      (float *)malloc(MAX_OUTPUT_CHANNELS * sizeof(float) * ie->frame_size);
  float *min =
      (float *)malloc(MAX_OUTPUT_CHANNELS * sizeof(float) * ie->frame_size);
  float *mout =
      (float *)malloc(MAX_OUTPUT_CHANNELS * sizeof(float) * ie->frame_size);

  for (int i = 0; i < mixp->num_layouts; i++) {
    memset(mix_pcm, 0x00,
           (MAX_OUTPUT_CHANNELS * sizeof(float) * ie->frame_size));
    memset(mout, 0x00, (MAX_OUTPUT_CHANNELS * sizeof(float) * ie->frame_size));
    memset(min, 0x00, (MAX_OUTPUT_CHANNELS * sizeof(float) * ie->frame_size));
    for (int j = 0; j < mixp->num_audio_elements; j++) {
      int element_id = mixp->audio_element_id[j];
      AudioElementEncoder *ae = ie->audio_element_enc;
      while (ae) {
        if (ae->element_id == element_id) break;
        ae = ae->next;
      }
      if (!ae) {
        ia_loge("invalid element id\n");
        return -1;
      }
      IAFrame *iframe = frame;
      while (ae) {
        if (iframe->element_id == element_id) break;
        iframe = iframe->next;
      }
      if (!iframe) {
        ia_loge("invalid element id\n");
        return -1;
      }

      IAMF_SP_LAYOUT lout;
      IAMF_PREDEFINED_SP_LAYOUT *l = (IAMF_PREDEFINED_SP_LAYOUT *)malloc(
          sizeof(IAMF_PREDEFINED_SP_LAYOUT));
      l->system = get_sound_system_get_rendering_id(
          mixp->loudness_layout[i].sound_system);
      l->lfe1 = get_sound_system_lfe1(mixp->loudness_layout[i].sound_system);
      l->lfe2 = get_sound_system_lfe2(mixp->loudness_layout[i].sound_system);

      lout.sp_type = 0;
      lout.sp_layout.predefined_sp = l;
      int out_channles = get_layout_channels_count(&mixp->loudness_layout[i]);
      float **sout = (float **)malloc(out_channles * sizeof(float *));
      float **sin = (float **)malloc(ae->channels * sizeof(float *));
      for (int n = 0; n < out_channles; ++n) {
        sout[n] = &mout[ie->frame_size * n];
      }
      for (int n = 0; n < ae->channels; ++n) {
        sin[n] = &min[ie->frame_size * n];
      }

      interleaved2plane_pcm2float(iframe->pcm, min, ae->channels,
                                  iframe->frame_size, ie->bits_per_sample,
                                  ie->sample_format);
      if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
        struct m2m_rdr_t m2m;
        IAMF_SP_LAYOUT lin;
        IAMF_PREDEFINED_SP_LAYOUT pin;

        int channel_layout = ae->channel_based_enc.layout_in;
        lin.sp_type = 0;
        lin.sp_layout.predefined_sp = &pin;
        pin.system = get_layer_layout_get_rendering_id(channel_layout);
        pin.lfe1 = get_layer_layout_lfe1(channel_layout);
        pin.lfe2 = 0;

        IAMF_element_renderer_get_M2M_matrix(&lin, &lout, &m2m);
        IAMF_element_renderer_render_M2M(&m2m, sin, sout, ie->frame_size);
      } else if (ae->element_type == AUDIO_ELEMENT_SCENE_BASED) {
        lfe_filter_t *plfe = 0;
        struct h2m_rdr_t h2m;
        IAMF_HOA_LAYOUT hin;
        hin.order = get_stream_ambisionisc_order(ae->channels);

        hin.lfe_on = 1;  // turn on LFE of HOA ##SR

        IAMF_element_renderer_get_H2M_matrix(&hin, lout.sp_layout.predefined_sp,
                                             &h2m);
        if (hin.lfe_on && layout_lfe_check(&mixp->loudness_layout[i])) {
          plfe = &lout.lfe_f;
        }
        IAMF_element_renderer_render_H2M(&h2m, sin, sout, ie->frame_size, plfe);
      }

      render_mixgain(mout, mix_pcm, out_channles, ie->frame_size,
                     mixp->element_mix_config[j].default_mix_gain);

      if (l) free(l);
      if (sin) {
        free(sin);
      }
      if (sout) {
        free(sout);
      }
    }
    LoudnessTarget *loudness_target =
        &(ie->descriptor_config.loudness_target[i]);
    loudness_target->loudmeter.processFrameLoudness(
        &(loudness_target->loudmeter), mix_pcm, loudness_target->msize25pct,
        loudness_target->frame_size);
  }

  if (mix_pcm) free(mix_pcm);
  if (min) free(min);
  if (mout) free(mout);
  return 0;
}

int IAMF_encoder_target_loudness_measure_stop(IAMF_Encoder *ie,
                                              MixPresentation *mixp) {
  for (int i = 0; i < mixp->num_layouts; i++) {
    LoudnessTarget *loudness_target =
        &(ie->descriptor_config.loudness_target[i]);
    loudness_target->loudmeter.stopIntegrated(&(loudness_target->loudmeter));
    loudness_target->loudmeter.processMomentaryLoudness(
        &(loudness_target->loudmeter), loudness_target->msize25pct);

    float entire_loudness = loudness_target->loudmeter.getIntegratedLoudness(
        &(loudness_target->loudmeter));
    ;
    float entire_peaksqr = loudness_target->loudmeter.getEntirePeakSquare(
        &(loudness_target->loudmeter));
    ;
    float entire_truepeaksqr =
        loudness_target->loudmeter.getEntireTruePeakSquare(
            &(loudness_target->loudmeter));
    ia_logw("sound system:[%d] entireLoudness: %f LKFS\n",
            mixp->loudness_layout[i].sound_system, entire_loudness);

    mixp->loudness[i].integrated_loudness = float_to_q(entire_loudness, 8);
    mixp->loudness[i].info_type = 0;
    mixp->loudness[i].digital_peak = float_to_q(lin2db(entire_peaksqr), 8);
    mixp->loudness[i].true_peak = float_to_q(lin2db(entire_truepeaksqr), 8);

    AudioLoudMeterDeinit(&(loudness_target->loudmeter));
  }
  memset(ie->descriptor_config.loudness_target, 0x00,
         sizeof(LoudnessTarget) * MAX_MEASURED_LAYOUT_NUM);

  return 0;
}

// signed 16->signed 16
static int extract_pcm_from_group(int16_t *input, int16_t *out, int nch,
                                  int ith, int single, int frame_size) {
  int channle_index = 0;
  if (single) {
    for (int i = 0; i < frame_size; i++) {
      out[i] = input[i * nch + ith];
    }
    channle_index = ith + 1;
  } else  // couple
  {
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < frame_size; j++) {
        out[j * 2 + i] = input[j * nch + ith + i];
      }
    }
    channle_index = ith + 2;
  }
  return channle_index;
}

// float->signed 16/24/32
static int extract_pcm_from_group2(float *src, void *dst, int nch, int ith,
                                   int single, int frame_size,
                                   int bits_per_sample, int sample_format) {
  int channle_index = 0;
  if (single) {
    if (sample_format == 1) {  // little-endian
      if (bits_per_sample == 16) {
        float den = 0x8000 - 1.0f;
        int16_t *out = (int16_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < frame_size; i++) {
          out[i] = (int16_t)(in[i * nch + ith] * den);
        }
      } else if (bits_per_sample == 24) {
        float den = 0x800000 - 1.0f;
        float *in = (float *)src;
        unsigned char *out = (unsigned char *)dst;
        for (int i = 0; i < frame_size; i++) {
          int32_t a = (int32_t)(in[i * nch + ith] * den);
          out[i * 3] = a & 0x000000FF;
          out[i * 3 + 1] = (a >> 8) & 0x000000FF;
          out[i * 3 + 1] = (a >> 16) & 0x000000FF;
        }
      } else if (bits_per_sample == 32) {
        float den = 0x80000000 - 1.0f;
        int32_t *out = (int32_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < frame_size; i++) {
          out[i] = (int32_t)(in[i * nch + ith] * den);
        }
      }
    } else if (sample_format == 0) {
      if (bits_per_sample == 16) {
        float den = 0x8000 - 1.0f;
        int16_t *out = (int16_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < frame_size; i++) {
          out[i] = BigToLittle16((int16_t)(in[i * nch + ith] * den));
        }
      } else if (bits_per_sample == 24) {
        float den = 0x800000 - 1.0f;
        unsigned char *in = (unsigned char *)src;
        unsigned char *out = (unsigned char *)dst;
        for (int i = 0; i < frame_size; i++) {
          int32_t a = (int32_t)(in[i * nch + ith] * den);
          out[i * 3] = (a >> 16) & 0x000000FF;
          out[i * 3 + 1] = (a >> 8) & 0x000000FF;
          out[i * 3 + 1] = a & 0x000000FF;
        }
      } else if (bits_per_sample == 32) {
        float den = 0x80000000 - 1.0f;
        int32_t *out = (int32_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < frame_size; i++) {
          out[i] = BigToLittle32((int32_t)(in[i * nch + ith] * den));
        }
      }
    }
    channle_index = ith + 1;
  } else {
    if (sample_format == 1) {  // little-endian
      if (bits_per_sample == 16) {
        float den = 0x8000 - 1.0f;
        int16_t *out = (int16_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < 2; i++) {
          for (int j = 0; j < frame_size; j++) {
            out[j * 2 + i] = (int16_t)(in[j * nch + ith + i] * den);
          }
        }
      } else if (bits_per_sample == 24) {
        float den = 0x800000 - 1.0f;
        unsigned char *in = (unsigned char *)src;
        unsigned char *out = (unsigned char *)dst;
        for (int i = 0; i < 2; i++) {
          for (int j = 0; j < frame_size; j++) {
            int32_t a = (int32_t)(in[j * nch + ith + i] * den);
            out[(j * 2 + i) * 3] = a & 0x000000FF;
            out[(j * 2 + i) * 3 + 1] = (a >> 8) & 0x000000FF;
            out[(j * 2 + i) * 3 + 2] = (a >> 16) & 0x000000FF;
          }
        }
      } else if (bits_per_sample == 32) {
        float den = 0x80000000 - 1.0f;
        int32_t *out = (int32_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < 2; i++) {
          for (int j = 0; j < frame_size; j++) {
            out[j * 2 + i] = (int32_t)(in[j * nch + ith + i] * den);
          }
        }
      }
    } else if (sample_format == 0) {
      if (bits_per_sample == 16) {
        float den = 0x8000 - 1.0f;
        int16_t *out = (int16_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < 2; i++) {
          for (int j = 0; j < frame_size; j++) {
            out[j * 2 + i] =
                BigToLittle16((int16_t)(in[j * nch + ith + i] * den));
          }
        }
      } else if (bits_per_sample == 24) {
        float den = 0x800000 - 1.0f;
        unsigned char *in = (unsigned char *)src;
        unsigned char *out = (unsigned char *)dst;
        for (int i = 0; i < 2; i++) {
          for (int j = 0; j < frame_size; j++) {
            int32_t a = (int32_t)(in[j * nch + ith + i] * den);
            out[(j * 2 + i) * 3] = (a >> 16) & 0x000000FF;
            out[(j * 2 + i) * 3 + 1] = (a >> 8) & 0x000000FF;
            out[(j * 2 + i) * 3 + 2] = a & 0x000000FF;
          }
        }
      } else if (bits_per_sample == 32) {
        float den = 0x80000000 - 1.0f;
        int32_t *out = (int32_t *)dst;
        float *in = (float *)src;
        for (int i = 0; i < 2; i++) {
          for (int j = 0; j < frame_size; j++) {
            out[j * 2 + i] =
                BigToLittle32((int32_t)(in[j * nch + ith + i] * den));
          }
        }
      }
    }
    channle_index = ith + 2;
  }
  return channle_index;
}

// signed 16/24/32->signed 16/24/32
static int extract_pcm_from_group3(void *src, void *dst, int nch, int ith,
                                   int single, int frame_size,
                                   int bits_per_sample, int sample_format) {
  int channle_index = 0;
  if (single) {
    if (bits_per_sample == 16) {
      float den = 0x8000 - 1.0f;
      int16_t *out = (int16_t *)dst;
      int16_t *in = (int16_t *)src;
      for (int i = 0; i < frame_size; i++) {
        out[i] = in[i * nch + ith];
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      unsigned char *out = (unsigned char *)dst;
      for (int i = 0; i < frame_size; i++) {
        out[i * 3] = in[(i * nch + ith) * 3];
        out[i * 3 + 1] = in[(i * nch + ith) * 3 + 1];
        out[i * 3 + 1] = in[(i * nch + ith) * 3 + 2];
      }
    } else if (bits_per_sample == 32) {
      float den = 0x80000000 - 1.0f;
      int32_t *out = (int32_t *)dst;
      int32_t *in = (int32_t *)src;
      for (int i = 0; i < frame_size; i++) {
        out[i] = in[i * nch + ith];
      }
    }
    channle_index = ith + 1;
  } else {
    if (bits_per_sample == 16) {
      int16_t *out = (int16_t *)dst;
      int16_t *in = (int16_t *)src;
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[j * 2 + i] = (int16_t)(in[j * nch + ith + i]);
        }
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      unsigned char *out = (unsigned char *)dst;
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[(j * 2 + i) * 3] = in[(j * nch + ith + i) * 3];
          out[(j * 2 + i) * 3 + 1] = in[(j * nch + ith + i) * 3 + 1];
          out[(j * 2 + i) * 3 + 2] = in[(j * nch + ith + i) * 3 + 2];
        }
      }
    } else if (bits_per_sample == 32) {
      int32_t *out = (int32_t *)dst;
      int32_t *in = (int32_t *)src;
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[j * 2 + i] = in[j * nch + ith + i];
        }
      }
    }
    channle_index = ith + 2;
  }
  return channle_index;
}

static int insert_pcm_to_group(int16_t *input, int16_t *out, int nch, int ith,
                               int single, int frame_size) {
  int channle_index = 0;
  if (single) {
    for (int i = 0; i < frame_size; i++) {
      out[i * nch + ith] = input[i];
    }
    channle_index = ith + 1;
  } else {
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < frame_size; j++) {
        out[j * nch + ith + i] = input[j * 2 + i];
      }
    }
    channle_index = ith + 2;
  }
  return channle_index;
}

int IAMF_encoder_reconstruct_gain(IAMF_Encoder *ie, int element_id,
                                  const int16_t *pcm, int frame_size) {
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    if (ae->element_id == element_id) break;
    ae = ae->next;
  }
  ChannelBasedEnc *ce = &(ae->channel_based_enc);

  if (ce->recon_gain_flag == 0) return 0;

  uint8_t dmix_index = default_dmix_index, w_index = default_w_index;
  QueuePop(&(ce->queue_dm[QUEUE_LD]), &dmix_index, 1);
  QueuePop(&(ce->queue_wg[QUEUE_LD]), &w_index, 1);

  QueuePush(&(ce->queue_dm[QUEUE_SF]), &dmix_index);
  QueuePush(&(ce->queue_wg[QUEUE_SF]), &w_index);

  ce->mdhr.dmix_matrix_type = dmix_index;
  ce->mdhr.weight_type = w_index;

  int16_t *gain_down_in =
      (int16_t *)malloc(ae->frame_size * ae->channels * sizeof(int16_t));
  float *temp = (float *)malloc(ae->frame_size * ae->channels * sizeof(float));
  unsigned char *encoded_frame = (unsigned char *)malloc(
      ae->frame_size * ae->channels * MAX_BITS_PER_SAMPLE);
  int16_t *decoded_frame =
      (int16_t *)malloc(ae->frame_size * ae->channels * MAX_BITS_PER_SAMPLE);

  downmix(ce->downmixer_rg, pcm, frame_size, dmix_index, w_index);
  gaindown(ce->downmixer_rg->downmix_s, ce->channel_layout_map,
           ce->gaindown_map, ce->mdhr.dmixgain_f, ae->frame_size);

  int16_t *extract_pcm =
      (int16_t *)malloc(ae->frame_size * 2 * sizeof(int16_t));
  int16_t *extract_pcm_dec =
      (int16_t *)malloc(ae->frame_size * 2 * sizeof(int16_t));

  int pre_ch = 0;
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    plane2interleaved_float2float(ce->downmixer_rg->downmix_m[lay_out], temp,
                                  enc_get_layout_channel_count(lay_out),
                                  ae->frame_size);
    plane2interleaved_float2pcm(
        ce->downmixer_rg->downmix_s[lay_out], gain_down_in,
        enc_get_layout_channel_count(lay_out) - pre_ch, ae->frame_size);
#ifdef INTER_FILE_DUMP
    ia_intermediate_file_write(ce, FILE_GAIN_DOWN, gaindown_wav[lay_out],
                               gain_down_in, ae->frame_size);
#endif
    reorder_channels(ae->ia_core_encoder[i].enc_stream_map,
                     ae->ia_core_encoder[i].channel, ae->frame_size,
                     gain_down_in);

    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (j < ae->ia_core_encoder[i].coupled_stream_count) {
        extract_pcm_from_group(gain_down_in, extract_pcm,
                               ae->ia_core_encoder[i].channel, j * 2, 0,
                               ae->frame_size);
        int32_t encoded_size =
            ae->encode_frame2(ae, i, j, 2, extract_pcm, encoded_frame);
        int ret = ae->decode_frame(ae, i, j, 2, encoded_frame, encoded_size,
                                   extract_pcm_dec);
        insert_pcm_to_group(extract_pcm_dec, decoded_frame,
                            ae->ia_core_encoder[i].channel, j * 2, 0,
                            ae->frame_size);
      } else {
        extract_pcm_from_group(
            gain_down_in, extract_pcm, ae->ia_core_encoder[i].channel,
            ae->ia_core_encoder[i].coupled_stream_count + j, 1, ae->frame_size);
        int32_t encoded_size =
            ae->encode_frame2(ae, i, j, 1, extract_pcm, encoded_frame);
        int ret = ae->decode_frame(ae, i, j, 1, encoded_frame, encoded_size,
                                   extract_pcm_dec);
        insert_pcm_to_group(
            extract_pcm_dec, decoded_frame, ae->ia_core_encoder[i].channel,
            ae->ia_core_encoder[i].coupled_stream_count + j, 1, ae->frame_size);
      }
    }
#ifdef INTER_FILE_DUMP
    ia_intermediate_file_write(ce, FILE_DECODED, decoded_wav[lay_out],
                               decoded_frame, ae->frame_size);
#endif
    reorder_channels(ae->ia_core_decoder[i].dec_stream_map,
                     ae->ia_core_decoder[i].channel, ae->frame_size,
                     (int16_t *)decoded_frame);
    pre_ch = enc_get_layout_channel_count(lay_out);

    QueuePush(&(ce->queue_m[lay_out]), temp);
    QueuePush(&(ce->queue_d[lay_out]), decoded_frame);
  }

  int16_t *up_input[IA_CHANNEL_LAYOUT_COUNT];
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    up_input[lay_out] =
        (int16_t *)malloc(ae->frame_size * MAX_CHANNELS * sizeof(int16_t));
    memset(up_input[lay_out], 0x00,
           ae->frame_size * MAX_CHANNELS * sizeof(int16_t));
    ce->upmixer->up_input[lay_out] = up_input[lay_out];
  }
  if (QueueLength(&(ce->queue_dm[QUEUE_SF])) < ce->the_preskip_frame) {
    for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
      int lay_out = ce->channel_layout_map[i];
      if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
      QueuePop(&(ce->queue_d[lay_out]), up_input[lay_out], ae->frame_size);
      /////////////////dummy recon gain, start////////////////////
      QueuePush(&(ce->queue_rg[lay_out]), ce->mdhr.scalablefactor[lay_out]);
      /////////////////dummy recon gain, end//////////////////////
    }
    /////////////////dummy demix mode, start////////////////////
    uint8_t dmix_index = default_dmix_index, w_index = default_w_index;
    QueuePush(&(ce->queue_dm[QUEUE_RG]), &dmix_index);
    QueuePush(&(ce->queue_wg[QUEUE_RG]), &w_index);
    /////////////////dummy demix mode, end//////////////////////
  } else if (QueueLength(&(ce->queue_dm[QUEUE_SF])) == ce->the_preskip_frame) {
    uint8_t dmix_index = default_dmix_index, w_index = default_w_index;
    QueuePop(&(ce->queue_dm[QUEUE_SF]), &dmix_index, 1);
    QueuePop(&(ce->queue_wg[QUEUE_SF]), &w_index, 1);

    QueuePush(&(ce->queue_dm[QUEUE_RG]), &dmix_index);
    QueuePush(&(ce->queue_wg[QUEUE_RG]), &w_index);

    ce->upmixer->mdhr_c = ce->mdhr;
    ce->upmixer->mdhr_c.dmix_matrix_type = dmix_index;
    ce->upmixer->mdhr_c.weight_type = w_index;

    for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
      int lay_out = ce->channel_layout_map[i];
      if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
      QueuePop(&(ce->queue_d[lay_out]), up_input[lay_out], ae->frame_size);
    }

    upmix(ce->upmixer, ce->gaindown_map);
    for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
      int lay_out = ce->channel_layout_map[i];
      if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
      plane2interleaved_float2float(ce->upmixer->upmix[lay_out], temp,
                                    enc_get_layout_channel_count(lay_out),
                                    ae->frame_size);
      QueuePush(&(ce->queue_r[lay_out]), temp);
#ifdef INTER_FILE_DUMP
      ia_intermediate_file_write(ce, FILE_UPMIX, upmix_wav[lay_out], temp,
                                 ae->frame_size);  //
#endif
    }
  }

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    if (up_input[lay_out]) free(up_input[lay_out]);
  }

  int layout = ce->channel_layout_map[0];
  if (QueueLength(&(ce->queue_r[layout])) > 0) {
    float *m_input = NULL, *r_input = NULL, *s_input = NULL;
    m_input = (float *)malloc(ae->frame_size * MAX_CHANNELS * sizeof(float));
    r_input = (float *)malloc(ae->frame_size * MAX_CHANNELS * sizeof(float));
    s_input = (float *)malloc(ae->frame_size * MAX_CHANNELS * sizeof(float));
    if (QueueLength(&(ce->queue_r[layout])) == 1) {
      //
      for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
        int lay_out = ce->channel_layout_map[i];
        if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
        QueuePop(&(ce->queue_r[lay_out]), r_input, ae->preskip_size);
      }
    } else {
      ce->sf->scalefactor_mode = ce->scalefactor_mode;
      int s_channel = 0;
      int last_layout = 0;
      InScalableBuffer scalable_buff;
      memset(&scalable_buff, 0x00, sizeof(scalable_buff));
      scalable_buff.gaindown_map = ce->gaindown_map;
      int recongain_cls[enc_channel_cnt];
      for (int i = 0; i < enc_channel_cnt; i++) {
        recongain_cls[i] = -1;
      }
      for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
        int lay_out = ce->channel_layout_map[i];
        if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
        QueuePop(&(ce->queue_m[lay_out]), m_input, ae->frame_size);
        QueuePop(&(ce->queue_r[lay_out]), r_input, ae->frame_size);

        scalable_buff.channels_s = s_channel;
        scalable_buff.inbuffer_s = (unsigned char *)s_input;
        scalable_buff.dtype_s = 1;

        scalable_buff.scalable_map = ce->upmixer->scalable_map[lay_out];
        scalable_buff.relevant_mixed_cl =
            ce->upmixer->relevant_mixed_cl[last_layout];
        scalable_buff.channels_m = enc_get_layout_channel_count(lay_out);
        scalable_buff.inbuffer_m = (unsigned char *)m_input;
        scalable_buff.dtype_m = 1;

        scalable_buff.channels_r = enc_get_layout_channel_count(lay_out);
        scalable_buff.inbuffer_r = (unsigned char *)r_input;
        scalable_buff.dtype_r = 1;
        if (i != 0)
          cal_scalablefactor2(ce->sf, &(ce->mdhr), scalable_buff, lay_out,
                              last_layout, recongain_cls);
        QueuePush(&(ce->queue_rg[lay_out]),
                  ce->mdhr.scalablefactor[lay_out]);  // save recon gain

        s_channel = enc_get_layout_channel_count(lay_out);
        last_layout = lay_out;
        memcpy(s_input, m_input, ae->frame_size * MAX_CHANNELS * sizeof(float));
      }
    }
    if (m_input) free(m_input);
    if (r_input) free(r_input);
    if (s_input) free(s_input);
  }

  if (gain_down_in) free(gain_down_in);
  if (temp) free(temp);
  if (encoded_frame) free(encoded_frame);
  if (decoded_frame) free(decoded_frame);
  if (extract_pcm) free(extract_pcm);
  if (extract_pcm_dec) free(extract_pcm_dec);

  return 0;
}

static void write_demixing_obu(AudioElementEncoder *ae, int demix_mode) {
  unsigned char bitstr[255] = {
      0,
  };
  bitstream_t bs;
  unsigned char coded_data_leb[10];
  int coded_size = 0;

  ChannelBasedEnc *ce = &(ae->channel_based_enc);

  bs_init(&bs, bitstr, sizeof(bitstr));
  bs_setbits_leb128(&bs, ae->param_definition[0].parameter_id);  // parameter_id

  bs_setbits(&bs, demix_mode, 3);
  bs_setbits(&bs, 0, 5);

  ae->size_of_parameter_demixing =
      iamf_write_obu_unit(bitstr, bs.m_posBase, OBU_IA_Parameter_Block, 0, 0, 0,
                          0, 0, ae->parameter_demixing_obu);
}

static void write_recon_gain_obu(AudioElementEncoder *ae) {
  unsigned char bitstr[255] = {
      0,
  };
  bitstream_t bs;
  unsigned char coded_data_leb[10];
  int coded_size = 0;
  int putsize_recon_gain = 0;
  ChannelBasedEnc *ce = &(ae->channel_based_enc);

  bs_init(&bs, bitstr, sizeof(bitstr));
  bs_setbits_leb128(&bs, ae->param_definition[1].parameter_id);  // parameter_id

  // write recon gain obu
  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int layout = ce->channel_layout_map[i];
    if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
    putsize_recon_gain +=
        write_recon_gain(ce, bitstr + bs.m_posBase + putsize_recon_gain, i);
  }
  putsize_recon_gain += bs.m_posBase;
  ae->size_of_parameter_recon_gain =
      iamf_write_obu_unit(bitstr, putsize_recon_gain, OBU_IA_Parameter_Block, 0,
                          0, 0, 0, 0, ae->parameter_recon_gain_obu);
}

static int write_audio_frame_obu(AudioElementEncoder *ae, uint8_t *src,
                                 uint8_t *dst, int size, int substreams,
                                 uint32_t num_samples_to_trim_at_start,
                                 uint32_t num_samples_to_trim_at_end) {
  unsigned char bitstr[255] = {
      0,
  };
  bitstream_t bs;
  unsigned char coded_data_leb[10];
  int coded_size = 0;
  int size_of_audio_substream_id = 0;
  int obu_trimming_status =
      (num_samples_to_trim_at_start > 0 || num_samples_to_trim_at_end > 0) ? 1
                                                                           : 0;
  if (ae->audio_substream_id[substreams] > 21) {
    unsigned char *coded_data = (unsigned char *)malloc(
        ae->channels * sizeof(int16_t) * ae->frame_size);

    bs_init(&bs, bitstr, sizeof(bitstr));
    size_of_audio_substream_id = bs_setbits_leb128(
        &bs, ae->audio_substream_id[substreams]);  // audio_substream_id
    memcpy(coded_data, bitstr, size_of_audio_substream_id);
    memcpy(coded_data + size_of_audio_substream_id, src, size);

    uint32_t obu_size = iamf_write_obu_unit(
        coded_data, size + size_of_audio_substream_id, OBU_IA_Audio_Frame, 0,
        obu_trimming_status, num_samples_to_trim_at_start,
        num_samples_to_trim_at_end, 0, dst);
    if (coded_data) free(coded_data);
    return obu_size;
  } else {
    return iamf_write_obu_unit(
        src, size, (ae->audio_substream_id[substreams] + SUB_STREAM_ID_SHIFT),
        0, obu_trimming_status, num_samples_to_trim_at_start,
        num_samples_to_trim_at_end, 0, dst);
  }
}

static int audio_element_encode(AudioElementEncoder *ae, IAFrame *frame) {
  unsigned char *coded_data = (unsigned char *)malloc(
      (ae->channels > MAX_CHANNELS ? ae->channels : MAX_CHANNELS) *
      ae->frame_size * MAX_BITS_PER_SAMPLE);
  int ret = 0;
  ae->size_of_audio_frame_obu = 0;
  ae->size_of_parameter_demixing = 0;
  ae->size_of_parameter_recon_gain = 0;
  uint32_t num_samples_to_trim_at_start = frame->num_samples_to_trim_at_start;
  uint32_t num_samples_to_trim_at_end = frame->num_samples_to_trim_at_end;
  if (num_samples_to_trim_at_start == 0) {
    if (ae->padding > 0) {
      if (ae->padding < ae->frame_size) {
        num_samples_to_trim_at_start = ae->padding;
        ae->padding = 0;
      } else {
        num_samples_to_trim_at_start = ae->frame_size;
        ae->padding -= ae->frame_size;
      }
    }
  }

  if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    int demix_mode = 0;
    ChannelBasedEnc *ce = &(ae->channel_based_enc);
    unsigned char *pcm = frame->pcm;
    int frame_size = frame->frame_size;
    unsigned char *data = ae->audio_frame_obu;
    if (pcm) {
      ae->initial_padding += frame->frame_size;
    } else {
      if (ae->initial_padding <= 0) {
        ret = -1;
        goto END_ENCODING;
      }
    }

    if (num_samples_to_trim_at_end == 0) {
      num_samples_to_trim_at_end = (ae->initial_padding < ae->frame_size)
                                       ? (ae->frame_size - ae->initial_padding)
                                       : 0;
    }
    ae->samples = ae->frame_size - num_samples_to_trim_at_end;

    if (ce->layout_in == IA_CHANNEL_LAYOUT_MONO) {
      pcm = (unsigned char *)malloc(ae->channels * frame_size *
                                    MAX_BITS_PER_SAMPLE);
      memset(pcm, 0x00, ae->channels * frame_size * MAX_BITS_PER_SAMPLE);
      if (frame->pcm)
        mono2stereo2(frame->pcm, pcm, ae->channels, frame_size,
                     ae->bits_per_sample, ae->sample_format);
    }

    float *pcm_f = (float *)malloc(ae->channels * frame_size * sizeof(float));
    memset(pcm_f, 0x00, ae->channels * frame_size * sizeof(float));
    if (pcm)
      interleaved2interleaved_pcm2float(pcm, pcm_f, ae->channels, frame_size,
                                        ae->bits_per_sample, ae->sample_format);

    int ret_size = 0;
    unsigned char meta_info[255];
    int putsize_recon_gain = 0, recon_gain_obu_size = 0;

    float *gain_down_out =
        (float *)malloc(ae->frame_size * ae->channels * MAX_BITS_PER_SAMPLE);
    memset(gain_down_out, 0x00,
           ae->frame_size * ae->channels * MAX_BITS_PER_SAMPLE);

    int pre_ch = 0;
    unsigned char *extract_pcm =
        (unsigned char *)malloc(ae->frame_size * 2 * MAX_BITS_PER_SAMPLE);
    memset(extract_pcm, 0x00, ae->frame_size * 2 * MAX_BITS_PER_SAMPLE);

    // write substream obu
    int sub_stream_obu_size = 0;
    if (ae->channel_groups > 1) {
      if (ce->recon_gain_flag == 1) {
        for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
          int layout = ce->channel_layout_map[i];
          if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
          for (int j = 0; j < MAX_CHANNELS; j++)
            ce->mdhr.scalablefactor[layout][j] = default_recon_gain[j];
          QueuePop(&(ce->queue_rg[layout]), ce->mdhr.scalablefactor[layout],
                   enc_get_layout_channel_count(layout));
        }

        // recon gain parameter block
        write_recon_gain_obu(ae);
      }
      uint8_t dmix_index = default_dmix_index, w_index = default_w_index;
      if (ce->recon_gain_flag == 1) {
        QueuePop(&(ce->queue_dm[QUEUE_RG]), &dmix_index, 1);
        QueuePop(&(ce->queue_wg[QUEUE_RG]), &w_index, 1);
      } else {
        QueuePop(&(ce->queue_dm[QUEUE_LD]), &dmix_index, 1);
        QueuePop(&(ce->queue_wg[QUEUE_LD]), &w_index, 1);
      }

      if (w_index > 0)
        demix_mode = dmix_index + 3;
      else
        demix_mode = dmix_index - 1;

      downmix2(ce->downmixer_enc, pcm_f, frame_size, dmix_index, w_index);
      gaindown(ce->downmixer_enc->downmix_s, ce->channel_layout_map,
               ce->gaindown_map, ce->mdhr.dmixgain_f, ae->frame_size);

      pre_ch = 0;
      int substreams = 0;
      for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
        int lay_out = ce->channel_layout_map[i];
        if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;

        plane2interleaved_float2float(
            ce->downmixer_enc->downmix_s[lay_out], gain_down_out,
            enc_get_layout_channel_count(lay_out) - pre_ch, ae->frame_size);

        reorder_channels2(ae->ia_core_encoder[i].enc_stream_map,
                          ae->ia_core_encoder[i].channel, ae->frame_size,
                          gain_down_out);

        int32_t encoded_size = 0;
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          if (j < ae->ia_core_encoder[i].coupled_stream_count) {
            extract_pcm_from_group2(gain_down_out, extract_pcm,
                                    ae->ia_core_encoder[i].channel, j * 2, 0,
                                    ae->frame_size, ae->bits_per_sample,
                                    ae->sample_format);
            encoded_size =
                ae->encode_frame(ae, i, j, 2, extract_pcm, coded_data);
          } else {
            extract_pcm_from_group2(
                gain_down_out, extract_pcm, ae->ia_core_encoder[i].channel,
                ae->ia_core_encoder[i].coupled_stream_count + j, 1,
                ae->frame_size, ae->bits_per_sample, ae->sample_format);
            encoded_size =
                ae->encode_frame(ae, i, j, 1, extract_pcm, coded_data);
          }
          // sub_stream_obu_size += iamf_write_obu_unit(coded_data,
          // encoded_size, data + recon_gain_obu_size + sub_stream_obu_size,
          // OBU_SUBSTREAM);
          if (encoded_size > 0)
            sub_stream_obu_size += write_audio_frame_obu(
                ae, coded_data, ae->audio_frame_obu + sub_stream_obu_size,
                encoded_size, substreams, num_samples_to_trim_at_start,
                num_samples_to_trim_at_end);
          substreams++;
        }
        pre_ch = enc_get_layout_channel_count(lay_out);
      }
      write_demixing_obu(ae, demix_mode);
    } else {
      int substreams = 0;
      int lay_out = ce->channel_layout_map[0];
      if (pcm)
        memcpy(gain_down_out, pcm,
               ae->bits_per_sample / 8 * enc_get_layout_channel_count(lay_out) *
                   frame_size);
      reorder_channels3(ae->ia_core_encoder[0].enc_stream_map,
                        ae->ia_core_encoder[0].channel, ae->frame_size,
                        gain_down_out, ae->bits_per_sample);
      int32_t encoded_size = 0;
      for (int j = 0; j < ae->ia_core_encoder[0].stream_count; j++) {
        if (j < ae->ia_core_encoder[0].coupled_stream_count) {
          extract_pcm_from_group3(
              gain_down_out, extract_pcm, ae->ia_core_encoder[0].channel, j * 2,
              0, ae->frame_size, ae->bits_per_sample, ae->sample_format);
          encoded_size = ae->encode_frame(ae, 0, j, 2, extract_pcm, coded_data);
        } else {
          extract_pcm_from_group3(
              gain_down_out, extract_pcm, ae->ia_core_encoder[0].channel,
              ae->ia_core_encoder[0].coupled_stream_count + j, 1,
              ae->frame_size, ae->bits_per_sample, ae->sample_format);
          encoded_size = ae->encode_frame(ae, 0, j, 1, extract_pcm, coded_data);
        }
        // sub_stream_obu_size += iamf_write_obu_unit(coded_data, encoded_size,
        // data + sub_stream_obu_size, OBU_SUBSTREAM);
        if (encoded_size > 0)
          sub_stream_obu_size += write_audio_frame_obu(
              ae, coded_data, ae->audio_frame_obu + sub_stream_obu_size,
              encoded_size, substreams, num_samples_to_trim_at_start,
              num_samples_to_trim_at_end);
        substreams++;
      }
    }
    ae->initial_padding -= ae->frame_size;
    ae->size_of_audio_frame_obu = sub_stream_obu_size;

    if (gain_down_out) free(gain_down_out);

    if (ce->layout_in == IA_CHANNEL_LAYOUT_MONO) {
      if (pcm) free(pcm);
    }
    if (pcm_f) {
      free(pcm_f);
    }
    if (extract_pcm) {
      free(extract_pcm);
    }

  } else if (ae->element_type == AUDIO_ELEMENT_SCENE_BASED) {
    unsigned char *extract_pcm =
        (unsigned char *)malloc(ae->frame_size * 2 * MAX_BITS_PER_SAMPLE);
    memset(extract_pcm, 0x00, ae->frame_size * 2 * MAX_BITS_PER_SAMPLE);

    // write substream obu
    int sub_stream_obu_size = 0;
    SceneBasedEnc *se = &(ae->scene_based_enc);
    unsigned char *pcm = frame->pcm;
    int frame_size = frame->frame_size;
    unsigned char *data = ae->audio_frame_obu;
    if (pcm) {
      ae->initial_padding += frame->frame_size;
    } else {
      if (ae->initial_padding <= 0) {
        ret = -1;
        goto END_ENCODING;
      }
    }

    if (num_samples_to_trim_at_end == 0) {
      num_samples_to_trim_at_end = (ae->initial_padding < ae->frame_size)
                                       ? (ae->frame_size - ae->initial_padding)
                                       : 0;
    }
    ae->samples = ae->frame_size - num_samples_to_trim_at_end;

    int substreams = 0;
    int32_t encoded_size = 0;
    for (int j = 0; j < ae->ia_core_encoder[0].stream_count; j++) {
      if (j < ae->ia_core_encoder[0].coupled_stream_count) {
        if (pcm)
          extract_pcm_from_group3(
              pcm, extract_pcm, ae->ia_core_encoder[0].channel, j * 2, 0,
              ae->frame_size, ae->bits_per_sample, ae->sample_format);
        encoded_size = ae->encode_frame(ae, 0, j, 2, extract_pcm, coded_data);
      } else {
        if (pcm)
          extract_pcm_from_group3(
              pcm, extract_pcm, ae->ia_core_encoder[0].channel,
              ae->ia_core_encoder[0].coupled_stream_count + j, 1,
              ae->frame_size, ae->bits_per_sample, ae->sample_format);
        encoded_size = ae->encode_frame(ae, 0, j, 1, extract_pcm, coded_data);
      }
      // sub_stream_obu_size += iamf_write_obu_unit(coded_data, encoded_size,
      // data + sub_stream_obu_size, OBU_SUBSTREAM);
      if (encoded_size > 0)
        sub_stream_obu_size += write_audio_frame_obu(
            ae, coded_data, ae->audio_frame_obu + sub_stream_obu_size,
            encoded_size, substreams, num_samples_to_trim_at_start,
            num_samples_to_trim_at_end);
      substreams++;
    }
    ae->initial_padding -= ae->frame_size;
    ae->size_of_audio_frame_obu = sub_stream_obu_size;

    if (extract_pcm) {
      free(extract_pcm);
    }
  }

END_ENCODING:
  if (coded_data) free(coded_data);
  return ret;
}

static int write_temporal_delimiter_obu(IAMF_Encoder *ie, unsigned char *dst) {
  return iamf_write_obu_unit(NULL, 0, OBU_IA_Temporal_Delimiter, 0, 0, 0, 0, 0,
                             dst);
}

static int write_magic_code_obu(IAMF_Encoder *ie, unsigned char *dst) {
  unsigned char bitstr[255] = {
      0,
  };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));
  bs_setbits(&bs, ie->descriptor_config.magic_code.ia_code, 32);  // ia_code
  bs_setbits(&bs, ie->descriptor_config.magic_code.version, 8);   // version
  bs_setbits(&bs, ie->descriptor_config.magic_code.profile_version,
             8);  // profile_version
  return iamf_write_obu_unit(
      bitstr, bs.m_posBase, OBU_IA_Magic_Code,
      ie->descriptor_config.magic_code.obu_redundant_copy, 0, 0, 0, 0, dst);
}

static int write_codec_config_obu(IAMF_Encoder *ie, unsigned char *dst) {
  unsigned char bitstr[1024] = {
      0,
  };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));
  bs_setbits_leb128(
      &bs,
      ie->descriptor_config.codec_config.codec_config_id);  // codec_config_id

  // codec_config
  bs_setbits(&bs, ie->descriptor_config.codec_config.codec_id, 32);  // codec_id
  bs_setbits_leb128(&bs, ie->descriptor_config.codec_config
                             .num_samples_per_frame);  // num_samples_per_frame
  bs_setbits(&bs, ie->descriptor_config.codec_config.roll_distance,
             16);  // roll_distance
  for (int i = 0; i < ie->descriptor_config.codec_config.size_of_decoder_config;
       i++) {
    bs_setbits(&bs, ie->descriptor_config.codec_config.decoder_config[i],
               8);  // codec_config();
  }

  return iamf_write_obu_unit(
      bitstr, bs.m_posBase, OBU_IA_Codec_Config,
      ie->descriptor_config.codec_config.obu_redundant_copy, 0, 0, 0, 0, dst);
}

static int write_audio_elements_obu(IAMF_Encoder *ie, unsigned char *dst) {
  int obu_size = 0;
  for (int i = 0; i < ie->descriptor_config.num_audio_elements; i++) {
    AudioElement *audio_element = &(ie->descriptor_config.audio_element[i]);
    unsigned char bitstr[512] = {
        0,
    };
    bitstream_t bs;
    bs_init(&bs, bitstr, sizeof(bitstr));
    bs_setbits_leb128(&bs,
                      audio_element->audio_element_id);  // audio_element_id
    bs_setbits(&bs, audio_element->audio_element_type,
               3);          // audio_element_type
    bs_setbits(&bs, 0, 5);  // reserved

    bs_setbits_leb128(&bs, audio_element->codec_config_id);  // codec_config_id

    bs_setbits_leb128(&bs, audio_element->num_substreams);  // num_substreams
    for (int j = 0; j < audio_element->num_substreams; j++) {
      bs_setbits_leb128(
          &bs, audio_element->audio_substream_id[j]);  // audio_substream_id
    }

    bs_setbits_leb128(&bs, audio_element->num_parameters);  // num_parameters
    for (int j = 0; j < audio_element->num_parameters; j++) {
      bs_setbits_leb128(
          &bs,
          audio_element->param_definition_type[j]);  // param_definition_type
      bs_setbits_leb128(
          &bs,
          audio_element->param_definition[j].parameter_id);  // parameter_id
      bs_setbits_leb128(
          &bs,
          audio_element->param_definition[j].parameter_rate);  // parameter_rate
      bs_setbits(&bs, audio_element->param_definition[j].param_definition_mode,
                 1);          // reserved
      bs_setbits(&bs, 0, 7);  // reserved
      bs_setbits_leb128(
          &bs, audio_element->param_definition[j].duration);  // duration
      bs_setbits_leb128(
          &bs,
          audio_element->param_definition[j].num_segments);  // num_segments
      bs_setbits_leb128(&bs, audio_element->param_definition[j]
                                 .constant_segment_interval);  // num_segments
    }

    if (audio_element->audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
      bs_setbits(&bs, audio_element->scalable_channel_layout_config.num_layers,
                 3);          // num_layers;
      bs_setbits(&bs, 0, 5);  // reserved

      for (int j = 0;
           j < audio_element->scalable_channel_layout_config.num_layers; j++) {
        bs_setbits(&bs,
                   audio_element->scalable_channel_layout_config
                       .channel_audio_layer_config[j]
                       .loudspeaker_layout,
                   4);
        bs_setbits(&bs,
                   audio_element->scalable_channel_layout_config
                       .channel_audio_layer_config[j]
                       .output_gain_is_present_flag,
                   1);
        bs_setbits(&bs,
                   audio_element->scalable_channel_layout_config
                       .channel_audio_layer_config[j]
                       .recon_gain_is_present_flag,
                   1);
        bs_setbits(&bs, 0, 2);
        bs_setbits(&bs,
                   audio_element->scalable_channel_layout_config
                       .channel_audio_layer_config[j]
                       .substream_count,
                   8);
        bs_setbits(&bs,
                   audio_element->scalable_channel_layout_config
                       .channel_audio_layer_config[j]
                       .coupled_substream_count,
                   8);
        /*
        bs_setbits(&bs,
        audio_element->scalable_channel_layout_config.channel_audio_layer_config[j].loudness.info_type,
        8); bs_setbits(&bs,
        audio_element->scalable_channel_layout_config.channel_audio_layer_config[j].loudness.integrated_loudness,
        16); bs_setbits(&bs,
        audio_element->scalable_channel_layout_config.channel_audio_layer_config[j].loudness.digital_peak,
        16);
        if(audio_element->scalable_channel_layout_config.channel_audio_layer_config[j].loudness.info_type
        & 1) bs_setbits(&bs,
        audio_element->scalable_channel_layout_config.channel_audio_layer_config[j].loudness.true_peak,
        16);
        */
        if (audio_element->scalable_channel_layout_config
                .channel_audio_layer_config[j]
                .output_gain_is_present_flag) {
          bs_setbits(&bs,
                     audio_element->scalable_channel_layout_config
                         .channel_audio_layer_config[j]
                         .output_gain_flags,
                     6);
          bs_setbits(&bs, 0, 2);
          bs_setbits(&bs,
                     audio_element->scalable_channel_layout_config
                         .channel_audio_layer_config[j]
                         .output_gain,
                     16);
        }
      }
    } else if (audio_element->audio_element_type == AUDIO_ELEMENT_SCENE_BASED) {
      bs_setbits_leb128(
          &bs,
          audio_element->ambisonics_config.ambisonics_mode);  // ambisonics_mode
      if (audio_element->ambisonics_config.ambisonics_mode == AMBISONICS_MONO) {
        bs_setbits(&bs,
                   audio_element->ambisonics_config.ambisonics_mono_config
                       .output_channel_count,
                   8);
        bs_setbits(&bs,
                   audio_element->ambisonics_config.ambisonics_mono_config
                       .substream_count,
                   8);
        for (int i = 0; i < audio_element->ambisonics_config
                                .ambisonics_mono_config.output_channel_count;
             i++) {
          bs_setbits(&bs,
                     audio_element->ambisonics_config.ambisonics_mono_config
                         .channel_mapping[i],
                     8);
        }
      } else if (audio_element->ambisonics_config.ambisonics_mode ==
                 AMBISONICS_PROJECTION) {
        bs_setbits(&bs,
                   audio_element->ambisonics_config.ambisonics_projection_config
                       .output_channel_count,
                   8);
        bs_setbits(&bs,
                   audio_element->ambisonics_config.ambisonics_projection_config
                       .substream_count,
                   8);
        bs_setbits(&bs,
                   audio_element->ambisonics_config.ambisonics_projection_config
                       .coupled_substream_count,
                   8);
        int size = audio_element->ambisonics_config.ambisonics_projection_config
                       .output_channel_count *
                   (audio_element->ambisonics_config
                        .ambisonics_projection_config.substream_count +
                    audio_element->ambisonics_config
                        .ambisonics_projection_config.coupled_substream_count);
        for (int i = 0; i < size; i++) {
          bs_setbits(&bs,
                     audio_element->ambisonics_config
                         .ambisonics_projection_config.demixing_matrix[i],
                     16);
        }
      }
    }

    obu_size += iamf_write_obu_unit(bitstr, bs.m_posBase, OBU_IA_Audio_Element,
                                    audio_element->obu_redundant_copy, 0, 0, 0,
                                    0, dst + obu_size);
  }

  AudioElementEncoder *ae = ie->audio_element_enc;
  while (ae) {
    ae->redundant_copy = 1;
    ae = ae->next;
  }
  return obu_size;
}

static cl_cast_to_ss[] = {-1,
                          SOUND_SYSTEM_A,
                          SOUND_SYSTEM_B,
                          SOUND_SYSTEM_C,
                          SOUND_SYSTEM_D,
                          SOUND_SYSTEM_I,
                          SOUND_SYSTEM_EXT_712,
                          SOUND_SYSTEM_J,
                          SOUND_SYSTEM_EXT_312};
static int write_mix_presentations_obu(IAMF_Encoder *ie, unsigned char *dst) {
  int obu_size = 0;
  unsigned char bitstr[1024] = {
      0,
  };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));
  int num_mix_presentations = ie->descriptor_config.num_mix_presentations;

  for (int num = 0; num < num_mix_presentations; num++) {
    bs_setbits_leb128(&bs, num);  // mix_presentation_id
    MixPresentation *mix_presentation =
        &(ie->descriptor_config.mix_presentation[num]);

    MixPresentationAnnotations *mpa =
        &(mix_presentation->mix_presentation_annotations);
    bs_setbits_string(&bs, mpa->mix_presentation_friendly_label);

    bs_setbits_leb128(&bs, mix_presentation->num_sub_mixes);  // num_sub_mixes

    bs_setbits_leb128(
        &bs, mix_presentation->num_audio_elements);  // num_audio_elements

    for (int i = 0; i < mix_presentation->num_audio_elements; i++) {
      int num_layouts = mix_presentation->num_layouts;
      bs_setbits_leb128(
          &bs, mix_presentation->audio_element_id[i]);  // audio_element_id
      bs_setbits_string(&bs, mpa->mix_presentation_friendly_label);

      AudioElementEncoder *ae = ie->audio_element_enc;
      while (ae) {
        if (ae->element_id == mix_presentation->audio_element_id[i]) break;
        ae = ae->next;
      }
      if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
        if (mix_presentation->num_audio_elements == 1) {
          for (int cl = 0; cl < IA_CHANNEL_LAYOUT_COUNT; cl++) {
            int layout = ae->channel_based_enc.loudgain->channel_layout_map[cl];
            if (layout == IA_CHANNEL_LAYOUT_COUNT) break;
            if (layout == IA_CHANNEL_LAYOUT_BINAURAL) {
              mix_presentation->loudness_layout[num_layouts].layout_type =
                  IAMF_LAYOUT_TYPE_BINAURAL;
            } else {
              mix_presentation->loudness_layout[num_layouts].layout_type =
                  IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
              mix_presentation->loudness_layout[num_layouts].sound_system =
                  cl_cast_to_ss[layout];
            }

            mix_presentation->loudness[num_layouts].info_type = 0;
            mix_presentation->loudness[num_layouts].integrated_loudness =
                ae->channel_based_enc.mdhr.LKFSch[layout];
            mix_presentation->loudness[num_layouts].digital_peak =
                ae->channel_based_enc.mdhr.digital_peak[layout];
            mix_presentation->loudness[num_layouts].true_peak =
                ae->channel_based_enc.mdhr.true_peak[layout];
            num_layouts++;
          }
        }
        mix_presentation->num_layouts = num_layouts;
        // rendering_config(); has an empty payload.
        // bs_setbits(&bs, 0, 1);
        // bs_setbits(&bs, 0, 1);
        // bs_setbits(&bs, 0, 1);
        // bs_setbits(&bs, 0, 1);
        // bs_setbits(&bs, 0, 4);
      } else if (ae->element_type == AUDIO_ELEMENT_SCENE_BASED) {
        // itur_bs2127_hoa_config() has an empty payload.
      }
      int parameter_id = insert_obu_node(ie->root_node, OBU_IA_Parameter_Block,
                                         mix_presentation->audio_element_id[i]);
      ie->descriptor_config.element_mix_gain_para[num][i].parameter_id =
          parameter_id;
      ie->descriptor_config.element_mix_gain_para[num][i].parameter_rate =
          ie->input_sample_rate;
      ie->descriptor_config.element_mix_gain_para[num][i]
          .param_definition_mode = 1;
      bs_setbits_leb128(&bs, ie->descriptor_config.element_mix_gain_para[num][i]
                                 .parameter_id);  // parameter_id
      bs_setbits_leb128(&bs, ie->descriptor_config.element_mix_gain_para[num][i]
                                 .parameter_rate);  // parameter_rate
      bs_setbits(&bs,
                 ie->descriptor_config.element_mix_gain_para[num][i]
                     .param_definition_mode,
                 1);          // param_definition_mode
      bs_setbits(&bs, 0, 7);  // reserved
      bs_setbits(
          &bs,
          float_to_q(mix_presentation->element_mix_config[i].default_mix_gain,
                     8),
          16);
    }

    int parameter_id = insert_obu_node(ie->root_node, OBU_IA_Parameter_Block,
                                       mix_presentation->audio_element_id[0]);
    ie->descriptor_config.output_mix_gain_para[num].parameter_id = parameter_id;
    ie->descriptor_config.output_mix_gain_para[num].parameter_rate =
        ie->input_sample_rate;
    ie->descriptor_config.output_mix_gain_para[num].param_definition_mode = 1;
    bs_setbits_leb128(&bs, ie->descriptor_config.output_mix_gain_para[num]
                               .parameter_id);  // parameter_id
    bs_setbits_leb128(&bs, ie->descriptor_config.output_mix_gain_para[num]
                               .parameter_rate);  // parameter_rate
    bs_setbits(
        &bs,
        ie->descriptor_config.output_mix_gain_para[num].param_definition_mode,
        1);                 // param_definition_mode
    bs_setbits(&bs, 0, 7);  // reserved
    bs_setbits(
        &bs,
        float_to_q(mix_presentation->output_mix_config.default_mix_gain, 8),
        16);

    // loudness for measured layout
    bs_setbits_leb128(&bs, mix_presentation->num_layouts);  // num_layouts
    for (int i = 0; i < mix_presentation->num_layouts; i++) {
      bs_setbits(&bs, mix_presentation->loudness_layout[i].layout_type, 2);  //
      if (mix_presentation->loudness_layout[i].layout_type ==
          IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
        bs_setbits(&bs, mix_presentation->loudness_layout[i].num_loudspeakers,
                   6);
        for (int i = 0;
             i < mix_presentation->loudness_layout[i].num_loudspeakers; i++) {
          bs_setbits(&bs, mix_presentation->loudness_layout[i].sp_label[i], 8);
        }
      } else if (mix_presentation->loudness_layout[i].layout_type ==
                 IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
        bs_setbits(&bs, mix_presentation->loudness_layout[i].sound_system, 4);
        bs_setbits(&bs, 0, 2);
      } else if (mix_presentation->loudness_layout[i].layout_type ==
                 IAMF_LAYOUT_TYPE_BINAURAL) {
        bs_setbits(&bs, 6, 2);  // reserved
      } else if (mix_presentation->loudness_layout[i].layout_type ==
                 IAMF_LAYOUT_TYPE_NOT_DEFINED) {
        bs_setbits(&bs, 6, 2);  // reserved
      }
      // loudness_info
      bs_setbits(&bs, mix_presentation->loudness[i].info_type, 8);
      bs_setbits(&bs, mix_presentation->loudness[i].integrated_loudness, 16);
      bs_setbits(&bs, mix_presentation->loudness[i].digital_peak, 16);
      if (mix_presentation->loudness[i].info_type & 1) {
        bs_setbits(&bs, mix_presentation->loudness[i].true_peak, 16);
      }
    }
  }

  obu_size += iamf_write_obu_unit(bitstr, bs.m_posBase, OBU_IA_Mix_Presentation,
                                  0, 0, 0, 0, 0, dst + obu_size);
  return obu_size;
}

static int write_sync_obu(IAMF_Encoder *ie, unsigned char *dst) {
  int obu_size = 0;
  unsigned char bitstr[512] = {
      0,
  };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));

  bs_setbits_leb128(&bs, ie->sync_syntax.global_offset);  // global_offset
  bs_setbits_leb128(&bs, ie->sync_syntax.num_obu_ids);    // num_obu_ids
  for (int i = 0; i < ie->sync_syntax.num_obu_ids; i++) {
    bs_setbits_leb128(&bs, ie->sync_syntax.obu_id[i]);     // obu_id
    bs_setbits(&bs, ie->sync_syntax.obu_data_type[i], 1);  // obu_data_type
    bs_setbits(&bs, ie->sync_syntax.reinitialize_decoder[i],
               1);  // reinitialize_decoder
    bs_setbits(&bs, 0, 6);
    bs_setbits_leb128(&bs,
                      ie->sync_syntax.relative_offset[i]);  // relative_offset
  }
  // bs_setbits_leb128(&bs, ie->sync_syntax.concatenation_rule);
  // //concatenation_rule

  obu_size += iamf_write_obu_unit(bitstr, bs.m_posBase, OBU_IA_Sync, 0, 0, 0, 0,
                                  0, dst + obu_size);
  return obu_size;
}

static int obu_packets_mix(IAMF_Encoder *ie, AudioElementEncoder *ae,
                           IAPacket *iapkt) {
  if (ie->is_standalone) {
    if (ie->is_descriptor_changed) {
      update_ia_descriptor(ie);
      update_ia_sync(ie);
      // write descriptor OBU
      iapkt->packet_size +=
          write_magic_code_obu(ie, iapkt->data + iapkt->packet_size);
      iapkt->packet_size +=
          write_codec_config_obu(ie, iapkt->data + iapkt->packet_size);
      iapkt->packet_size +=
          write_audio_elements_obu(ie, iapkt->data + iapkt->packet_size);
      iapkt->packet_size +=
          write_mix_presentations_obu(ie, iapkt->data + iapkt->packet_size);
    }
    if (ie->need_place_sync) {
      iapkt->packet_size +=
          write_sync_obu(ie, iapkt->data + iapkt->packet_size);
      ie->need_place_sync = 0;
      ie->sync_syntax.num_obu_ids = 0;
    }
  }
  if (!ae->size_of_audio_frame_obu) return 0;
  if (ie->is_standalone) {
    if (ie->descriptor_config.magic_code.profile_version <=
        16)  // add Temporal Delimiter OBU
    {
      iapkt->packet_size +=
          write_temporal_delimiter_obu(ie, iapkt->data + iapkt->packet_size);
    }

    if (ae->size_of_parameter_demixing > 0 &&
        ae->parameter_demixing_obu)  // standalone case, exsist
    {
      memcpy(iapkt->data + iapkt->packet_size, ae->parameter_demixing_obu,
             ae->size_of_parameter_demixing);
      iapkt->packet_size += ae->size_of_parameter_demixing;
    }
  } else {
    if (ae->size_of_parameter_demixing > 0 &&
        ae->parameter_demixing_obu)  // mp4 case
    {
      memcpy(iapkt->data + iapkt->packet_size, ae->parameter_demixing_obu,
             ae->size_of_parameter_demixing);
      iapkt->packet_size += ae->size_of_parameter_demixing;
    }
  }

  if (ae->size_of_parameter_recon_gain > 0 && ae->parameter_recon_gain_obu) {
    memcpy(iapkt->data + iapkt->packet_size, ae->parameter_recon_gain_obu,
           ae->size_of_parameter_recon_gain);
    iapkt->packet_size += ae->size_of_parameter_recon_gain;
  }
  if (ae->size_of_audio_frame_obu > 0 && ae->audio_frame_obu) {
    memcpy(iapkt->data + iapkt->packet_size, ae->audio_frame_obu,
           ae->size_of_audio_frame_obu);
    iapkt->packet_size += ae->size_of_audio_frame_obu;
  }

  iapkt->samples = ae->samples;
  return 0;
}

int IAMF_encoder_encode(IAMF_Encoder *ie, const IAFrame *frame, IAPacket *iapkt,
                        int32_t max_data_bytes) {
  int ret = -1;
  iapkt->packet_size = 0;
  IAFrame *inframe = frame;
  AudioElementEncoder *ae = ie->audio_element_enc;
  while (inframe) {
    ae = ie->audio_element_enc;
    while (ae) {
      if (ae->element_id == inframe->element_id) break;
      ae = ae->next;
    }
    if (audio_element_encode(ae, inframe) == 0) ret = 0;

    obu_packets_mix(ie, ae, iapkt);
    inframe = inframe->next;
  }
  return ret;
}

static void channel_based_ia_encoder_close(ChannelBasedEnc *ce) {
  downmix_destroy(ce->downmixer_ld);
  downmix_destroy(ce->downmixer_rg);
  downmix_destroy(ce->downmixer_enc);
  immersive_audio_encoder_loudgain_destory(ce->loudgain);
  upmix_destroy(ce->upmixer);
  scalablefactor_destroy(ce->sf);

#ifdef INTER_FILE_DUMP
  ia_intermediate_file_readclose(ce, FILE_ENCODED, "ALL");
  ia_intermediate_file_readclose(ce, FILE_SCALEFACTOR, "ALL");
#endif

  for (int i = 0; i < QUEUE_STEP_MAX; i++) {
    QueueDestroy(&(ce->queue_dm[i]));
    QueueDestroy(&(ce->queue_wg[i]));
  }

  for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
    int lay_out = ce->channel_layout_map[i];
    if (lay_out == IA_CHANNEL_LAYOUT_COUNT) break;
    QueueDestroy(&(ce->queue_r[lay_out]));
    QueueDestroy(&(ce->queue_m[lay_out]));
    QueueDestroy(&(ce->queue_d[lay_out]));
    QueueDestroy(&(ce->queue_r[lay_out]));
    QueueDestroy(&(ce->queue_rg[lay_out]));
  }

#ifdef INTER_FILE_DUMP
  ia_intermediate_file_writeclose(ce, FILE_DOWNMIX_M, "ALL");
  ia_intermediate_file_writeclose(ce, FILE_DOWNMIX_S, "ALL");
  ia_intermediate_file_writeclose(ce, FILE_GAIN_DOWN, "ALL");
  ia_intermediate_file_writeclose(ce, FILE_UPMIX, "ALL");
  ia_intermediate_file_writeclose(ce, FILE_DECODED, "ALL");
#endif
}

static void scene_based_ia_encoder_close(SceneBasedEnc *se) {
  // TODO
  return;
}

/*
static void free_audio_element(AudioElementEncoder *ae)
{
  if (ae->next)
  {
    free_audio_element(ae->next);
    ae->encode_close(ae);
    if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED)
    {
      ae->encode_close2(ae);
      ae->decode_close(ae);
    }
    free(ae->next);
  }
}
*/

int IAMF_encoder_get_descriptor(IAMF_Encoder *ie, uint8_t *data, int max_size) {
#if 0
  update_ia_descriptor(ie);
  update_ia_sync(ie);
  int size = 0;
  unsigned char bitstr[2048] = { 0, };
  bitstream_t bs;

  size += write_magic_code_obu(ie, bitstr + size);
  size += write_codec_config_obu(ie, bitstr + size);

  bs_init(&bs, (bitstr + size), sizeof(bitstr));
  size += bs_setbits_leb128(&bs, ie->descriptor_config.num_audio_elements);
  size += write_audio_elements_obu(ie, bitstr + size);
  bs_init(&bs, (bitstr + size), sizeof(bitstr));
  size += bs_setbits_leb128(&bs, ie->descriptor_config.num_mix_presentations);
  size += write_mix_presentations_obu(ie, bitstr + size);
  if (size > max_size)
    return -1;
  else
    return size;
#else
  update_ia_descriptor(ie);
  update_ia_sync(ie);
  int size = 0;
  unsigned char bitstr[2048] = {
      0,
  };
  bitstream_t bs;

  size += write_magic_code_obu(ie, bitstr + size);
  size += write_codec_config_obu(ie, bitstr + size);
  size += write_audio_elements_obu(ie, bitstr + size);
  size += write_mix_presentations_obu(ie, bitstr + size);
  size += write_sync_obu(ie, bitstr + size);
  ie->need_place_sync = 0;
  ie->sync_syntax.num_obu_ids = 0;
  if (size > max_size)
    return -1;
  else {
    memcpy(data, bitstr, size);
    return size;
  }

#endif
}

void IAMF_encoder_destroy(IAMF_Encoder *ie) {
  AudioElementEncoder *ae = ie->audio_element_enc;
  AudioElementEncoder *ae_list[100];
  int list_size = 0;
  while (ae) {
    ae_list[list_size++] = ae;
    if (ae->element_type == AUDIO_ELEMENT_CHANNEL_BASED)
      channel_based_ia_encoder_close(&(ae->channel_based_enc));
    else if (ae->element_type == AUDIO_ELEMENT_SCENE_BASED)
      scene_based_ia_encoder_close(&(ae->scene_based_enc));
    ae = ae->next;
  }

  for (int i = 0; i < list_size; i++) {
    AudioElementEncoder *ae_free = ae_list[i];
    ae_free->encode_close(ae_free);
    if (ae_free->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
      ae_free->encode_close2(ae_free);
      ae_free->decode_close(ae_free);
    }
    if (ae_free->audio_element_obu) {
      free(ae_free->audio_element_obu);
      ae_free->audio_element_obu = NULL;
    }
    if (ae_free->audio_frame_obu) {
      free(ae_free->audio_frame_obu);
      ae_free->audio_frame_obu = NULL;
    }
    if (ae_free->parameter_demixing_obu) {
      free(ae_free->parameter_demixing_obu);
      ae_free->parameter_demixing_obu = NULL;
    }
    if (ae_free->parameter_recon_gain_obu) {
      free(ae_free->parameter_recon_gain_obu);
      ae_free->parameter_recon_gain_obu = NULL;
    }
    free(ae_free);
  }

  delete_obu_node(ie->root_node, OBU_IA_Invalid, -1);

  free(ie);
}
