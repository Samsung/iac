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
 * @file pcmconvert.c
 * @brief Different PCM format converting
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "pcmconvert.h"

#include "audio_defines.h"

// int16_t
void reorder_channels(uint8_t *channel_map, int channels, unsigned pcm_frames,
                      int16_t *samples) {
  int16_t t[MAX_CHANNELS];
  int map_ch;

  for (int i = 0; i < pcm_frames; i++) {
    for (int ch = 0; ch < channels; ch++) {
      map_ch = channel_map[ch];
      t[ch] = samples[i * channels + map_ch];
    }
    for (int ch = 0; ch < channels; ch++) {
      samples[i * channels + ch] = t[ch];
    }
  }
}

// float
void reorder_channels2(uint8_t *channel_map, int channels, unsigned pcm_frames,
                       float *samples) {
  float t[MAX_CHANNELS];
  int map_ch;

  for (int i = 0; i < pcm_frames; i++) {
    for (int ch = 0; ch < channels; ch++) {
      map_ch = channel_map[ch];
      t[ch] = samples[i * channels + map_ch];
    }
    for (int ch = 0; ch < channels; ch++) {
      samples[i * channels + ch] = t[ch];
    }
  }
}

// int(16 24 32) reorder
void reorder_channels3(uint8_t *channel_map, int channels, unsigned pcm_frames,
                       void *samples, int bits_per_sample) {
  int map_ch;
  if (bits_per_sample == 16) {
    int16_t t[MAX_CHANNELS];
    int16_t *out = (int16_t *)samples;
    for (int i = 0; i < pcm_frames; i++) {
      for (int ch = 0; ch < channels; ch++) {
        map_ch = channel_map[ch];
        t[ch] = out[i * channels + map_ch];
      }
      for (int ch = 0; ch < channels; ch++) {
        out[i * channels + ch] = t[ch];
      }
    }
  } else if (bits_per_sample == 24) {
    unsigned char t[MAX_CHANNELS * 3];
    unsigned char *out = (unsigned char *)samples;
    for (int i = 0; i < pcm_frames; i++) {
      for (int ch = 0; ch < channels; ch++) {
        map_ch = channel_map[ch];
        t[ch * 3] = out[(i * channels + map_ch) * 3];
        t[ch * 3 + 1] = out[(i * channels + map_ch) * 3 + 1];
        t[ch * 3 + 2] = out[(i * channels + map_ch) * 3 + 2];
      }
      for (int ch = 0; ch < channels; ch++) {
        out[(i * channels + ch) * 3] = t[ch * 3];
        out[(i * channels + ch) * 3 + 1] = t[ch * 3 + 1];
        out[(i * channels + ch) * 3 + 2] = t[ch * 3 + 2];
      }
    }
  } else if (bits_per_sample == 32) {
    int32_t t[MAX_CHANNELS];
    int32_t *out = (int32_t *)samples;
    for (int i = 0; i < pcm_frames; i++) {
      for (int ch = 0; ch < channels; ch++) {
        map_ch = channel_map[ch];
        t[ch] = out[i * channels + map_ch];
      }
      for (int ch = 0; ch < channels; ch++) {
        out[i * channels + ch] = t[ch];
      }
    }
  }
}

void interleaved2interleaved_pcm2float(void *src, void *dst, int channels,
                                       int frame_size, int bits_per_sample,
                                       int sample_format) {
  float *out = (float *)dst;
  if (sample_format == 1) {  // little-endian
    if (bits_per_sample == 16) {
      float den = 0x8000;
      int16_t *in = (int16_t *)src;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[i + j * channels] = (float)(in[i + j * channels]) / den;
        }
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      float den = 0x800000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int32_t a = in[(i + j * channels) * 3];
          int32_t b = in[(i + j * channels) * 3 + 1];
          int32_t c = in[(i + j * channels) * 3 + 2];
          int32_t d = 0;
          d = d | (c << 16) | (b << 8) | a;
          if (d & 0x800000) {
            d = d | 0xFF000000;
          }
          out[i + j * channels] = (float)(d) / den;
        }
      }
    } else if (bits_per_sample == 32) {
      int32_t *in = (int32_t *)src;
      float den = 0x80000000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[i + j * channels] = (float)(in[i + j * channels]) / den;
        }
      }
    }
  } else if (sample_format == 0) {  // big-endian
    if (bits_per_sample == 16) {
      int16_t *in = (int16_t *)src;
      float den = 0x8000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int16_t a = BigToLittle16(in[i + j * channels]);
          out[i + j * channels] = (float)(a) / den;
        }
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      float den = 0x800000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int32_t a = in[(i + j * channels) * 3];
          int32_t b = in[(i + j * channels) * 3 + 1];
          int32_t c = in[(i + j * channels) * 3 + 2];
          int32_t d = 0;
          d = d | (a << 16) | (b << 8) | c;
          if (d & 0x800000) {
            d = d | 0xFF000000;
          }
          out[i + j * channels] = (float)(d) / den;
        }
      }
    } else if (bits_per_sample == 32) {
      int32_t *in = (int32_t *)src;
      float den = 0x80000000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int32_t a = BigToLittle32(in[i + j * channels]);
          out[i + j * channels] = (float)(a) / den;
        }
      }
    }
  }
}

void interleaved2plane_pcm2float(void *src, void *dst, int channels,
                                 int frame_size, int bits_per_sample,
                                 int sample_format) {
  float *out = (float *)dst;
  if (sample_format == 1) {  // little-endian
    if (bits_per_sample == 16) {
      float den = 0x8000;
      int16_t *in = (int16_t *)src;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[i * frame_size + j] = (float)(in[i + j * channels]) / den;
        }
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      float den = 0x800000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int32_t a = in[(i + j * channels) * 3];
          int32_t b = in[(i + j * channels) * 3 + 1];
          int32_t c = in[(i + j * channels) * 3 + 2];
          int32_t d = 0;
          d = d | (c << 16) | (b << 8) | a;
          if (d & 0x800000) {
            d = d | 0xFF000000;
          }
          out[i * frame_size + j] = (float)(d) / den;
        }
      }
    } else if (bits_per_sample == 32) {
      int32_t *in = (int32_t *)src;
      float den = 0x80000000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          out[i * frame_size + j] = (float)(in[i + j * channels]) / den;
        }
      }
    }
  } else if (sample_format == 0) {  // big-endian
    if (bits_per_sample == 16) {
      int16_t *in = (int16_t *)src;
      float den = 0x8000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int16_t a = BigToLittle16(in[i + j * channels]);
          out[i * frame_size + j] = (float)(a) / den;
        }
      }
    } else if (bits_per_sample == 24) {
      unsigned char *in = (unsigned char *)src;
      float den = 0x800000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int32_t a = in[(i + j * channels) * 3];
          int32_t b = in[(i + j * channels) * 3 + 1];
          int32_t c = in[(i + j * channels) * 3 + 2];
          int32_t d = 0;
          d = d | (a << 16) | (b << 8) | c;
          if (d & 0x800000) {
            d = d | 0xFF000000;
          }
          out[i * frame_size + j] = (float)(d) / den;
        }
      }
    } else if (bits_per_sample == 32) {
      int32_t *in = (int32_t *)src;
      float den = 0x80000000;
      for (int i = 0; i < channels; i++) {
        for (int j = 0; j < frame_size; j++) {
          int32_t a = BigToLittle32(in[i + j * channels]);
          out[i * frame_size + j] = (float)(a) / den;
        }
      }
    }
  }
}

void plane2interleaved_float2float(void *src, void *dst, int channels,
                                   int frame_size) {
  float *in = (float *)src;
  float *out = (float *)dst;
  for (int i = 0; i < channels; i++) {
    for (int j = 0; j < frame_size; j++) {
      out[i + j * channels] = in[i * frame_size + j];
    }
  }
}

void plane2interleaved_float2pcm(void *src, void *dst, int channels,
                                 int frame_size) {
  float *in = (float *)src;
  int16_t *out = (int16_t *)dst;
  for (int i = 0; i < channels; i++) {
    for (int j = 0; j < frame_size; j++) {
      out[i + j * channels] = (int16_t)(in[i * frame_size + j] * 32767.0f);
    }
  }
}
