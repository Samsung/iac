
#ifndef OPUS_HEADER_H
#define OPUS_HEADER_H

#include <stdint.h>


enum {
	fL = 0x1,
	fR = 0x2,
	fC = 0x4,
	LFE = 0x8,
	bL = 0x10,
	bR = 0x20,
	bC = 0x100,
	sL = 0x200,
	sR = 0x400,
	// SRopus
	tfL = 0x1000,
	tfR = 0x2000,
	tbL = 0x4000,
	tbR = 0x8000
	// SRopus
};

typedef struct {
  uint32_t version;
  uint32_t output_channel_count;
  uint32_t preskip;
  uint32_t input_sample_rate;
  int32_t output_gain;
  uint32_t channel_mapping_family;
} OpusHeader;


#endif
