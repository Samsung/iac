#ifndef AAC_HEADER_H
#define AAC_HEADER_H

#include <stdint.h>

typedef struct {
  uint32_t sample_rate;
  uint32_t samples;
  uint32_t bit_rate;
  uint8_t  crc_absent;
  uint8_t  object_type;
  uint8_t  sampling_index;
  uint8_t  chan_config;
  uint8_t  num_aac_frames;
} AacHeader;
#endif //AAC_HEADER_H
