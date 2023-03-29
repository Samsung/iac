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
 * @file metadata_write.h
 * @brief The metadate writing definition
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef _METADATA_WRITE_H
#define _METADATA_WRITE_H
#include "stdint.h"

#define CHANNEL_LAYOUT_MDHR 9
typedef struct {
  uint32_t dialog_onoff;
  uint32_t dialog_level;
  int16_t LKFSch[CHANNEL_LAYOUT_MDHR];
  int16_t digital_peak[CHANNEL_LAYOUT_MDHR];
  int16_t true_peak[CHANNEL_LAYOUT_MDHR];
  float dmixgain_f[CHANNEL_LAYOUT_MDHR];
  int16_t dmixgain_db[CHANNEL_LAYOUT_MDHR];

  uint32_t drc_profile;
  uint32_t chsilence[CHANNEL_LAYOUT_MDHR];
  uint8_t scalablefactor[CHANNEL_LAYOUT_MDHR][12];
  uint32_t len_of_4chauxstrm;
  uint32_t lfe_onoff;
  uint32_t lfe_gain;
  uint32_t len_of_6chauxstrm;
  uint32_t dmix_matrix_type;
  uint32_t weight_type;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t coding_type;
  uint32_t nsamples_of_frame;

} Mdhr;

#endif