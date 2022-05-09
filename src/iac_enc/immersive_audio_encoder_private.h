#ifndef IMMERSIVE_AUDIO_ENCODER_PRIVATE_H
#define IMMERSIVE_AUDIO_ENCODER_PRIVATE_H

#include "opus_multistream.h"
#include "metadata_write.h"
#include "opus_extension.h"
#include "stdint.h"
#include <opus_projection.h>
#include "stdarg.h"
#include "audio_loud_meter.h"
#include "downmixer.h"
#include "upmixer.h"
#include "gaindown.h"
#include "scalable_factor.h"
#include "intermediate_file_context.h"

#include "ia_asc.h"
#include "ia_heq.h"

typedef struct {
  AudioLoudMeter loudmeter[MAX_CHANNELS];
  AudioTruePeakMeter peakmeter[MAX_CHANNELS];

  int max_loudness_layout;
  int max_gain_layout;
  float msize25pct;
  float loudness[MAX_CHANNELS];
  float gain[MAX_CHANNELS];

  float entire_loudness[MAX_CHANNELS];
  float entire_peaksqr[MAX_CHANNELS];
  float entire_truepeaksqr[MAX_CHANNELS];

  float entire_peaksqr_gain[MAX_CHANNELS];
  float entire_truepeaksqr_gain[MAX_CHANNELS];
  float dmixgain_lin[MAX_CHANNELS];
  int measure_end;

  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX];
  unsigned char gaindown_flag[CHANNEL_LAYOUT_MAX]; // channel layout that have gain value.
}LoudGainMeasure;


typedef struct {
  OpusMSEncoder *opus_encoder_dcg;
  int channel_dcg;
  opus_uint32 meta_mode_dcg;
  int stream_count_dcg;
  int coupled_stream_count_dcg;
  unsigned char enc_stream_map_dcg[255];
}IA_ENCODER_DCG;

typedef struct {
  OpusMSDecoder *opus_decoder_dcg;
  int channel_dcg;
  int stream_count_dcg;
  int coupled_stream_count_dcg;
  unsigned char dec_stream_map_dcg[255];
}IA_DECODER_DCG;

struct IAEncoder {
  uint32_t input_sample_rate;
  IA_ENCODER_DCG ia_encoder_dcg[CHANNEL_LAYOUT_MAX];
  //opus_uint32 compression;
  opus_uint32 substream_size_flag;
  opus_uint32 recon_gain_flag;
  opus_uint32 output_gain_flag;

  IA_DECODER_DCG ia_decoder_dcg[CHANNEL_LAYOUT_MAX];
  opus_uint32 scalefactor_mode;

  int scalable_format; //0: Non-Scalable format, 1: Scalable format
  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX];
  unsigned char gaindown_map[MAX_CHANNELS]; // channles that have gain value.
  DownMixer *downmixer;
  LoudGainMeasure *loudgain;
  Mdhr mdhr;
  UpMixer *upmixer;
  ScalableFactor *sf;

  FileContext fc;


  FILE *fp_dmix;
  char dmix_fn[255];
  FILE *fp_weight;
  char weight_fn[255];

  //ASC and HEQ
  IA_ASC *asc;
  IA_HEQ *heq;
} ;


LoudGainMeasure * immersive_audio_encoder_loudgain_create(const unsigned char *channel_layout_map, int sample_rate);
int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout);
int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int begin_ch, int nch);
int immersive_audio_encoder_loudgain_destory(LoudGainMeasure *lm);
#endif /*IMMERSIVE_AUDIO_ENCODER_PRIVATE_H*/
