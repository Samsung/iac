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
 * @file gaindown.c
 * @brief Audio gain down function
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "gaindown.h"

#include "fixedp11_5.h"
static float DmixTypeMat[][4] = {{1.0f, 1.0f, 0.707f, 0.707f},      // type1
                                 {0.707f, 0.707f, 0.707f, 0.707f},  // type2
                                 {1.0f, 0.866f, 0.866f, 0.866f}};   // type3

/*
a
b
t
p
q
*/
void gaindown(float *downmix_s[CHANNEL_LAYOUT_MAX],
              const unsigned char *channel_layout_map,
              const unsigned char *gain_down_map, float *dmixgain_f,
              int frame_size) {
  unsigned char pre_ch = 0;
  unsigned char base_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int lay_out = channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX) break;
    int channels = enc_get_layout_channel_count(lay_out) - pre_ch;
    for (int j = 0; j < channels; j++) {
      if (gain_down_map[base_ch + j] == 0) continue;
      for (int k = 0; k < frame_size; k++) {
        downmix_s[lay_out][j * frame_size + k] =
            downmix_s[lay_out][j * frame_size + k] * dmixgain_f[lay_out];
      }
    }
    base_ch += channels;
    pre_ch = enc_get_layout_channel_count(lay_out);
  }
}

/*
abtpq...
*/
void gaindown2(float *downmix_s[CHANNEL_LAYOUT_MAX],
               const unsigned char *channel_layout_map,
               const unsigned char *gain_down_map, float *dmixgain_f,
               int frame_size) {
  unsigned char pre_ch = 0;
  unsigned char base_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++) {
    int lay_out = channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX) break;
    int channels = enc_get_layout_channel_count(lay_out) - pre_ch;
    for (int j = 0; j < channels; j++) {
      if (gain_down_map[base_ch + j] == 0) continue;
      for (int k = 0; k < frame_size; k++) {
        downmix_s[lay_out][j + k * channels] =
            downmix_s[lay_out][j + k * channels] * dmixgain_f[lay_out];
      }
    }
    base_ch += channels;
    pre_ch = enc_get_layout_channel_count(lay_out);
  }
}