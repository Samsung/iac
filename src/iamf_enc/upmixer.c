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
 * @file upmixer.c
 * @brief upmix the audio to generate target layout
 * @version 0.1
 * @date Created 3/3/2023
 **/

#include "upmixer.h"
#define _USE_MATH_DEFINES
#include "fixedp11_5.h"
#include "math.h"

static union trans2char {
  float f;
  unsigned char c[4];
};

static float DmixTypeMat[][4] = {{1.0f, 1.0f, 0.707f, 0.707f},      // type1
                                 {0.707f, 0.707f, 0.707f, 0.707f},  // type2
                                 {1.0f, 0.866f, 0.866f, 0.866f}};   // type3

static float calc_w_v2(int weighttypenum, float w_x_prev, float *w_x) {
  // weighttypenum == 0: 0,  weighttypenum != 0: 1.0 ???
  if (weighttypenum)
    *w_x = fminf(w_x_prev + 0.1, 1.0);
  else
    *w_x = fmaxf(w_x_prev - 0.1, 0.0);

  float w_y, w_z;
  float factor = (float)(1) / 3;

  if (*w_x <= 1.0) {
    if (*w_x - 0.5 < 0)
      w_y = pow(((0.5 - *w_x) / 4), factor) * (-1) + 0.5;
    else
      w_y = pow(((*w_x - 0.5) / 4), factor) + 0.5;
  } else {
    if (*w_x - 0.5 - 1 < 0)
      w_y = pow(((0.5 + 1 - *w_x) / 4), factor) * (-1) + 0.5;
    else
      w_y = pow(((*w_x - 0.5 - 1) / 4), factor) + 0.5 + 1;
  }
  w_z = w_y * 0.5;
  return (w_z);
}

void conv_upmixpcm(unsigned char *pcmbuf, void *dspbuf[], int nch, int shift,
                   int frame_size) {
  int16_t *buff = (int16_t *)pcmbuf;

  float **outbuff = (float **)dspbuf;
  for (int i = 0; i < nch; i++) {
    for (int j = 0; j < frame_size; j++) {
      outbuff[i + shift][j] =
          (float)(buff[i + j * nch]) / 32768.0f;  /// why / 32768.0f??
    }
  }
}

#if 0
static void conv_writtenfloat_(float pcmbuf[][FRAME_SIZE], void *wavbuf, int nch, int shift)
{
  unsigned char *wbuf = (unsigned char *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      union trans2char trans;
      trans.f = pcmbuf[i + shift][j];
      wbuf[(i + j*nch) * 4] = trans.c[0];
      wbuf[(i + j*nch) * 4 + 1] = trans.c[1];
      wbuf[(i + j*nch) * 4 + 2] = trans.c[2];
      wbuf[(i + j*nch) * 4 + 3] = trans.c[3];
    }
  }
}


void upmix_gain(UpMixer *um, int count, int *in, int channel_layout)
{

  Mdhr current = um->mdhr_c, last = um->mdhr_l;

  float last_dmix_gain = qf_to_float(last.dmixgain[channel_layout], 8);
  float dmix_gain = qf_to_float(current.dmixgain[channel_layout], 8);
  int ich;
  for (int c = 0; c < count; ++c) 
  {
    ich = in[c];

    for (int i = 0; i < FRAME_SIZE; i++)
    {
      if (i < PRESKIP_SIZE)
      {
        um->ch_data[ich][i] = um->ch_data[ich][i] / last_dmix_gain;
      }
      if (i >= PRESKIP_SIZE)
      {
        um->ch_data[ich][i] = um->ch_data[ich][i] / dmix_gain;
      }
    }
  }
}
#endif

static int upmix_s1to2(UpMixer *um, int *w_x) {
  for (int i = 0; i < um->frame_size; i++) {
    um->buffer[enc_channel_mixed_s2_l][i] = um->ch_data[enc_channel_l2][i];
    um->buffer[enc_channel_mixed_s2_r][i] =
        (um->ch_data[enc_channel_mono][i] -
         0.5 * um->ch_data[enc_channel_l2][i]) *
        2.0;
  }
  um->ch_data[enc_channel_l2] = um->buffer[enc_channel_mixed_s2_l];
  um->ch_data[enc_channel_r2] = um->buffer[enc_channel_mixed_s2_r];
  um->scalable_map[CHANNEL_LAYOUT_200][enc_channel_r2] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_100][enc_channel_mono] = 1;

  return 0;
}

static int upmix_s2to3(UpMixer *um, int *w_x) {
  for (int i = 0; i < um->frame_size; i++) {
    um->buffer[enc_channel_mixed_s3_l][i] =
        um->ch_data[enc_channel_l2][i] - 0.707 * um->ch_data[enc_channel_c][i];
    um->buffer[enc_channel_mixed_s3_r][i] =
        um->ch_data[enc_channel_r2][i] - 0.707 * um->ch_data[enc_channel_c][i];
  }
  um->ch_data[enc_channel_l3] = um->buffer[enc_channel_mixed_s3_l];
  um->ch_data[enc_channel_r3] = um->buffer[enc_channel_mixed_s3_r];

  um->scalable_map[CHANNEL_LAYOUT_312][enc_channel_l3] = 1;
  um->scalable_map[CHANNEL_LAYOUT_312][enc_channel_r3] = 1;

  um->relevant_mixed_cl[CHANNEL_LAYOUT_200][enc_channel_l2] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_200][enc_channel_r2] = 1;

  return 0;
}

static int upmix_s2(UpMixer *um, int *w_x) { return upmix_s1to2(um, w_x); }

static int upmix_s3(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_r2]) upmix_s2(um, w_x);
  return upmix_s2to3(um, w_x);
}

static int upmix_s3to5(UpMixer *um, int *w_x) {
  Mdhr current = um->mdhr_c, last = um->mdhr_l;
  int last_Typeid = last.dmix_matrix_type - 1;
  int DmixTypeNum = current.dmix_matrix_type;
  int Typeid = DmixTypeNum - 1;

  for (int i = 0; i < um->frame_size; i++) {
    for (int i = 0; i < um->frame_size; i++) {
      if (i < um->preskip_size) {
        um->buffer[enc_channel_mixed_s5_l][i] =
            (um->ch_data[enc_channel_l3][i] - um->ch_data[enc_channel_l5][i]) /
            DmixTypeMat[last_Typeid][3];
        um->buffer[enc_channel_mixed_s5_r][i] =
            (um->ch_data[enc_channel_r3][i] - um->ch_data[enc_channel_r5][i]) /
            DmixTypeMat[last_Typeid][3];
      }
      if (i >= um->preskip_size) {
        um->buffer[enc_channel_mixed_s5_l][i] =
            (um->ch_data[enc_channel_l3][i] - um->ch_data[enc_channel_l5][i]) /
            DmixTypeMat[Typeid][3];
        um->buffer[enc_channel_mixed_s5_r][i] =
            (um->ch_data[enc_channel_r3][i] - um->ch_data[enc_channel_r5][i]) /
            DmixTypeMat[Typeid][3];
      }
    }
  }
  um->ch_data[enc_channel_sl5] = um->buffer[enc_channel_mixed_s5_l];
  um->ch_data[enc_channel_sr5] = um->buffer[enc_channel_mixed_s5_r];

  um->scalable_map[CHANNEL_LAYOUT_510][enc_channel_sl5] = 1;
  um->scalable_map[CHANNEL_LAYOUT_510][enc_channel_sr5] = 1;
  um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_sl5] = 1;
  um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_sr5] = 1;
  um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_sl5] = 1;
  um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_sr5] = 1;

  um->relevant_mixed_cl[CHANNEL_LAYOUT_312][enc_channel_l3] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_312][enc_channel_r3] = 1;

  return 0;
}

static int upmix_s5(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_l3]) upmix_s3(um, w_x);
  return upmix_s3to5(um, w_x);
}

static int upmix_s5to7(UpMixer *um, int *w_x) {
  Mdhr current = um->mdhr_c, last = um->mdhr_l;
  int last_Typeid = last.dmix_matrix_type - 1;
  int DmixTypeNum = current.dmix_matrix_type;
  int Typeid = DmixTypeNum - 1;

  for (int i = 0; i < um->frame_size; i++) {
    for (int i = 0; i < um->frame_size; i++) {
      if (i < um->preskip_size) {
        um->buffer[enc_channel_mixed_s7_l][i] =
            (um->ch_data[enc_channel_sl5][i] -
             um->ch_data[enc_channel_sl7][i] * DmixTypeMat[last_Typeid][0]) /
            DmixTypeMat[last_Typeid][1];
        um->buffer[enc_channel_mixed_s7_r][i] =
            (um->ch_data[enc_channel_sr5][i] -
             um->ch_data[enc_channel_sr7][i] * DmixTypeMat[last_Typeid][0]) /
            DmixTypeMat[last_Typeid][1];
      }
      if (i >= um->preskip_size) {
        um->buffer[enc_channel_mixed_s7_l][i] =
            (um->ch_data[enc_channel_sl5][i] -
             um->ch_data[enc_channel_sl7][i] * DmixTypeMat[Typeid][0]) /
            DmixTypeMat[Typeid][1];
        um->buffer[enc_channel_mixed_s7_r][i] =
            (um->ch_data[enc_channel_sr5][i] -
             um->ch_data[enc_channel_sr7][i] * DmixTypeMat[Typeid][0]) /
            DmixTypeMat[Typeid][1];
      }
    }
  }
  um->ch_data[enc_channel_bl7] = um->buffer[enc_channel_mixed_s7_l];
  um->ch_data[enc_channel_br7] = um->buffer[enc_channel_mixed_s7_r];

  um->scalable_map[CHANNEL_LAYOUT_710][enc_channel_bl7] = 1;
  um->scalable_map[CHANNEL_LAYOUT_710][enc_channel_br7] = 1;
  um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_bl7] = 1;
  um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_br7] = 1;
  um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_bl7] = 1;
  um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_br7] = 1;

  um->relevant_mixed_cl[CHANNEL_LAYOUT_510][enc_channel_sl5] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_510][enc_channel_sr5] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_512][enc_channel_sl5] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_512][enc_channel_sr5] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_514][enc_channel_sl5] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_514][enc_channel_sr5] = 1;

  return 0;
}

static int upmix_s7(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_sl5]) upmix_s5(um, w_x);
  return upmix_s5to7(um, w_x);
}

static int upmix_hf2toh2(UpMixer *um, int *w_x) {
  Mdhr current = um->mdhr_c, last = um->mdhr_l;
  int last_Typeid = last.dmix_matrix_type - 1;
  float last_w_z = 0.0, w_z = 0.0;
  if (um->default_demix_is_set)
    last_w_z = get_w(um->default_demix_weight);
  else
    last_w_z =
        calc_w(last.weight_type, um->last_weight_state_value_x_prev2_int, w_x);
  int DmixTypeNum = current.dmix_matrix_type;
  int WeightTypeNum = current.weight_type;
  int Typeid = DmixTypeNum - 1;
  if (um->default_demix_is_set)
    w_z = get_w(um->default_demix_weight);
  else
    w_z = calc_w(WeightTypeNum, um->last_weight_state_value_x_prev_int, w_x);

  for (int i = 0; i < um->frame_size; i++) {
    if (i < um->preskip_size)  // 0-311
    {
      um->buffer[enc_channel_mixed_h_l][i] =
          um->ch_data[enc_channel_tl][i] - DmixTypeMat[last_Typeid][3] *
                                               last_w_z *
                                               um->ch_data[enc_channel_sl5][i];
      um->buffer[enc_channel_mixed_h_r][i] =
          um->ch_data[enc_channel_tr][i] - DmixTypeMat[last_Typeid][3] *
                                               last_w_z *
                                               um->ch_data[enc_channel_sr5][i];
    }
    if (i >= um->preskip_size) {
      um->buffer[enc_channel_mixed_h_l][i] =
          um->ch_data[enc_channel_tl][i] -
          DmixTypeMat[Typeid][3] * w_z * um->ch_data[enc_channel_sl5][i];
      um->buffer[enc_channel_mixed_h_r][i] =
          um->ch_data[enc_channel_tr][i] -
          DmixTypeMat[Typeid][3] * w_z * um->ch_data[enc_channel_sr5][i];
    }
  }

  um->ch_data[enc_channel_hl] = um->buffer[enc_channel_mixed_h_l];
  um->ch_data[enc_channel_hr] = um->buffer[enc_channel_mixed_h_r];

  um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_hl] = 1;
  um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_hr] = 1;
  um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_hl] = 1;
  um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_hr] = 1;

  um->relevant_mixed_cl[CHANNEL_LAYOUT_312][enc_channel_tl] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_312][enc_channel_tr] = 1;

  return 0;
}

static int upmix_h2(UpMixer *um, int *w_x) { return upmix_hf2toh2(um, w_x); }

static int upmix_h2toh4(UpMixer *um, int *w_x) {
  Mdhr current = um->mdhr_c, last = um->mdhr_l;
  int last_Typeid = last.dmix_matrix_type - 1;
  int DmixTypeNum = current.dmix_matrix_type;
  int WeightTypeNum = current.weight_type;
  int Typeid = DmixTypeNum - 1;

  for (int i = 0; i < um->frame_size; i++) {
    if (i < um->preskip_size)  // 0-311
    {
      um->buffer[enc_channel_mixed_h_bl][i] =
          (um->ch_data[enc_channel_hl][i] - um->ch_data[enc_channel_hfl][i]) /
          DmixTypeMat[last_Typeid][2];
      um->buffer[enc_channel_mixed_h_br][i] =
          (um->ch_data[enc_channel_hr][i] - um->ch_data[enc_channel_hfr][i]) /
          DmixTypeMat[last_Typeid][2];
    }
    if (i >= um->preskip_size) {
      um->buffer[enc_channel_mixed_h_bl][i] =
          (um->ch_data[enc_channel_hl][i] - um->ch_data[enc_channel_hfl][i]) /
          DmixTypeMat[Typeid][2];
      um->buffer[enc_channel_mixed_h_br][i] =
          (um->ch_data[enc_channel_hr][i] - um->ch_data[enc_channel_hfr][i]) /
          DmixTypeMat[Typeid][2];
    }
  }

  um->ch_data[enc_channel_hbl] = um->buffer[enc_channel_mixed_h_bl];
  um->ch_data[enc_channel_hbr] = um->buffer[enc_channel_mixed_h_br];

  um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_hbl] = 1;
  um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_hbr] = 1;
  um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_hbl] = 1;
  um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_hbr] = 1;

  um->relevant_mixed_cl[CHANNEL_LAYOUT_512][enc_channel_hl] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_512][enc_channel_hr] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_712][enc_channel_hl] = 1;
  um->relevant_mixed_cl[CHANNEL_LAYOUT_712][enc_channel_hr] = 1;

  return 0;
}

static int upmix_h4(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_hl]) upmix_h2(um, w_x);
  return upmix_h2toh4(um, w_x);
}

void upmix_to100(UpMixer *um, int *w_x) { int in[] = {enc_channel_mono}; }

void upmix_to200(UpMixer *um, int *w_x) {
  int in[] = {enc_channel_l2, enc_channel_r2};
  if (!um->ch_data[enc_channel_r2]) upmix_s2(um, w_x);
  // int out[] = { enc_channel_mixed_s2_l, enc_channel_mixed_s2_r };

  // upmix_gain(um, 2, in, CHANNEL_LAYOUT_200);

  // um->ch_data[enc_channel_l2] = um->buffer[enc_channel_mixed_s2_l];
  // um->ch_data[enc_channel_r2] = um->buffer[enc_channel_mixed_s2_r];
}

void upmix_to312(UpMixer *um, int *w_x) {
  int in[] = {enc_channel_tl, enc_channel_tr};
  // int out[] = { enc_channel_mixed_h_l, enc_channel_mixed_h_r };

  // upmix_gain(um, 2, in, CHANNEL_LAYOUT_312);

  // um->ch_data[enc_channel_tl] = um->buffer[enc_channel_mixed_h_l];
  // um->ch_data[enc_channel_tr] = um->buffer[enc_channel_mixed_h_r];

  if (!um->ch_data[enc_channel_l3]) upmix_s3(um, w_x);
}

void upmix_to510(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_sl5]) upmix_s5(um, w_x);
}

void upmix_to512(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_sl5]) upmix_s5(um, w_x);
  if (!um->ch_data[enc_channel_hl]) upmix_h2(um, w_x);
}

void upmix_to514(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_sl5]) upmix_s5(um, w_x);
  if (!um->ch_data[enc_channel_hbl]) upmix_h4(um, w_x);
}

void upmix_to710(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_bl7]) upmix_s7(um, w_x);
}

void upmix_to712(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_bl7]) upmix_s7(um, w_x);
  if (!um->ch_data[enc_channel_hl]) upmix_h2(um, w_x);
}

void upmix_to714(UpMixer *um, int *w_x) {
  if (!um->ch_data[enc_channel_bl7]) upmix_s7(um, w_x);
  if (!um->ch_data[enc_channel_hbl]) upmix_h4(um, w_x);
}

void upmix_smooth(UpMixer *um, int layout, int count, int *channel) {
  float *filtBuf[MAX_CHANNELS];
  for (int i = 0; i < MAX_CHANNELS; i++) {
    filtBuf[i] = (float *)malloc(um->frame_size * sizeof(float));
    memset(filtBuf[i], 0x00, um->frame_size * sizeof(float));
  }

  float N = 7.0;
  int bitshift = 0;
  int mask = 0xff;
  int scaledata, scaleindex;
  float sf;
  float sfavg[12];
  int ch;
  float *out;
  for (int i = 0; i < count; i++) {
    scaledata = scaleindex = (um->mdhr_c.chsilence[layout] >> bitshift) & mask;
    sf = qf_to_float(scaledata, 8);

    if (N > 0) {
      sfavg[i] = (2.0 / (N + 1.0)) * sf +
                 (1.0 - 2.0 / (N + 1.0)) * um->last_sfavg[layout][i];
    } else {
      sfavg[i] = sf;
    }
    for (int j = 0; j < um->frame_size; j++) {
      filtBuf[i][j] = um->last_sfavg[layout][i] * um->stopWin[j] +
                      sfavg[i] * um->startWin[j];
    }
    um->last_sf[layout][i] = sf;
    um->last_sfavg[layout][i] = sfavg[i];
    bitshift = bitshift + 8;
  }
  if (um->recon_gain_flag == 1) {
    for (int i = 0; i < count; i++) {
      ch = channel[i];
      out = um->ch_data[ch];
      for (int j = 0; j < um->frame_size; j++) {
        out[j] = out[j] * filtBuf[i][j];
      }
    }
  }

  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (filtBuf[i]) {
      free(filtBuf[i]);
    }
  }
}

static int upmix_add_mixed_channels(UpMixer *um, int layout, int *array) {
  int cnt = 0;
  uint8_t *tchs = NULL;
  int nch = enc_get_layout_channel_count(layout);
  tchs = enc_get_layout_channels(layout);
  for (int i = 0; i < nch; i++) {
    int channel = tchs[i];
    if (um->scalable_map[layout][channel] == 1) array[cnt++] = channel;
  }

  return cnt;
}

static int upmix_add_mixed_s2_channels(UpMixer *um, int *array) {
  int cnt = 0;
  if (um->ch_data[enc_channel_mono] && um->ch_data[enc_channel_l2]) {
    array[cnt++] = enc_channel_r2;
    um->scalable_map[CHANNEL_LAYOUT_200][enc_channel_r2] = 1;
  }
  return cnt;
}

static int upmix_add_mixed_s3_channels(UpMixer *um, int *array) {
  int cnt = 0;
  if (um->ch_data[enc_channel_l2] && um->ch_data[enc_channel_l3]) {
    array[cnt++] = enc_channel_l3;
    array[cnt++] = enc_channel_r3;
    um->scalable_map[CHANNEL_LAYOUT_312][enc_channel_l3] = 1;
    um->scalable_map[CHANNEL_LAYOUT_312][enc_channel_r3] = 1;
  }
  return cnt;
}

static int upmix_add_mixed_s5_channels(UpMixer *um, int *array) {
  int cnt = 0;
  if (um->ch_data[enc_channel_l3] && um->ch_data[enc_channel_sl5]) {
    array[cnt++] = enc_channel_sl5;
    array[cnt++] = enc_channel_sr5;
    um->scalable_map[CHANNEL_LAYOUT_510][enc_channel_sl5] = 1;
    um->scalable_map[CHANNEL_LAYOUT_510][enc_channel_sr5] = 1;
    um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_sl5] = 1;
    um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_sr5] = 1;
    um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_sl5] = 1;
    um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_sr5] = 1;
  }
  return cnt;
}

static int upmix_add_mixed_s7_channels(UpMixer *um, int *array) {
  int cnt = 0;
  if (um->ch_data[enc_channel_sl5] && um->ch_data[enc_channel_bl7]) {
    array[cnt++] = enc_channel_bl7;
    array[cnt++] = enc_channel_br7;
    um->scalable_map[CHANNEL_LAYOUT_710][enc_channel_bl7] = 1;
    um->scalable_map[CHANNEL_LAYOUT_710][enc_channel_br7] = 1;
    um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_bl7] = 1;
    um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_br7] = 1;
    um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_bl7] = 1;
    um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_br7] = 1;
  }
  return cnt;
}

static int upmix_add_mixed_h2_channels(UpMixer *um, int *array) {
  int cnt = 0;
  if (um->ch_data[enc_channel_tl] && um->ch_data[enc_channel_hl]) {
    array[cnt++] = enc_channel_hl;
    array[cnt++] = enc_channel_hr;

    um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_hl] = 1;
    um->scalable_map[CHANNEL_LAYOUT_512][enc_channel_hr] = 1;
    um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_hl] = 1;
    um->scalable_map[CHANNEL_LAYOUT_712][enc_channel_hr] = 1;
  }
  return cnt;
}

static int upmix_add_mixed_h4_channels(UpMixer *um, int *array) {
  int cnt = 0;
  if (um->ch_data[enc_channel_hl] && um->ch_data[enc_channel_hbl]) {
    array[cnt++] = enc_channel_hbl;
    array[cnt++] = enc_channel_hbr;
    um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_hbl] = 1;
    um->scalable_map[CHANNEL_LAYOUT_514][enc_channel_hbr] = 1;
    um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_hbl] = 1;
    um->scalable_map[CHANNEL_LAYOUT_714][enc_channel_hbr] = 1;
  }
  return cnt;
}

void smooth_to200(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_200, mixch_list);
  // ret = upmix_add_mixed_s2_channels(um, mixch_list);
  upmix_smooth(um, CHANNEL_LAYOUT_200, ret, mixch_list);
}

void smooth_to312(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_312, mixch_list);
  // ret = upmix_add_mixed_s3_channels(um, mixch_list);
  upmix_smooth(um, CHANNEL_LAYOUT_312, ret, mixch_list);
}

void smooth_to510(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_510, mixch_list);
  // ret = upmix_add_mixed_s5_channels(um, mixch_list);
  upmix_smooth(um, CHANNEL_LAYOUT_510, ret, mixch_list);
}

void smooth_to512(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_512, mixch_list);
  // ret = upmix_add_mixed_s5_channels(um, mixch_list);
  // ret += upmix_add_mixed_h2_channels(um, &mixch_list[ret]);
  upmix_smooth(um, CHANNEL_LAYOUT_512, ret, mixch_list);
}

void smooth_to514(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_514, mixch_list);
  // ret = upmix_add_mixed_s5_channels(um, mixch_list);
  // ret += upmix_add_mixed_h4_channels(um, &mixch_list[ret]);
  upmix_smooth(um, CHANNEL_LAYOUT_514, ret, mixch_list);
}

void smooth_to710(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_710, mixch_list);
  // ret = upmix_add_mixed_s7_channels(um, mixch_list);
  upmix_smooth(um, CHANNEL_LAYOUT_710, ret, mixch_list);
}

void smooth_to712(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_712, mixch_list);
  // ret = upmix_add_mixed_s7_channels(um, mixch_list);
  // ret += upmix_add_mixed_h2_channels(um, &mixch_list[ret]);
  upmix_smooth(um, CHANNEL_LAYOUT_712, ret, mixch_list);
}

void smooth_to714(UpMixer *um) {
  int mixch_list[MAX_CHANNELS];
  int ret = 0;
  ret = upmix_add_mixed_channels(um, CHANNEL_LAYOUT_714, mixch_list);
  // ret = upmix_add_mixed_s7_channels(um, mixch_list);
  // ret += upmix_add_mixed_h4_channels(um, &mixch_list[ret]);
  upmix_smooth(um, CHANNEL_LAYOUT_714, ret, mixch_list);
}

typedef struct {
  int opcode;
  void *data;
} creator_t;

static creator_t g_upmix[] = {
    {CHANNEL_LAYOUT_100, upmix_to100}, {CHANNEL_LAYOUT_200, upmix_to200},
    {CHANNEL_LAYOUT_510, upmix_to510}, {CHANNEL_LAYOUT_512, upmix_to512},
    {CHANNEL_LAYOUT_514, upmix_to514}, {CHANNEL_LAYOUT_710, upmix_to710},
    {CHANNEL_LAYOUT_712, upmix_to712}, {CHANNEL_LAYOUT_714, upmix_to714},
    {CHANNEL_LAYOUT_312, upmix_to312}, {-1}};

static creator_t g_factorsmooth[] = {
    {CHANNEL_LAYOUT_100, NULL},         {CHANNEL_LAYOUT_200, smooth_to200},
    {CHANNEL_LAYOUT_510, smooth_to510}, {CHANNEL_LAYOUT_512, smooth_to512},
    {CHANNEL_LAYOUT_514, smooth_to514}, {CHANNEL_LAYOUT_710, smooth_to710},
    {CHANNEL_LAYOUT_712, smooth_to712}, {CHANNEL_LAYOUT_714, smooth_to714},
    {CHANNEL_LAYOUT_312, smooth_to312}, {-1}};

UpMixer *upmix_create(int recon_gain_flag,
                      const unsigned char *channel_layout_map, int frame_size) {
  UpMixer *um = (UpMixer *)malloc(sizeof(UpMixer));
  if (!um) return NULL;
  memset(um, 0x00, sizeof(UpMixer));
  um->recon_gain_flag = recon_gain_flag;
  um->pre_layout = CHANNEL_LAYOUT_MAX;
  memcpy(um->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_MAX);
  um->frame_size = frame_size;

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    um->upmix[layout] =
        (float *)malloc(um->frame_size * MAX_CHANNELS * sizeof(float));
    if (!um->upmix[layout]) goto FAILED;
    memset(um->upmix[layout], 0x00,
           um->frame_size * MAX_CHANNELS * sizeof(float));
  }

  um->hanning = (float *)malloc(um->frame_size / 8 * sizeof(float));
  um->startWin = (float *)malloc(um->frame_size * sizeof(float));
  um->stopWin = (float *)malloc(um->frame_size * sizeof(float));

  if (!um->hanning || !um->startWin | !um->stopWin) goto FAILED;

  int frameLen = um->frame_size / 8;
  float den = (float)(frameLen - 1);
  // create hanning window
  for (int i = 0; i < frameLen; i++) {
    um->hanning[i] = 0.5 * (1.0 - cos(2 * M_PI * i / den));
  }

  for (int i = 0; i < 12; i++) {
    um->last_sf1[i] = um->last_sfavg1[i] = um->last_sf2[i] =
        um->last_sfavg2[i] = um->last_sf3[i] = um->last_sfavg3[i] = 1;
  }

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    for (int j = 0; j < 12; j++) {
      um->last_sf[i][j] = um->last_sfavg[i][j] = 1;
    }
  }

  // init last and current mdhr

  // um->last_weight_state_value_x_prev = 0.0;
  // um->last_weight_state_value_x_prev2 = 0.0;

  um->last_weight_state_value_x_prev_int = 0;
  um->last_weight_state_value_x_prev2_int = 0;

  for (int i = 0; i < enc_channel_mixed_cnt; i++) {
    um->buffer[i] = (float *)malloc(um->frame_size * sizeof(float));
    if (!um->buffer[i]) goto FAILED;
    memset(um->buffer[i], 0x00, um->frame_size * sizeof(float));
  }

  int idx = 0, ret = 0;
  ;
  int last_cl_layout = CHANNEL_LAYOUT_INVALID;
  uint8_t new_channels[256];

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    ret = enc_get_new_channels2(last_cl_layout, layout, new_channels);

    for (int i = idx, j = 0; j < ret; ++i, ++j) {
      um->channel_order[i] = new_channels[j];
    }
    idx += ret;

    last_cl_layout = layout;
  }
  return um;
FAILED:
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    if (um->upmix[layout]) free(um->upmix[layout]);
  }
  if (um->hanning) free(um->hanning);
  if (um->startWin) free(um->startWin);
  if (um->stopWin) free(um->stopWin);
  for (int i = 0; i < enc_channel_mixed_cnt; i++) {
    if (um->buffer[i]) free(um->buffer[i]);
  }
  if (um) free(um);
  return NULL;
}

void upmix_destroy(UpMixer *um) {
  if (um) {
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
      if (um->upmix[i]) free(um->upmix[i]);
    }
    for (int i = 0; i < enc_channel_mixed_cnt; i++) {
      if (um->buffer[i]) free(um->buffer[i]);
    }
    if (um->hanning) free(um->hanning);
    if (um->startWin) free(um->startWin);
    if (um->stopWin) free(um->stopWin);
    free(um);
  }
}

void upmix_set_preskip_size(UpMixer *um, int preskip_size) {
  um->preskip_size = preskip_size;
  int frameLen = um->frame_size / 8;
  float den = (float)(frameLen - 1);
  // create hanning window
  for (int i = 0; i < frameLen; i++) {
    um->hanning[i] = 0.5 * (1.0 - cos(2 * M_PI * i / den));
  }
  int overlapLen = frameLen / 2;
#if 1
  for (int i = 0; i < um->frame_size; i++) {
    if (i < um->preskip_size) {
      um->startWin[i] = 0.0f;
      um->stopWin[i] = 1.0f;
    } else if (i >= (um->preskip_size) && i < um->preskip_size + overlapLen) {
      um->startWin[i] = um->hanning[i - um->preskip_size];
      um->stopWin[i] = um->hanning[i - um->preskip_size + overlapLen];
    } else {
      um->startWin[i] = 1.0f;
      um->stopWin[i] = 0.0f;
    }
  }
#else
  for (int i = 0; i < um->frame_size; i++) {
    if (i < um->preskip_size - overlapLen) {
      um->startWin[i] = 0.0f;
      um->stopWin[i] = 1.0f;
    } else if (i >= (um->preskip_size - overlapLen) && i < um->preskip_size) {
      um->startWin[i] = um->hanning[i - (um->preskip_size - overlapLen)];
      um->stopWin[i] = um->hanning[i - um->preskip_size + 2 * overlapLen];
    } else {
      um->startWin[i] = 1.0f;
      um->stopWin[i] = 0.0f;
    }
  }
#endif
}

void upmix_gain_up(UpMixer *um, float *pcmbuf[], int nch,
                   const unsigned char *gain_down_map) {
  Mdhr current = um->mdhr_c, last = um->mdhr_l;

  int loop = 0;
  int layout = um->channel_layout_map[loop];
  float last_dmix_gain = last.dmixgain_f[layout];
  float dmix_gain = current.dmixgain_f[layout];
  for (int i = 0; i < nch; i++) {
    if (i >= enc_get_layout_channel_count(layout)) {
      loop++;
      layout = um->channel_layout_map[loop];
      last_dmix_gain = last.dmixgain_f[layout];
      dmix_gain = current.dmixgain_f[layout];
    }
    if (gain_down_map[i]) {
      for (int j = 0; j < um->frame_size; j++) {
        if (j < um->preskip_size) {
          pcmbuf[i][j] = pcmbuf[i][j] / last_dmix_gain;
        }
        if (j >= um->preskip_size) {
          pcmbuf[i][j] = pcmbuf[i][j] / dmix_gain;
        }
      }
    }
  }
}

int upmix_set_default_demix(UpMixer *um, int demix_mode, int demix_weight) {
  if (demix_mode > -1) um->default_demix_mode = demix_mode;
  if (demix_weight > -1) um->default_demix_weight = demix_weight;
  um->default_demix_is_set = 1;
  return 0;
}

void upmix(UpMixer *um, const unsigned char *gain_down_map) {
  if (um->default_demix_is_set) {
    um->mdhr_c.dmix_matrix_type = (um->default_demix_mode % 4) + 1;  // 0~3, 4~5
  }
  // float weight_state_value_x_curr = 0.0;
  int weight_state_value_x_curr_int = 0;

  unsigned char pre_ch = 0;
  unsigned char start_ch = 0;
  int last_layout = 0;
  uint8_t *playout;
  int channel;
  float *dspInBuf[MAX_CHANNELS];
  for (int i = 0; i < MAX_CHANNELS; i++) {
    dspInBuf[i] = (float *)malloc(um->frame_size * sizeof(float));
    memset(dspInBuf[i], 0x00, um->frame_size * sizeof(float));
  }

  for (int i = 0; i < enc_channel_cnt; i++) {
    um->ch_data[i] = NULL;
  }

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    conv_upmixpcm(um->up_input[layout], dspInBuf,
                  enc_get_layout_channel_count(layout) - pre_ch, start_ch,
                  um->frame_size);
    start_ch += (enc_get_layout_channel_count(layout) - pre_ch);
    pre_ch = enc_get_layout_channel_count(layout);
    last_layout = layout;
  }

  int chs = enc_get_layout_channel_count(last_layout);
  upmix_gain_up(um, dspInBuf, chs, gain_down_map);
  for (int i = 0; i < chs; i++) {
    int channel = um->channel_order[i];
    um->ch_data[channel] = dspInBuf[i];
    // printf("%s (%d) \n", up_get_channel_name(channel), channel);
  }

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    if (g_upmix[layout].data) {
      ((void (*)(UpMixer * um, int *w_x)) g_upmix[layout].data)(
          um, &weight_state_value_x_curr_int);
    }
  }

#if 1
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    if (g_factorsmooth[layout].data) {
      ((void (*)(UpMixer * um)) g_factorsmooth[layout].data)(um);
    }
  }
#endif
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int layout = um->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX) break;
    playout = enc_get_layout_channels(layout);
    for (int ch = 0; ch < enc_get_layout_channel_count(layout); ++ch) {
      channel = playout[ch];
      if (!um->ch_data[channel]) {
        printf("channel %d doesn't has data.\n", playout[ch]);
        continue;
      }
      memcpy(&(um->upmix[layout][ch * um->frame_size]), um->ch_data[channel],
             sizeof(float) * um->frame_size);
    }
  }

  um->mdhr_l = um->mdhr_c;

  // um->last_weight_state_value_x_prev2 = um->last_weight_state_value_x_prev;
  // um->last_weight_state_value_x_prev = weight_state_value_x_curr;

  um->last_weight_state_value_x_prev2_int =
      um->last_weight_state_value_x_prev_int;
  um->last_weight_state_value_x_prev_int = weight_state_value_x_curr_int;

  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (dspInBuf[i]) {
      free(dspInBuf[i]);
    }
  }
}
