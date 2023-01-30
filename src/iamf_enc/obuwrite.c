#include <string.h>
#include "obuwrite.h"
#include "bitstreamrw.h"

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 12
#endif

#ifndef FRAME_SIZE
#define FRAME_SIZE 960 //
#endif

#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE  (MAX_CHANNELS*sizeof(int16_t)*FRAME_SIZE) // 960*2/channel
#endif

static const uint32_t kMaximumLeb128Size = 8;
static const uint8_t kLeb128ByteMask = 0x7f;  // Binary: 01111111
static const uint64_t kMaximumLeb128Value = UINT32_MAX;

const char *obu_type_to_string(AUDIO_OBU_TYPE type) {
  switch (type) {
  case OBU_IA_STREAM_INDICATOR: return "OBU_IA_STREAM_INDICATOR";
  case OBU_CODEC_SPECIFIC_INFO: return "OBU_CODEC_SPECIFIC_INFO";
  case OBU_IA_STATIC_META: return "OBU_IA_STATIC_META";
  case OBU_TEMPORAL_DELIMITOR: return "OBU_TEMPORAL_DELIMITOR";
  case OBU_DEMIXING_INFO: return "OBU_DEMIXING_INFO";
  case OBU_SUBSTREAM: return "OBU_SUBSTREAM";
  default: break;
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

uint32_t iamf_write_obu_header(AUDIO_OBU_TYPE obu_type,
  int obu_redundant_copy,
  int obu_trimming_status, int num_samples_to_trim_at_start, int num_samples_to_trim_at_end,
  int obu_extension, uint8_t *const dst)
{
  unsigned char coded_data_leb[128];
  int coded_size = 0;

  uint32_t size = 0;
  bitstream_t bs;
  bs_init(&bs, dst, MAX_PACKET_SIZE);

  bs_setbits(&bs, (int)obu_type, 5);
  bs_setbits(&bs, obu_redundant_copy ? 1 : 0, 1); // obu_redundant_copy
  bs_setbits(&bs, obu_trimming_status ? 1 : 0, 1); // obu_trimming_status_flag
  bs_setbits(&bs, obu_extension ? 1 : 0, 1); // obu_extension_flag
                         // obu_size...
  if (obu_trimming_status)
  {
    if (uleb_encode(num_samples_to_trim_at_end, sizeof(num_samples_to_trim_at_end), coded_data_leb,
      &coded_size) != 0) {
      return 0;
    }
    for (int i = 0; i < coded_size; i++)
      bs_setbits(&bs, coded_data_leb[i], 8);

    if (uleb_encode(num_samples_to_trim_at_start, sizeof(num_samples_to_trim_at_start), coded_data_leb,
      &coded_size) != 0) {
      return 0;
    }
    for (int i = 0; i < coded_size; i++)
      bs_setbits(&bs, coded_data_leb[i], 8);

  }
  if (obu_extension) {
    bs_setbits(&bs, obu_extension & 0xFF, 8);
  }
  size = bs.m_posBase;
  return size;
}

#if 1


int iamf_write_uleb_obu_size(uint32_t obu_header_size, uint32_t obu_payload_size,
  uint8_t *dest) {
  const uint32_t offset = obu_header_size;
  uint32_t coded_obu_size = 0;
  const uint32_t obu_size = (uint32_t)obu_payload_size;


  if (uleb_encode(obu_size, sizeof(obu_size), dest + offset,
    &coded_obu_size) != 0) {
    return CODEC_ERROR;
  }

  return CODEC_OK;
}

uint32_t iamf_write_obu_unit(const uint8_t * sampledata, int playload_size,
  AUDIO_OBU_TYPE obu_type, int obu_redundant_copy,
  int obu_trimming_status, int num_samples_to_trim_at_start, int num_samples_to_trim_at_end,
  int obu_extension, uint8_t *const dst) {
  int obu_header_size = 0;
  obu_header_size = iamf_write_obu_header(obu_type,
    obu_redundant_copy, obu_trimming_status, num_samples_to_trim_at_start, num_samples_to_trim_at_end,
    obu_extension, dst);
  const uint32_t length_field_size = uleb_size_in_bytes(playload_size);
  if (obu_header_size >1)
    memmove(dst + length_field_size + 1, dst + 1, obu_header_size - 1); //1: obu_type + obu_redundant_copy + obu_trimming_status_flag + obu_extension_flag

  iamf_write_uleb_obu_size(1, playload_size, dst);
  if(playload_size > 0 && sampledata)
    memcpy(dst + obu_header_size + length_field_size, sampledata, playload_size);
  // Add trailing bits.

  return (uint32_t)(obu_header_size + length_field_size + playload_size);
}
#endif
