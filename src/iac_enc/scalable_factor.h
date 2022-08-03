#ifndef _PATCH_MDHR_H
#define _PATCH_MDHR_H


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "metadata_write.h"
#include "audio_defines.h"
#include "opus_types.h"
#include "scalable_format.h"

#define SF_LEN 16


typedef struct {
  float sum_sig[12];
  float rms_sig[12];
  float sum_nse[12];
  float rms_nse[12];
}RmsStruct;

typedef struct {
  int scalefactor_index[12];
  float scalefactor_data[12];
}ScalerFactorStruct;

typedef struct {
  int channels_m;
  int dtype_m; // 0:int16, 1: float32
  unsigned char * inbuffer_m; //Level Ok is the signal power for the frame #k of a channel of the down-mixed audio for CL #i.

  int channels_r;
  int dtype_r;
  unsigned char * inbuffer_r; //Level Dk is the signal power for the frame #k of the de-mixed channel for CL #i (after demixing).

  int channels_s;
  int dtype_s;
  unsigned char * inbuffer_s; //Level Mk is the signal power for the frame #k of the relevant mixed channel of the down-mixed audio for CL #i-1.

  unsigned char * gaindown_map;
  unsigned char * scalable_map;
  unsigned char * relevant_mixed_cl;
}InScalableBuffer;

typedef struct {
  float spl_avg_data[CHANNEL_LAYOUT_MAX][12];
  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX];
  int scalefactor_mode;
  int frame_size;
}ScalableFactor;
int scalablefactor_init();
ScalableFactor * scalablefactor_create(const unsigned char *channel_layout_map, int frame_size);
void cal_scalablefactor(Mdhr *mdhr, InScalableBuffer inbuffer, int scalefactor, CHANNEL_LAYOUT_TYPE clayer);
void cal_scalablefactor2(ScalableFactor *sf, Mdhr *mdhr, InScalableBuffer inbuffer, CHANNEL_LAYOUT_TYPE clayer, CHANNEL_LAYOUT_TYPE llayer, int recongain_cls[enc_channel_cnt]);
void scalablefactor_destroy(ScalableFactor *sf);
#endif