#ifndef _UPMIXER_H
#define _UPMIXER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "metadata_write.h"
#include "audio_defines.h"
#include "opus_types.h"
#include "scalable_format.h"

#ifndef CHANNEL_LAYOUT_UMAX

typedef enum {
  CHANNEL_LAYOUT_U100, //1.0.0
  CHANNEL_LAYOUT_U200, //2.0.0 
  CHANNEL_LAYOUT_U510, //5.1.0
  CHANNEL_LAYOUT_U512, //5.1.2
  CHANNEL_LAYOUT_U514, //5.1.4
  CHANNEL_LAYOUT_U710, //7.1.0
  CHANNEL_LAYOUT_U712, //7.1.2
  CHANNEL_LAYOUT_U714, //7.1.4
  CHANNEL_LAYOUT_U312, //3.1.2
  CHANNEL_LAYOUT_UMAX
}CHANNEL_LAYOUT_U;

#endif

typedef struct {
  int16_t *up_input[CHANNEL_LAYOUT_UMAX];
  float *upmix[CHANNEL_LAYOUT_UMAX];
  int recon_gain_flag;
  int frame_size;
  int preskip_size;

  float last_sf1[12]; //312 last scalefactor
  float last_sfavg1[12]; //312 last average scalefactor
  float last_sf2[12]; //512 last scalefactor
  float last_sfavg2[12]; //512 last average scalefactor
  float last_sf3[12];// 714 last scalefactor
  float last_sfavg3[12]; //714 last average scalefactor
  
  float last_sf[CHANNEL_LAYOUT_UMAX][12];
  float last_sfavg[CHANNEL_LAYOUT_UMAX][12];
  //float up_input_temp[MAX_CHANNELS][FRAME_SIZE];
  float *ch_data[enc_channel_cnt];
  float *buffer[enc_channel_mixed_cnt];
  Mdhr mdhr_l;
  Mdhr mdhr_c;
  float last_weight_state_value_x_prev;
  float last_weight_state_value_x_prev2;
  unsigned char channel_layout_map[CHANNEL_LAYOUT_UMAX];
  unsigned char  channel_order[enc_channel_cnt];
  int pre_layout;
  unsigned char scalable_map[CHANNEL_LAYOUT_UMAX][enc_channel_cnt];

  float *hanning;
  float *startWin;
  float *stopWin;
}UpMixer;

UpMixer * upmix_create(int recon_gain_flag, const unsigned char *channel_layout_map, int frame_size, int preskip_size);
//void upmix_push_buffer(UpMixer *um, unsigned char *ab2ch, unsigned char *tpq4ch, unsigned char *suv6ch);
void upmix(UpMixer *um);
void upmix2(UpMixer *um);
void upmix3(UpMixer *um, const unsigned char *gain_down_map);
void upmix_destroy(UpMixer *um);

#endif
