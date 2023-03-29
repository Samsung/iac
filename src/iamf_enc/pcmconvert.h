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
 * @file pcmconvert.h
 * @brief Different PCM format converting
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef PCM_CONVERT_H_
#define PCM_CONVERT_H_

#include "stdint.h"

#define BigToLittle16(A) \
  ((((uint16_t)(A)&0xff00) >> 8) | (((uint16_t)(A)&0x00ff) << 8))

#define BigToLittle32(A)                                                    \
  ((((uint32_t)(A)&0xff000000) >> 24) | (((uint32_t)(A)&0x00ff0000) >> 8) | \
   (((uint32_t)(A)&0x0000ff00) << 8) | (((uint32_t)(A)&0x000000ff) << 24))

void reorder_channels(uint8_t *channel_map, int channels, unsigned pcm_frames,
                      int16_t *samples);
void reorder_channels2(uint8_t *channel_map, int channels, unsigned pcm_frames,
                       float *samples);
void reorder_channels3(uint8_t *channel_map, int channels, unsigned pcm_frames,
                       void *samples, int bits_per_sample);
void interleaved2interleaved_pcm2float(void *src, void *dst, int channels,
                                       int frame_size, int bits_per_sample,
                                       int sample_format);
void plane2interleaved_float2float(void *src, void *dst, int channels,
                                   int frame_size);
void plane2interleaved_float2pcm(void *src, void *dst, int channels,
                                 int frame_size);
void interleaved2plane_pcm2float(void *src, void *dst, int channels,
                                 int frame_size, int bits_per_sample,
                                 int sample_format);
#endif  // PCM_CONVERT_H_
