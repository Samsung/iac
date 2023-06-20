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
 * @file downmixer.h
 * @brief Audio scalable downmix function
 * @version 0.1
 * @date Created 3/3/2023
 **/

#ifndef __DOWNMIXER_H_
#define __DOWNMIXER_H
#include "audio_defines.h"
#include "scalable_format.h"

#define DefDmixType = 1;    // default value
#define DefWeightType = 1;  // default value

typedef struct DownMixer {
  int channels;
  int frame_size;
  int preskip_size;
  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX];
  int channel_groups;
  float *downmix_m[CHANNEL_LAYOUT_MAX];
  float *downmix_s[CHANNEL_LAYOUT_MAX];
  float weight_state_value_x_prev;
  int weight_state_value_x_prev_int;
  float *ch_data[enc_channel_cnt];
  float *buffer[enc_channel_mixed_cnt];
  unsigned char channel_order[enc_channel_cnt + 1];
  unsigned char gaindown_map[CHANNEL_LAYOUT_MAX][enc_channel_cnt];
  int default_demix_mode;
  int default_demix_weight;
  int default_demix_is_set;
} DownMixer;
DownMixer *downmix_create(const unsigned char *channel_layout_map,
                          int frame_size);
int downmix_set_default_demix(DownMixer *dm, int demix_mode, int demix_weight);
void downmix_destroy(DownMixer *dm);
int downmix(DownMixer *dm, unsigned char *inbuffer, int size, int dmix_type,
            int weight_type);
int downmix2(DownMixer *dm, float *inbuffer, int size, int dmix_type,
             int weight_type);
void downmix_clear(DownMixer *dm);
#endif
