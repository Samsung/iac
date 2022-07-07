#ifndef IMMERSIVE_AUDIO_ENCODER_PRIVATE_H
#define IMMERSIVE_AUDIO_ENCODER_PRIVATE_H

#include "metadata_write.h"
#include "audio_defines.h"
#include <stdint.h>
#include <stdarg.h>
#include "audio_loud_meter.h"
#include "downmixer.h"
#include "upmixer.h"
#include "gaindown.h"
#include "scalable_factor.h"
#include "intermediate_file_context.h"
#include "queue_plus.h"

#include "ia_asc.h"
#include "ia_heq.h"

typedef struct {
  AudioLoudMeter loudmeter[MAX_CHANNELS];
  AudioTruePeakMeter peakmeter[MAX_CHANNELS]; //for loudness measure
  AudioTruePeakMeter peakmeter2[MAX_CHANNELS]; //for gain measure.
  int frame_size;
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

  unsigned char channel_layout_map[IA_CHANNEL_LAYOUT_COUNT];
  unsigned char gaindown_flag[IA_CHANNEL_LAYOUT_COUNT]; // channel layout that have gain value.
}LoudGainMeasure;

typedef struct
{
  int opcode;
  int(*init)(IAEncoder *st);
  int(*control)(IAEncoder *, int, va_list);
  int(*encode)(IAEncoder *, int, int, int, int16_t *, unsigned char*);
  int(*close)(IAEncoder *st);
} encode_creator_t;

typedef struct
{
  int opcode;
  int(*init)(IAEncoder *st);
  int(*decode)(IAEncoder *, int, int, int, unsigned char*, int, int16_t *);
  int(*close)(IAEncoder *st);
} decode_creator_t;

#define ENTRY_COUNT 7
typedef struct {
  void *dep_encoder[ENTRY_COUNT];
  void *dep_encoder2[ENTRY_COUNT];
  int channel;
  int stream_count;
  int coupled_stream_count;
  unsigned char enc_stream_map[255];
}IA_ENCODER_DCG;

typedef struct {
  void *dep_decoder[ENTRY_COUNT];
  int channel;
  int stream_count;
  int coupled_stream_count;
  unsigned char dec_stream_map[255];
  unsigned char extra_data[10];
  unsigned char extra_data_size;
}IA_DECODER_DCG;


enum QUEUE_STEPS {
  QUEUE_DMPD,
  QUEUE_LD,
  QUEUE_SF,
  QUEUE_RG,
  QUEUE_STEP_MAX
};
struct IAEncoder {
  uint32_t input_sample_rate;
  int frame_size;
  int preskip_size;
  int codec_id;
  int channel_groups; //1: Non-Scalable format, >1: Scalable format
  IA_ENCODER_DCG ia_encoder_dcg[IA_CHANNEL_LAYOUT_COUNT];
  //below interfaces are defined for encoding to get encoded frame
  int(*encode_init)(IAEncoder *st);
  int(*encode_ctl)(IAEncoder *, int, va_list);
  int(*encode_frame)(IAEncoder *, int, int, int, int16_t *, unsigned char*);
  int(*encode_close)(IAEncoder *st);

  //below interfaces are defined for encoding to get recon gain value
  int(*encode_init2)(IAEncoder *st);
  int(*encode_ctl2)(IAEncoder *, int, va_list);
  int(*encode_frame2)(IAEncoder *, int, int, int, int16_t *, unsigned char*);
  int(*encode_close2)(IAEncoder *st);

  uint32_t recon_gain_flag;
  uint32_t output_gain_flag;

  IA_DECODER_DCG ia_decoder_dcg[IA_CHANNEL_LAYOUT_COUNT];
  int(*decode_init)(IAEncoder *st);
  int(*decode_frame)(IAEncoder *, int, int, int, unsigned char*, int, int16_t *);
  int(*decode_close)(IAEncoder *st);

  uint32_t scalefactor_mode;

  unsigned char channel_layout_map[IA_CHANNEL_LAYOUT_COUNT];
  unsigned char gaindown_map[MAX_CHANNELS]; // channles that have gain value.
  DownMixer *downmixer_ld;
  DownMixer *downmixer_rg;
  DownMixer *downmixer_enc;
  LoudGainMeasure *loudgain;
  Mdhr mdhr;
  UpMixer *upmixer;
  ScalableFactor *sf;

  FileContext fc;


  FILE *fp_dmix;
  char dmix_fn[255];
  FILE *fp_weight;
  char weight_fn[255];
  int the_preskip_frame;
  QueuePlus queue_dm[QUEUE_STEP_MAX]; //asc
  QueuePlus queue_wg[QUEUE_STEP_MAX]; //heq
  QueuePlus queue_m[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_r[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_d[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_rg[IA_CHANNEL_LAYOUT_COUNT];

  //ASC and HEQ
  IA_ASC *asc;
  IA_HEQ *heq;
} ;


LoudGainMeasure * immersive_audio_encoder_loudgain_create(const unsigned char *channel_layout_map, int sample_rate, int frame_size);
int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout);
int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int begin_ch, int nch);
int immersive_audio_encoder_loudgain_destory(LoudGainMeasure *lm);
#endif /*IMMERSIVE_AUDIO_ENCODER_PRIVATE_H*/
