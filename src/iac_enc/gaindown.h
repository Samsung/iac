#ifndef __GAINDOWN_H_
#define __GAINDOWN_H_
#include "opus_extension.h"
#include "stdint.h"

#ifndef CHANNEL_LAYOUT_GMAX

typedef enum {
  CHANNEL_LAYOUT_G100, //1.0.0
  CHANNEL_LAYOUT_G200, //2.0.0 
  CHANNEL_LAYOUT_G510, //5.1.0
  CHANNEL_LAYOUT_G512, //5.1.2
  CHANNEL_LAYOUT_G514, //5.1.4
  CHANNEL_LAYOUT_G710, //7.1.0
  CHANNEL_LAYOUT_G712, //7.1.2
  CHANNEL_LAYOUT_G714, //7.1.4
  CHANNEL_LAYOUT_G312, //3.1.2
  CHANNEL_LAYOUT_GMAX
}CHANNEL_LAYOUT_G;

#endif

void tpq_downgain(float *inbuf, void *outbuf, int nch, float sdn);
void ab_downgain(int16_t* inbuffer, int inbuffer_ch, int16_t* inbuffer_tpq, int tpq_ch, void *outbuf1, void *outbuf2, float sdn, int dmix_type, int weight_type);
void gaindown(float *downmix_s[CHANNEL_LAYOUT_GMAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, uint16_t *dmixgain, int frame_size);
void gaindown2(float *downmix_s[CHANNEL_LAYOUT_GMAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, uint16_t *dmixgain, int frame_size);
#endif
