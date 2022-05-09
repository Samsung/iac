#ifndef __DOWNMIXER_H_
#define __DOWNMIXER_H
#include "opus_extension.h"
#include "opus_types.h"
#include "scalable_format.h"

#define DefDmixType = 1; //default value
#define DefWeightType = 1; //default value

#ifndef CHANNEL_LAYOUT_DMAX

typedef enum {
  CHANNEL_LAYOUT_D100, //1.0.0
  CHANNEL_LAYOUT_D200, //2.0.0 
  CHANNEL_LAYOUT_D510, //5.1.0
  CHANNEL_LAYOUT_D512, //5.1.2
  CHANNEL_LAYOUT_D514, //5.1.4
  CHANNEL_LAYOUT_D710, //7.1.0
  CHANNEL_LAYOUT_D712, //7.1.2
  CHANNEL_LAYOUT_D714, //7.1.4
  CHANNEL_LAYOUT_D312, //3.1.2
  CHANNEL_LAYOUT_DMAX
}CHANNEL_LAYOUT_D;

#endif

#ifndef MAX_CHANNELS
define MAX_CHANNELS 12
#endif
typedef struct {
  int channels;
  unsigned char channel_layout_map[CHANNEL_LAYOUT_DMAX];
  int channel_groups;
  float *downmix_m[CHANNEL_LAYOUT_DMAX];
  float *downmix_s[CHANNEL_LAYOUT_DMAX];
  float weight_state_value_x_prev;
  float *ch_data[enc_channel_cnt];
  float *buffer[enc_channel_mixed_cnt];
  unsigned char  channel_order[enc_channel_cnt + 1];
  unsigned char gaindown_map[CHANNEL_LAYOUT_DMAX][enc_channel_cnt];
}DownMixer;
DownMixer * downmix_create(const unsigned char *channel_layout_map);
void downmix_destroy(DownMixer *dm);
int downmix(DownMixer *dm, unsigned char* inbuffer, int dmix_type, int weight_type);
int downmix2(DownMixer *dm, unsigned char* inbuffer, int dmix_type, int weight_type);
void downmix_clear(DownMixer *dm);
//unsigned char* get_downmix_data(DownMixer *dm, DownmixChannelType type);
#endif
