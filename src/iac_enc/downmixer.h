#ifndef __DOWNMIXER_H_
#define __DOWNMIXER_H
#include "audio_defines.h"
#include "opus_types.h"
#include "scalable_format.h"

#define DefDmixType = 1; //default value
#define DefWeightType = 1; //default value

typedef struct {
  int channels;
  int frame_size;
  int preskip_size;
  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX];
  int channel_groups;
  float *downmix_m[CHANNEL_LAYOUT_MAX];
  float *downmix_s[CHANNEL_LAYOUT_MAX];
  float weight_state_value_x_prev;
  float *ch_data[enc_channel_cnt];
  float *buffer[enc_channel_mixed_cnt];
  unsigned char  channel_order[enc_channel_cnt + 1];
  unsigned char gaindown_map[CHANNEL_LAYOUT_MAX][enc_channel_cnt];
}DownMixer;
DownMixer * downmix_create(const unsigned char *channel_layout_map, int frame_size);
void downmix_destroy(DownMixer *dm);
int downmix2(DownMixer *dm, unsigned char* inbuffer, int size, int dmix_type, int weight_type);
void downmix_clear(DownMixer *dm);
//unsigned char* get_downmix_data(DownMixer *dm, DownmixChannelType type);
#endif
