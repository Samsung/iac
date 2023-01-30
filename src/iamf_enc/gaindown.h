#ifndef __GAINDOWN_H_
#define __GAINDOWN_H_
#include "audio_defines.h"
#include "stdint.h"
#include "scalable_format.h"

void gaindown(float *downmix_s[CHANNEL_LAYOUT_MAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, float *dmixgain_f, int frame_size);
void gaindown2(float *downmix_s[CHANNEL_LAYOUT_MAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, float *dmixgain_f, int frame_size);
#endif
