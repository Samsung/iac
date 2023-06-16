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
 * @file intermediate_file_context.h
 * @brief The debug function for dumping different file types
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef _INTERMEDIATE_FILE_CONTEXT_H
#define _INTERMEDIATE_FILE_CONTEXT_H

#include "IAMF_encoder.h"
#include "audio_defines.h"
#include "stdint.h"
#include "stdio.h"
#include "wav/dep_wavreader.h"
#include "wav/dep_wavwriter.h"

typedef struct ChannelBasedEnc ChannelBasedEnc;

typedef struct {
  int format;
  int sample_rate;
  int bits_per_sample;
  int channels;
  uint32_t data_length;
} wavinfo;

typedef enum {
  FILE_DOWNMIX_M = 0,
  FILE_DOWNMIX_S,
  FILE_GAIN_DOWN,
  FILE_UPMIX,
  FILE_DECODED,
  FILE_ENCODED,
  FILE_SCALEFACTOR,
  INTER_FILE_MAX
} INTER_FILE_TYPE;

static char* downmix_m_wav[IA_CHANNEL_LAYOUT_COUNT] = {  // 0
    "m100_down.wav", "m200_down.wav", "m510_down.wav",
    "m512_down.wav", "m514_down.wav", "m710_down.wav",
    "m712_down.wav", "m714_down.wav", "m312_down.wav"};

static char* downmix_s_wav[IA_CHANNEL_LAYOUT_COUNT] = {  // 1
    "s100_down.wav", "s200_down.wav", "s510_down.wav",
    "s512_down.wav", "s514_down.wav", "s710_down.wav",
    "s712_down.wav", "s714_down.wav", "s312_down.wav"};

static char* gaindown_wav[IA_CHANNEL_LAYOUT_COUNT] = {  // 2
    "g100_down.wav", "g200_down.wav", "g510_down.wav",
    "g512_down.wav", "g514_down.wav", "g710_down.wav",
    "g712_down.wav", "g714_down.wav", "g312_down.wav"};

static char* upmix_wav[IA_CHANNEL_LAYOUT_COUNT] = {  // 3
    "r100_up.wav", "r200_up.wav", "r510_up.wav", "r512_up.wav", "r514_up.wav",
    "r710_up.wav", "r712_up.wav", "r714_up.wav", "r312_up.wav"};

static char* decoded_wav[IA_CHANNEL_LAYOUT_COUNT] = {  // 4
    "d100_dec.wav", "d200_dec.wav", "d510_dec.wav",
    "d512_dec.wav", "d514_dec.wav", "d710_dec.wav",
    "d712_dec.wav", "d714_dec.wav", "d312_dec.wav"};

static char* encoded_ia[IA_CHANNEL_LAYOUT_COUNT] = {  // 5
    "e100_down.ia", "e200_down.ia", "e510_down.ia",
    "e512_down.ia", "e514_down.ia", "e710_down.ia",
    "e712_down.ia", "e714_down.ia", "e312_down.ia"};

static char* scalefactor_cfg[IA_CHANNEL_LAYOUT_COUNT] = {  // 5
    "f100_scale.cfg", "f200_scale.cfg", "f510_scale.cfg",
    "f512_scale.cfg", "f514_scale.cfg", "f710_scale.cfg",
    "f712_scale.cfg", "f714_scale.cfg", "f312_scale.cfg"};

typedef struct {
  wavinfo info;
  void* file;
} TempFile;

typedef struct {
  char in_file[256];
  char file_name[256];
  void* in_wavf;

  TempFile f_downmix_m_wav[IA_CHANNEL_LAYOUT_COUNT];
  TempFile f_downmix_s_wav[IA_CHANNEL_LAYOUT_COUNT];
  TempFile f_upmix_wav[IA_CHANNEL_LAYOUT_COUNT];
  TempFile f_gaindown_wav[IA_CHANNEL_LAYOUT_COUNT];
  TempFile f_decoded_wav[IA_CHANNEL_LAYOUT_COUNT];
  TempFile f_encoded_ia[IA_CHANNEL_LAYOUT_COUNT];
  TempFile f_scalefactor_cfg[IA_CHANNEL_LAYOUT_COUNT];

} FileContext;

void ia_intermediate_file_writeopen(ChannelBasedEnc* ce, int file_type,
                                    const char* file_name);
void ia_intermediate_file_readopen(ChannelBasedEnc* ce, int file_type,
                                   const char* file_name);
void ia_intermediate_file_write(ChannelBasedEnc* ce, int file_type,
                                const char* file_name, void* input, int size);
int ia_intermediate_file_read(ChannelBasedEnc* ce, int file_type,
                              const char* file_name, void* output, int size);
void ia_intermediate_file_writeclose(ChannelBasedEnc* ce, int file_type,
                                     const char* file_name);
void ia_intermediate_file_readclose(ChannelBasedEnc* ce, int file_type,
                                    const char* file_name);

#endif
