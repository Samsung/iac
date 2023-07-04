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
 * @file obuwrite.h
 * @brief Write different OBU
 * @version 0.1
 * @date Created 3/3/2023
 **/

#ifndef OBU_WRITE_H_
#define OBU_WRITE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*!\brief OBU types. */
typedef enum ATTRIBUTE_PACKED {
  OBU_IA_Invalid = -1,
  OBU_IA_Codec_Config = 0,
  OBU_IA_Audio_Element = 1,
  OBU_IA_Mix_Presentation = 2,
  OBU_IA_Parameter_Block = 3,
  OBU_IA_Temporal_Delimiter = 4,
  OBU_IA_Audio_Frame = 5,
  OBU_IA_Audio_Frame_ID0 = 6,
  OBU_IA_Audio_Frame_ID1 = 7,
  OBU_IA_Audio_Frame_ID2 = 8,
  OBU_IA_Audio_Frame_ID3 = 9,
  OBU_IA_Audio_Frame_ID4 = 10,
  OBU_IA_Audio_Frame_ID5 = 11,
  OBU_IA_Audio_Frame_ID6 = 12,
  OBU_IA_Audio_Frame_ID7 = 13,
  OBU_IA_Audio_Frame_ID8 = 14,
  OBU_IA_Audio_Frame_ID9 = 15,
  OBU_IA_Audio_Frame_ID10 = 16,
  OBU_IA_Audio_Frame_ID11 = 17,
  OBU_IA_Audio_Frame_ID12 = 18,
  OBU_IA_Audio_Frame_ID13 = 19,
  OBU_IA_Audio_Frame_ID14 = 20,
  OBU_IA_Audio_Frame_ID15 = 21,
  OBU_IA_Audio_Frame_ID16 = 22,
  OBU_IA_Audio_Frame_ID17 = 23,
  OBU_IA_Sync = 30,  // Temporal usage.
  OBU_IA_Sequence_Header = 31,
  OBU_IA_MAX_Count

} AUDIO_OBU_TYPE;

/*!\brief Algorithm return codes */
typedef enum {
  /*!\brief Operation completed without error */
  CODEC_OK,

  /*!\brief Unspecified error */
  CODEC_ERROR,

} codec_err_t;

int uleb_encode(uint64_t value, uint32_t available, uint8_t *coded_value,
                uint32_t *coded_size);
uint32_t iamf_write_obu_unit(const uint8_t *sampledata, int playload_size,
                             AUDIO_OBU_TYPE obu_type, int obu_redundant_copy,
                             int obu_trimming_status,
                             int num_samples_to_trim_at_start,
                             int num_samples_to_trim_at_end, int obu_extension,
                             uint8_t *const dst);
#ifdef __cplusplus
}
#endif

#endif  // OBU_WRITE_H_
