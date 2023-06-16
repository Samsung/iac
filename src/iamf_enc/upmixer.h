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

#ifndef _UPMIXER_H
#define _UPMIXER_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_defines.h"
#include "metadata_write.h"
#include "scalable_format.h"

typedef struct {
  int16_t *up_input[CHANNEL_LAYOUT_MAX];
  float *upmix[CHANNEL_LAYOUT_MAX];
  int recon_gain_flag;
  int frame_size;
  int preskip_size;

  float last_sf1[12];     // 312 last scalefactor
  float last_sfavg1[12];  // 312 last average scalefactor
  float last_sf2[12];     // 512 last scalefactor
  float last_sfavg2[12];  // 512 last average scalefactor
  float last_sf3[12];     // 714 last scalefactor
  float last_sfavg3[12];  // 714 last average scalefactor

  float last_sf[CHANNEL_LAYOUT_MAX][12];
  float last_sfavg[CHANNEL_LAYOUT_MAX][12];
  // float up_input_temp[MAX_CHANNELS][FRAME_SIZE];
  float *ch_data[enc_channel_cnt];
  float *buffer[enc_channel_mixed_cnt];
  Mdhr mdhr_l;
  Mdhr mdhr_c;
  float last_weight_state_value_x_prev;
  float last_weight_state_value_x_prev2;
  int last_weight_state_value_x_prev_int;
  int last_weight_state_value_x_prev2_int;
  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX];
  unsigned char channel_order[enc_channel_cnt];
  int pre_layout;
  unsigned char scalable_map[CHANNEL_LAYOUT_MAX][enc_channel_cnt];
  unsigned char relevant_mixed_cl[CHANNEL_LAYOUT_MAX][enc_channel_cnt];
  float *hanning;
  float *startWin;
  float *stopWin;
} UpMixer;

UpMixer *upmix_create(int recon_gain_flag,
                      const unsigned char *channel_layout_map, int frame_size);
// void upmix_push_buffer(UpMixer *um, unsigned char *ab2ch, unsigned char
// *tpq4ch, unsigned char *suv6ch);
void upmix(UpMixer *um, const unsigned char *gain_down_map);
void upmix_set_preskip_size(UpMixer *um, int preskip_size);
void upmix_destroy(UpMixer *um);

#endif
