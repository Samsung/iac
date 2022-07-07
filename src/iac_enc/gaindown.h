#ifndef __GAINDOWN_H_
#define __GAINDOWN_H_
#include "opus_extension.h"
#include "stdint.h"
#include "scalable_format.h"

void tpq_downgain(float *inbuf, void *outbuf, int nch, float sdn);
void ab_downgain(int16_t* inbuffer, int inbuffer_ch, int16_t* inbuffer_tpq, int tpq_ch, void *outbuf1, void *outbuf2, float sdn, int dmix_type, int weight_type);
void gaindown(float *downmix_s[CHANNEL_LAYOUT_MAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, float *dmixgain_f, int frame_size);
void gaindown2(float *downmix_s[CHANNEL_LAYOUT_MAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, float *dmixgain_f, int frame_size);
#endif
