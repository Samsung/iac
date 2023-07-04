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
 * @file obuwrite.c
 * @brief Write different OBU
 * @version 0.1
 * @date Created 3/3/2023
 **/

#include "obuwrite.h"

#include <string.h>

#include "bitstreamrw.h"

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 12
#endif

#ifndef FRAME_SIZE
#define FRAME_SIZE 960  //
#endif

#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE \
  (MAX_CHANNELS * sizeof(int16_t) * FRAME_SIZE)  // 960*2/channel
#endif

static const uint32_t kMaximumLeb128Size = 8;
static const uint8_t kLeb128ByteMask = 0x7f;  // Binary: 01111111
static const uint64_t kMaximumLeb128Value = UINT32_MAX;

const char *obu_type_to_string(AUDIO_OBU_TYPE type) {
  switch (type) {
    case OBU_IA_Codec_Config:
      return "OBU_IA_Codec_Config";
    case OBU_IA_Audio_Element:
      return "OBU_IA_Audio_Element";
    case OBU_IA_Mix_Presentation:
      return "OBU_IA_Mix_Presentation";
    case OBU_IA_Parameter_Block:
      return "OBU_IA_Parameter_Block";
    case OBU_IA_Temporal_Delimiter:
      return "OBU_IA_Temporal_Delimiter";
    case OBU_IA_Audio_Frame:
      return "OBU_IA_Audio_Frame";
    case OBU_IA_Sequence_Header:
      return "OBU_IA_Sequence_Header";
    default:
      break;
  }
  return "<Invalid OBU Type>";
}

static uint32_t uleb_size_in_bytes(uint64_t value) {
  uint32_t size = 0;
  do {
    ++size;
  } while ((value >>= 7) != 0);
  return size;
}

int uleb_encode(uint64_t value, uint32_t available, uint8_t *coded_value,
                uint32_t *coded_size) {
  const uint32_t leb_size = uleb_size_in_bytes(value);
  if (value > kMaximumLeb128Value || leb_size > kMaximumLeb128Size ||
      leb_size > available || !coded_value || !coded_size) {
    return -1;
  }

  for (uint32_t i = 0; i < leb_size; ++i) {
    uint8_t byte = value & 0x7f;
    value >>= 7;

    if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

    *(coded_value + i) = byte;
  }

  *coded_size = leb_size;
  return 0;
}

uint32_t iamf_obu_memmove(uint32_t obu_header_size, uint32_t obu_payload_size,
                          uint8_t *data) {
  const uint32_t length_field_size = uleb_size_in_bytes(obu_payload_size);
  const uint32_t move_dst_offset = length_field_size + obu_header_size;
  const uint32_t move_src_offset = obu_header_size;
  const uint32_t move_size = obu_payload_size;
  memmove(data + move_dst_offset, data + move_src_offset, move_size);
  return length_field_size;
}

static int bs_setbits_leb128(bitstream_t *bs, uint64_t num) {
  unsigned char coded_data_leb[128];
  int coded_size = 0;
  if (uleb_encode(num, sizeof(num), coded_data_leb, &coded_size) != 0) {
    return 0;
  }
  for (int i = 0; i < coded_size; i++) {
    bs_setbits(bs, coded_data_leb[i], 8);  // leb128()
  }
  return coded_size;
}

static int bs_setbits_buffer(bitstream_t *bs, unsigned char *str, int size) {
  if (!str) return 0;
  for (int i = 0; i < size; i++) {
    bs_setbits(bs, str[i], 8);
  }
  return size * 8;
}

uint32_t iamf_write_obu_header(AUDIO_OBU_TYPE obu_type, int playload_size,
                               int obu_redundant_copy, int obu_trimming_status,
                               int num_samples_to_trim_at_start,
                               int num_samples_to_trim_at_end,
                               int obu_extension, int extension_header_size,
                               uint8_t *const dst) {
  uint32_t size = 0;
  bitstream_t bs;
  bs_init(&bs, dst, MAX_PACKET_SIZE);

  bs_setbits(&bs, (int)obu_type, 5);
  bs_setbits(&bs, obu_redundant_copy ? 1 : 0, 1);   // obu_redundant_copy
  bs_setbits(&bs, obu_trimming_status ? 1 : 0, 1);  // obu_trimming_status_flag
  bs_setbits(&bs, obu_extension ? 1 : 0, 1);        // obu_extension_flag

  int size_of_trimming =
      obu_trimming_status ? (uleb_size_in_bytes(num_samples_to_trim_at_end) +
                             uleb_size_in_bytes(num_samples_to_trim_at_start))
                          : 0;
  int size_of_extension =
      obu_extension ? (uleb_size_in_bytes(extension_header_size)) : 0;
  int obu_size = playload_size + size_of_trimming + size_of_extension;

  bs_setbits_leb128(&bs, obu_size);

  if (obu_trimming_status) {
    bs_setbits_leb128(&bs, num_samples_to_trim_at_end);
    bs_setbits_leb128(&bs, num_samples_to_trim_at_start);
  }
  if (obu_extension) {  // This flag SHALL be set to 0 for this version of the
                        // specification
    bs_setbits_leb128(&bs, extension_header_size);
    unsigned char extension_header_bytes[] = {0};
    bs_setbits_buffer(&bs, extension_header_bytes, extension_header_size);
  }
  size = bs.m_posBase;
  return size;
}

#if 1

int iamf_write_uleb_obu_size(uint32_t obu_header_size,
                             uint32_t obu_payload_size, uint8_t *dest) {
  const uint32_t offset = obu_header_size;
  uint32_t coded_obu_size = 0;
  const uint32_t obu_size = (uint32_t)obu_payload_size;

  if (uleb_encode(obu_size, sizeof(obu_size), dest + offset, &coded_obu_size) !=
      0) {
    return CODEC_ERROR;
  }

  return CODEC_OK;
}

uint32_t iamf_write_obu_unit(const uint8_t *sampledata, int playload_size,
                             AUDIO_OBU_TYPE obu_type, int obu_redundant_copy,
                             int obu_trimming_status,
                             int num_samples_to_trim_at_start,
                             int num_samples_to_trim_at_end, int obu_extension,
                             uint8_t *const dst) {
  int extension_header_size = 0;  // TODO, this flag shall be set to 0 for the
                                  // current version of the specification
  int obu_header_size = 0;
  obu_header_size = iamf_write_obu_header(
      obu_type, playload_size, obu_redundant_copy, obu_trimming_status,
      num_samples_to_trim_at_start, num_samples_to_trim_at_end, obu_extension,
      extension_header_size, dst);

  if (playload_size > 0 && sampledata)
    memcpy(dst + obu_header_size, sampledata, playload_size);

  return (uint32_t)(obu_header_size + playload_size);
}

#endif
