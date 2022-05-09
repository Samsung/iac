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
  case OBU_CODEC_SPECIFIC_INFO: return "OBU_CODEC_SPECIFIC_INFO";
  case OBU_IA_STATIC_META: return "OBU_IA_STATIC_META";
  case OBU_IA_TIMED_META: return "OBU_IA_TIMED_META";
  case OBU_IA_CODED_DATA: return "OBU_IA_CODED_DATA";
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

uint32_t opus_obu_memmove(uint32_t obu_header_size, uint32_t obu_payload_size,
  uint8_t *data) {
  const uint32_t length_field_size = uleb_size_in_bytes(obu_payload_size);
  const uint32_t move_dst_offset = length_field_size + obu_header_size;
  const uint32_t move_src_offset = obu_header_size;
  const uint32_t move_size = obu_payload_size;
  memmove(data + move_dst_offset, data + move_src_offset, move_size);
  return length_field_size;
}

uint32_t opus_write_obu_header(AUDIO_OBU_TYPE obu_type,
  int obu_extension, uint8_t *const dst)
{
  uint32_t size = 0;
  bitstream_t bs;
  bs_init(&bs, dst, MAX_PACKET_SIZE);

  bs_setbits(&bs, 0, 1);  // forbidden bit.
  bs_setbits(&bs, (int)obu_type, 4);
  bs_setbits(&bs, obu_extension ? 1 : 0, 1);
  bs_setbits(&bs, 1, 1); // obu_has_payload_length_field
  bs_setbits(&bs, 0, 1); // reserved

  if (obu_extension) {
    bs_setbits(&bs, obu_extension & 0xFF, 8);
  }

  size = bs.m_posBase;
  return size;
}


#if 1


int opus_write_uleb_obu_size(uint32_t obu_header_size, uint32_t obu_payload_size,
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

uint32_t opus_write_metadata_obu(uint8_t *const *metadata, int playload_size,
  uint8_t *const dst) {
  int obu_header_size = 0;
  obu_header_size = opus_write_obu_header(OBU_IA_TIMED_META, 0, dst);
  const uint32_t length_field_size = uleb_size_in_bytes(playload_size);
  opus_write_uleb_obu_size(obu_header_size, playload_size, dst);
  memcpy(dst + obu_header_size + length_field_size, metadata, playload_size);
  // Add trailing bits.

  return (uint32_t)(obu_header_size + length_field_size + playload_size);
}

uint32_t opus_write_codeddata_obu(const uint8_t *const sampledata, int playload_size,
  uint8_t *const dst) {
  int obu_header_size = 0;
  obu_header_size = opus_write_obu_header(OBU_IA_CODED_DATA, 0, dst);
  const uint32_t length_field_size = uleb_size_in_bytes(playload_size);
  opus_write_uleb_obu_size(obu_header_size, playload_size, dst);
  memcpy(dst + obu_header_size + length_field_size, sampledata, playload_size);
  // Add trailing bits.

  return (uint32_t)(obu_header_size + length_field_size + playload_size);
}
#endif
