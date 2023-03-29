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
 * @file scalable_factor.c
 * @brief Calculate the reconstruct gain
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "scalable_factor.h"

#include "scalable_format.h"
#define _USE_MATH_DEFINES
#include <time.h>

#include "fixedp11_5.h"
#include "math.h"

float scalefactor_list_[SF_LEN] = {
    1,           0.928577102, 0.857154205, 0.785731307,
    0.714308409, 0.642885512, 0.571462614, 0.500039716,
    0.428616819, 0.357193921, 0.285771023, 0.214348126,
    0.142925228, 0.071502330, 0.000079432, 0};
static float spl714avg_data[12] = {
    0,
};
static float spl512avg_data[8] = {
    0,
};
static float spl312avg_data[6] = {
    0,
};
static void conv_scalable_factorpcm(unsigned char *pcmbuf, void *dspbuf[],
                                    int nch, int dtype, int frame_size) {
  float **outbuff = (float **)dspbuf;
  if (dtype == 0) {
    int16_t *buff = (int16_t *)pcmbuf;

    for (int i = 0; i < nch; i++) {
      for (int j = 0; j < frame_size; j++) {
        outbuff[i][j] =
            (float)(buff[i + j * nch]) / 32768.0f;  /// why / 32768.0f??
      }
    }
  } else if (dtype == 1) {
    float *buff = (float *)pcmbuf;

    for (int i = 0; i < nch; i++) {
      for (int j = 0; j < frame_size; j++) {
        outbuff[i][j] = (buff[i + j * nch]);
      }
    }
  } else if (dtype == 2) {
    float *buff = (float *)pcmbuf;

    for (int i = 0; i < nch; i++) {
      for (int j = 0; j < frame_size; j++) {
        outbuff[i][j] = (buff[i * frame_size + j]);
      }
    }
    // memcpy(outbuff, buff, sizeof(float)*frame_size*nch);
  }
}
float sum_(float *arr, int size) {
  float ret = 0.0;
  for (int i = 0; i < size; i++) {
    ret += arr[i];
  }
  return ret;
}
void calc_rms(float *mBuf[], int channels, float *sum_sig, float *rms_sig,
              int frame_size) {
  float *mBuf_sq = (float *)malloc(frame_size * sizeof(float));

  for (int i = 0; i < channels; i++) {
    for (int j = 0; j < frame_size; j++) {
      mBuf_sq[j] = mBuf[i][j] * mBuf[i][j];
    }
    sum_sig[i] = sum_(mBuf_sq, frame_size);
    rms_sig[i] = sqrt((sum_sig[i]) / frame_size);
  }
}
void rms_test(InScalableBuffer inbuffer, CHANNEL_LAYOUT_TYPE clayer,
              RmsStruct *rms, int frame_size) {
  float *mBuf[MAX_CHANNELS];
  float *rBuf[MAX_CHANNELS];
  float *diffmrBuff[MAX_CHANNELS];

  for (int i = 0; i < MAX_CHANNELS; i++) {
    mBuf[i] = (float *)malloc(frame_size * sizeof(float));
    rBuf[i] = (float *)malloc(frame_size * sizeof(float));
    diffmrBuff[i] = (float *)malloc(frame_size * sizeof(float));
    memset(mBuf[i], 0x00, frame_size * sizeof(float));
    memset(rBuf[i], 0x00, frame_size * sizeof(float));
    memset(diffmrBuff[i], 0x00, frame_size * sizeof(float));
  }
  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m,
                          inbuffer.dtype_m, frame_size);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r,
                          inbuffer.dtype_r, frame_size);

  for (int i = 0; i < inbuffer.channels_m; i++)
    for (int j = 0; j < frame_size; j++) {
      diffmrBuff[i][j] = mBuf[i][j] - rBuf[i][j];
    }
  calc_rms(mBuf, inbuffer.channels_m, rms->sum_sig, rms->rms_sig, frame_size);
  calc_rms(diffmrBuff, inbuffer.channels_m, rms->sum_nse, rms->rms_nse,
           frame_size);
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (mBuf[i]) free(mBuf[i]);
    if (rBuf[i]) free(rBuf[i]);
    if (diffmrBuff[i]) free(diffmrBuff[i]);
  }
}

float rms_(float *input, int size) {
  float average = 0.0;
  for (int i = 0; i < size; i++) {
    average += input[i] * input[i];
  }
  average /= (float)(size);
  return (sqrt(average) + pow(10.0, -20));
}

float calc_snr(float *origsig, float *reconsig, int size) {
  float sig_rms = rms_(origsig, size) + pow(10.0, -20);
  float *diff = (float *)malloc(size * sizeof(float));
  for (int i = 0; i < size; i++) diff[i] = reconsig[i] - origsig[i];

  float nse_rms = rms_(diff, size) + pow(10.0, -20);
  float snr_db = 20 * log10(nse_rms / sig_rms);
  if (diff) free(diff);
  return snr_db;
}

float calc_smr(float *origsig, float *mixedsig, int size) {
  float sig_rms = rms_(origsig, size) + pow(10.0, -20);
  float mixed_rms = rms_(mixedsig, size) + pow(10.0, -20);
  float snr_db = 20 * log10(sig_rms / mixed_rms);
  return snr_db;
}

int get_min_index(float *in, int size) {
  int index = 0;
  float value = in[0];
  for (int i = 0; i < size; i++) {
    if (in[i] < value) {
      index = i;
      value = in[i];
    }
  }
  return index;
}
void calc_scalefactor1(float *origsig[], float *reconsig[], int channels,
                       float thres, int *scalefactor_index,
                       float *scalefactor_data, int frame_size) {
  float diff_list[SF_LEN];

  float *scaled_sig = (float *)malloc(frame_size * sizeof(float));
  float *diff_sig = (float *)malloc(frame_size * sizeof(float));
  float *origsig_sq = (float *)malloc(frame_size * sizeof(float));
  float *reconsig_sq = (float *)malloc(frame_size * sizeof(float));
  if (scaled_sig) memset(scaled_sig, 0x00, frame_size * sizeof(float));
  if (diff_sig) memset(diff_sig, 0x00, frame_size * sizeof(float));
  if (origsig_sq) memset(origsig_sq, 0x00, frame_size * sizeof(float));
  if (reconsig_sq) memset(reconsig_sq, 0x00, frame_size * sizeof(float));
  for (int i = 0; i < channels; i++) {
    float snr_ = calc_snr(origsig[i], reconsig[i], frame_size);
    if (calc_snr(origsig[i], reconsig[i], frame_size) > thres) {
      for (int j = 0; j < SF_LEN; j++) {
        for (int k = 0; k < frame_size; k++) {
          scaled_sig[k] = reconsig[i][k] * scalefactor_list_[j];
          diff_sig[k] = origsig[i][k] - scaled_sig[k];
          diff_sig[k] = diff_sig[k] * diff_sig[k];
        }
        diff_list[j] = sum_(diff_sig, frame_size);
      }
      for (int k = 0; k < frame_size; k++) {
        origsig_sq[k] = origsig[i][k] * origsig[i][k];
        reconsig_sq[k] = reconsig[i][k] * reconsig[i][k];
      }
      scalefactor_index[i] = get_min_index(diff_list, SF_LEN);
      scalefactor_data[i] =
          sqrt((sum_(origsig_sq, frame_size) + pow(10.0, -20)) /
               (sum_(reconsig_sq, frame_size) + pow(10.0, -20)));
      // printf("scalefactor_index %d %f\n", scalefactor_index[i],
      // scalefactor_data[i]);
      if (scalefactor_index[i] == 0xf) {
        if (sum_(origsig_sq, frame_size) != 0) {
          scalefactor_index[i] = 0x0e;
        } else
          scalefactor_data[i] = 0;
      }
    } else {
      scalefactor_index[i] = 0;
      scalefactor_data[i] = 1.0;
    }
    if (scalefactor_data[i] > 1.0) {
      scalefactor_data[i] = 1.0;
    }
  }
  if (scaled_sig) free(scaled_sig);
  if (diff_sig) free(diff_sig);
  if (origsig_sq) free(origsig_sq);
  if (reconsig_sq) free(reconsig_sq);
}

void calc_scalefactor2(float *origsig, float *mixedsig, float *reconsig,
                       float thres, float *splavg, int *scalefactor_index,
                       float *scalefactor_data, int frame_size) {
  float N = 3.0;
  float diff_list[SF_LEN];

  float *scaled_sig = (float *)malloc(frame_size * sizeof(float));
  float *diff_sig = (float *)malloc(frame_size * sizeof(float));
  float *origsig_sq = (float *)malloc(frame_size * sizeof(float));
  float *reconsig_sq = (float *)malloc(frame_size * sizeof(float));
  if (scaled_sig) memset(scaled_sig, 0x00, frame_size * sizeof(float));
  if (diff_sig) memset(diff_sig, 0x00, frame_size * sizeof(float));
  if (origsig_sq) memset(origsig_sq, 0x00, frame_size * sizeof(float));
  if (reconsig_sq) memset(reconsig_sq, 0x00, frame_size * sizeof(float));

  float smr = calc_smr(origsig, mixedsig, frame_size);
  float origsig_db = 20 * log10(rms_(origsig, frame_size));
  *splavg =
      (2.0 / (N + 1.0)) * origsig_db + (1.0 - 2.0 / (N + 1.0)) * (*splavg);

  if (origsig_db < -80.0 && (*splavg) < -80.0) {
    *scalefactor_index = 0xf;
    *scalefactor_data = 0;
  } else if (smr < thres) {
    for (int j = 0; j < SF_LEN; j++) {
      for (int k = 0; k < frame_size; k++) {
        scaled_sig[k] = reconsig[k] * scalefactor_list_[j];
        diff_sig[k] = origsig[k] - scaled_sig[k];
        diff_sig[k] = diff_sig[k] * diff_sig[k];
      }
      diff_list[j] = sum_(diff_sig, frame_size);
    }

    for (int k = 0; k < frame_size; k++) {
      origsig_sq[k] = origsig[k] * origsig[k];
      reconsig_sq[k] = reconsig[k] * reconsig[k];
    }
    *scalefactor_index = get_min_index(diff_list, SF_LEN);
    *scalefactor_data = sqrt((sum_(origsig_sq, frame_size) + +pow(10.0, -20)) /
                             (sum_(reconsig_sq, frame_size) + pow(10.0, -20)));
    if (*scalefactor_index == 0xf) {
      if (sum_(origsig_sq, frame_size) != 0) {
        *scalefactor_index = 0x0e;
      } else
        scalefactor_data = 0;
    }
  } else {
    *scalefactor_index = 0;
    *scalefactor_data = 1.0;
  }
  if (scalefactor_data && *scalefactor_data > 1.0) {
    *scalefactor_data = 1.0;
  }

  if (scaled_sig) free(scaled_sig);
  if (diff_sig) free(diff_sig);
  if (origsig_sq) free(origsig_sq);
  if (reconsig_sq) free(reconsig_sq);
}

void scalefactor1_test(InScalableBuffer inbuffer, CHANNEL_LAYOUT_TYPE clayer,
                       ScalerFactorStruct *sf, int frame_size) {
  float *mBuf[MAX_CHANNELS];
  float *rBuf[MAX_CHANNELS];
  for (int i = 0; i < MAX_CHANNELS; i++) {
    mBuf[i] = (float *)malloc(frame_size * sizeof(float));
    rBuf[i] = (float *)malloc(frame_size * sizeof(float));
    memset(mBuf[i], 0x00, frame_size * sizeof(float));
    memset(rBuf[i], 0x00, frame_size * sizeof(float));
  }
  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m,
                          inbuffer.dtype_m, frame_size);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r,
                          inbuffer.dtype_r, frame_size);
  calc_scalefactor1(mBuf, rBuf, inbuffer.channels_m, -9.0,
                    sf->scalefactor_index, sf->scalefactor_data, frame_size);
}

int scalablefactor_init() {
  for (int i = 0; i < 12; i++) {
    spl714avg_data[i] = 0.0;
  }
  for (int i = 0; i < 8; i++) {
    spl512avg_data[i] = 0.0;
  }
  for (int i = 0; i < 6; i++) {
    spl312avg_data[i] = 0.0;
  }
  return 0;
}

ScalableFactor *scalablefactor_create(const unsigned char *channel_layout_map,
                                      int frame_size) {
  ScalableFactor *sf = (ScalableFactor *)malloc(sizeof(ScalableFactor));
  memset(sf, 0x00, sizeof(ScalableFactor));
  memcpy(sf->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_MAX);
  sf->frame_size = frame_size;
  return sf;
}

void scalablefactor_destroy(ScalableFactor *sf) {
  if (sf) free(sf);
}

void cal_scalablefactor2(ScalableFactor *sf, Mdhr *mdhr,
                         InScalableBuffer inbuffer, CHANNEL_LAYOUT_TYPE clayer,
                         CHANNEL_LAYOUT_TYPE llayer,
                         int recongain_cls[enc_channel_cnt]) {
  int channels = enc_get_layout_channel_count(clayer);
  int channels_last = enc_get_layout_channel_count(llayer);
  ScalerFactorStruct sfs;
  int chx[12] = {
      0,
  };
  float *mBuf[MAX_CHANNELS];
  float *rBuf[MAX_CHANNELS];
  float *sBuf[MAX_CHANNELS];
  for (int i = 0; i < MAX_CHANNELS; i++) {
    mBuf[i] = (float *)malloc(sf->frame_size * sizeof(float));
    rBuf[i] = (float *)malloc(sf->frame_size * sizeof(float));
    sBuf[i] = (float *)malloc(sf->frame_size * sizeof(float));
    memset(mBuf[i], 0x00, sf->frame_size * sizeof(float));
    memset(rBuf[i], 0x00, sf->frame_size * sizeof(float));
    memset(sBuf[i], 0x00, sf->frame_size * sizeof(float));
  }
  if (sf->scalefactor_mode == 0) {
    RmsStruct rms;
    rms_test(inbuffer, clayer, &rms, sf->frame_size);
    for (int i = 0; i < channels; i++) {
      if (rms.sum_sig[i] == 0) chx[i] = 0xf;
    }
  } else if (sf->scalefactor_mode == 1 || sf->scalefactor_mode == 2) {
    if (sf->scalefactor_mode == 1) {
      for (int i = 0; i < 12; i++) {
        sfs.scalefactor_index[i] = 0;
        sfs.scalefactor_data[i] = 1;
      }
      scalefactor1_test(inbuffer, clayer, &sfs, sf->frame_size);

      uint8_t *cl_lay = enc_get_layout_channels(clayer);
      int cl = 0;
      int last_cl = 0;
      for (int i = 0; i < channels; i++) {
        cl = cl_lay[i];
        if (inbuffer.scalable_map[cl] == 1) {
          chx[i] = float_to_qf(sfs.scalefactor_data[i], 8);
        }
      }
    } else if (sf->scalefactor_mode == 2) {
      conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m,
                              inbuffer.dtype_m, sf->frame_size);
      conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r,
                              inbuffer.dtype_r, sf->frame_size);
      conv_scalable_factorpcm(inbuffer.inbuffer_s, sBuf, inbuffer.channels_s,
                              inbuffer.dtype_s, sf->frame_size);
      int cl_index = 0;
      float thres = -6.0;
      for (int i = 0; i < 12; i++) {
        sfs.scalefactor_index[i] = 0;
        sfs.scalefactor_data[i] = 1;
      }
      uint8_t *cl_lay = enc_get_layout_channels(clayer);
      uint8_t *ll_lay = enc_get_layout_channels(llayer);
      int cl = 0;
      int last_cl = 0;
      for (int i = 0; i < channels; i++) {
        cl = cl_lay[i];
        if (inbuffer.scalable_map[cl] == 1) {
          if (recongain_cls[cl] >= 0) {
            chx[i] = recongain_cls[cl];
            continue;
          }
          for (int j = last_cl; j < channels_last; j++) {
            int lcl = ll_lay[j];
            if (inbuffer.relevant_mixed_cl[lcl] == 1) {
              calc_scalefactor2(mBuf[i], sBuf[j], rBuf[i], thres,
                                (sf->spl_avg_data[clayer] + i),
                                (sfs.scalefactor_index + i),
                                (sfs.scalefactor_data + i), sf->frame_size);
              chx[i] = float_to_qf(sfs.scalefactor_data[i], 8);
              recongain_cls[cl] = chx[i];
              if (lcl == enc_channel_mono)
                last_cl = j;
              else
                last_cl = j + 1;
              break;
            }
          }
        }
      }
    }
  }

  for (int i = 0; i < channels; i++) {
    mdhr->scalablefactor[clayer][i] = chx[i];
  }

  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (mBuf[i]) {
      free(mBuf[i]);
    }
    if (rBuf[i]) {
      free(rBuf[i]);
    }
    if (sBuf[i]) {
      free(sBuf[i]);
    }
  }
#if 0
  if (clayer == CHANNEL_LAYOUT_312)
  {
    mdhr->chsilence[CHANNEL_LAYOUT_312] = 0;
    mdhr->chsilence[CHANNEL_LAYOUT_312] = mdhr->chsilence[CHANNEL_LAYOUT_312] | chx[gML3];
    mdhr->chsilence[CHANNEL_LAYOUT_312] = mdhr->chsilence[CHANNEL_LAYOUT_312] | (chx[gMR3] << 8);

  }
  else if (clayer == CHANNEL_LAYOUT_512)
  {
    mdhr->chsilence[CHANNEL_LAYOUT_512] = 0;
    mdhr->chsilence[CHANNEL_LAYOUT_512] = mdhr->chsilence[CHANNEL_LAYOUT_512] | chx[gMSL5];
    mdhr->chsilence[CHANNEL_LAYOUT_512] = mdhr->chsilence[CHANNEL_LAYOUT_512] | (chx[gMSR5] << 8);
    mdhr->chsilence[CHANNEL_LAYOUT_512] = mdhr->chsilence[CHANNEL_LAYOUT_512] | (chx[gMHL5] << 16);
    mdhr->chsilence[CHANNEL_LAYOUT_512] = mdhr->chsilence[CHANNEL_LAYOUT_512] | (chx[gMHR5] << 24);
  }
  else if (clayer == CHANNEL_LAYOUT_714)
  {
    mdhr->chsilence[CHANNEL_LAYOUT_714] = 0;
    mdhr->chsilence[CHANNEL_LAYOUT_714] = mdhr->chsilence[CHANNEL_LAYOUT_714] | chx[gBL];
    mdhr->chsilence[CHANNEL_LAYOUT_714] = mdhr->chsilence[CHANNEL_LAYOUT_714] | (chx[gBR] << 8);
    mdhr->chsilence[CHANNEL_LAYOUT_714] = mdhr->chsilence[CHANNEL_LAYOUT_714] | (chx[gHBL] << 16);
    mdhr->chsilence[CHANNEL_LAYOUT_714] = mdhr->chsilence[CHANNEL_LAYOUT_714] | (chx[gHBR] << 24);
  }
#endif
}
