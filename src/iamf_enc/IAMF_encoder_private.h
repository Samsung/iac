/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file IAMF_encoder_private.h
 * @brief The iamf encoding framework
 * @version 0.1
 * @date Created 3/3/2023
 **/

#ifndef IAMF_ENCODER_PRIVATE_H
#define IAMF_ENCODER_PRIVATE_H

#include <stdarg.h>
#include <stdint.h>

#include "IAMF_debug.h"
#include "audio_defines.h"
#include "audio_loud_meter.h"
#include "downmixer.h"
#include "gaindown.h"
#include "iamf_asc.h"
#include "iamf_heq.h"
#include "intermediate_file_context.h"
#include "metadata_write.h"
#include "obu_multiway_tree.h"
#include "queue_plus.h"
#include "scalable_factor.h"
#include "upmixer.h"

typedef struct AudioElementEncoder AudioElementEncoder;

#define MKTAG(a, b, c, d) \
  (((unsigned)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))

enum {
  FLAC_METADATA_TYPE_STREAMINFO = 0,
  FLAC_METADATA_TYPE_PADDING,
  FLAC_METADATA_TYPE_APPLICATION,
  FLAC_METADATA_TYPE_SEEKTABLE,
  FLAC_METADATA_TYPE_VORBIS_COMMENT,
  FLAC_METADATA_TYPE_CUESHEET,
  FLAC_METADATA_TYPE_PICTURE,
  FLAC_METADATA_TYPE_INVALID = 127
};

typedef struct GlobalTimming {
  uint64_t global_timestamp;
  uint32_t time_rate;
} GlobalTimming;

typedef struct IASequenceHeader {
  uint32_t ia_code;
  uint32_t primary_profile;
  uint32_t additional_profile;

  int obu_redundant_copy;
} IASequenceHeader;

typedef struct CodecConfig {
  uint32_t codec_config_id;
  uint32_t codec_id;

  int size_of_decoder_config;
  unsigned char decoder_config[128];
  uint32_t num_samples_per_frame;
  int32_t roll_distance;

  int obu_redundant_copy;
} CodecConfig;

typedef struct ChannelAudioLayerConfig {
  int loudspeaker_layout;
  int output_gain_is_present_flag;
  int recon_gain_is_present_flag;
  int substream_count;
  int coupled_substream_count;
  // int loudness;
  LoudnessInfo loudness;
  int output_gain_flags;
  int output_gain;
} ChannelAudioLayerConfig;

typedef struct ScalableChannelLayoutConfig {
  uint32_t num_layers;
  uint32_t reserved;
  ChannelAudioLayerConfig channel_audio_layer_config[IA_CHANNEL_LAYOUT_COUNT];
} ScalableChannelLayoutConfig;

typedef struct AmbisonicsConfig {
  uint32_t ambisonics_mode;
  union {
    AmbisonicsMonoConfig ambisonics_mono_config;
    AmbisonicsProjectionConfig ambisonics_projection_config;
  };
} AmbisonicsConfig;

typedef struct ParamDefinition {
  uint32_t parameter_id;
  uint32_t parameter_rate;
  uint32_t param_definition_mode;

  uint64_t duration;
  uint64_t num_subblocks;
  uint64_t constant_subblock_duration;

  uint64_t *subblock_duration;
} ParamDefinition;

typedef struct AudioElement {
  uint32_t audio_element_id;    // leb128()
  uint32_t audio_element_type;  // 3
  uint32_t resevered;           // 5

  uint32_t codec_config_id;

  uint32_t num_substreams;
  uint32_t audio_substream_id[22];

  uint32_t num_parameters;
  uint32_t param_definition_type[22];
  ParamDefinition param_definition[22];

  uint32_t default_demix_mode;
  uint32_t default_demix_weight;

  union {
    ScalableChannelLayoutConfig scalable_channel_layout_config;
    AmbisonicsConfig ambisonics_config;
  };

  int obu_redundant_copy;
} AudioElement;

typedef struct LoudnessTarget {
  AudioLoudMeter loudmeter;
  int frame_size;
  int msize25pct;
  float loudness;

  float entire_loudness;
  float entire_peaksqr;
  float entire_truepeaksqr;

  int16_t entire_peak;
  int16_t entire_truepeak;

} LoudnessTarget;

typedef struct IamfDataObu {
  int obu_id;
  int obu_type;
  int size_of_data_obu;
  int size_of_data_obu_last;
  unsigned char *data_obu;

  uint64_t start_timestamp;
  uint64_t data_rate;
  uint64_t timestamp;
  uint64_t duration;

  uint64_t index;
} IamfDataObu;

#define MAX_MIX_PRESENTATIONS_NUM 10
typedef struct MixPresentationPriv {
  int mix_presentation_obu_id;
  MixPresentation mix_presentation;
  ParamDefinition element_mix_gain_para[MAX_AUDIO_ELEMENT_NUM];
  ParamDefinition output_mix_gain_para;
  IamfDataObu parameter_element_mix_gain_data_obu[MAX_AUDIO_ELEMENT_NUM];
  IamfDataObu parameter_output_mix_gain_data_obu;

  IamfDataObu mix_presentatin_obu;
  uint8_t mix_redundant_copy;
} MixPresentationPriv;

typedef struct DescriptorConfig {
  IASequenceHeader ia_sequence_header;
  CodecConfig codec_config;

  uint32_t num_audio_elements;
  AudioElement audio_element[MAX_AUDIO_ELEMENT_NUM];

  uint32_t num_mix_presentations;
  MixPresentationPriv mix_presentation_priv[MAX_MIX_PRESENTATIONS_NUM];

  LoudnessTarget loudness_target[MAX_MEASURED_LAYOUT_NUM];
} DescriptorConfig;

#define MAX_NUM_OBU_IDS 100

enum {
  OBU_DATA_TYPE_INVALIDE = -1,
  OBU_DATA_TYPE_SUBSTREAM,
  OBU_DATA_TYPE_PARAMETER,
};

typedef struct SyncSyntax {
  uint32_t global_offset;
  uint32_t num_obu_ids;

  uint32_t obu_id[MAX_NUM_OBU_IDS];
  uint32_t obu_data_type[MAX_NUM_OBU_IDS];
  uint32_t reinitialize_decoder[MAX_NUM_OBU_IDS];
  int64_t relative_offset[MAX_NUM_OBU_IDS];

  uint32_t concatenation_rule;
} SyncSyntax;

/*
typedef enum {
  AUDIO_FRAME_SIZE_INVALID = -1,
  AUDIO_FRAME_SIZE_OPUS = 960,
  AUDIO_FRAME_SIZE_AAC = 1024,
  AUDIO_FRAME_SIZE_PCM = 960
} AudioFrameSize;
*/
typedef enum {
  AUDIO_PRESKIP_SIZE_INVALID = -1,
  AUDIO_PRESKIP_SIZE_OPUS = 312,
  AUDIO_PRESKIP_SIZE_AAC = 0,
  AUDIO_PRESKIP_SIZE_FLAC = 0,
  AUDIO_PRESKIP_SIZE_PCM = 0
} AudioPreskipSize;

typedef enum {
  AUDIO_DECODER_DELAY_INVALID = -1,
  AUDIO_DECODER_DELAY_OPUS = 0,
  AUDIO_DECODER_DELAY_AAC = 720,
  AUDIO_DECODER_DELAY_FLAC = 0,
  AUDIO_DECODER_DELAY_PCM = 0
} AudioDecoderDelay;

typedef enum {
  PARAMETER_DEFINITION_MIX_GAIN = 0,
  PARAMETER_DEFINITION_DEMIXING_INFO,
  PARAMETER_DEFINITION_RECON_GAIN_INFO
} ParameterDefinitionType;

typedef struct LoudGainMeasure {
  AudioLoudMeter loudmeter[MAX_CHANNELS];
  AudioTruePeakMeter peakmeter[MAX_CHANNELS];   // for loudness measure
  AudioTruePeakMeter peakmeter2[MAX_CHANNELS];  // for gain measure.
  int frame_size;
  int max_loudness_layout;
  int max_gain_layout;
  int msize25pct;
  float loudness[MAX_CHANNELS];
  float gain[MAX_CHANNELS];

  float entire_loudness[MAX_CHANNELS];
  float entire_peaksqr[MAX_CHANNELS];
  float entire_truepeaksqr[MAX_CHANNELS];

  int16_t entire_peak[MAX_CHANNELS];
  int16_t entire_truepeak[MAX_CHANNELS];

  float entire_peaksqr_gain[MAX_CHANNELS];
  float entire_truepeaksqr_gain[MAX_CHANNELS];
  float dmixgain_lin[MAX_CHANNELS];
  int measure_end;

  unsigned char channel_layout_map[IA_CHANNEL_LAYOUT_COUNT];
  unsigned char gaindown_flag[IA_CHANNEL_LAYOUT_COUNT];  // channel layout that
                                                         // have gain value.
} LoudGainMeasure;

typedef struct encode_creator_t {
  int opcode;
  int (*init)(AudioElementEncoder *st);
  int (*control)(AudioElementEncoder *, int, va_list);
  int (*encode)(AudioElementEncoder *, int, int, int, void *, unsigned char *);
  int (*close)(AudioElementEncoder *st);
} encode_creator_t;

typedef struct decode_creator_t {
  int opcode;
  int (*init)(AudioElementEncoder *st);
  int (*decode)(AudioElementEncoder *, int, int, int, unsigned char *, int,
                int16_t *);
  int (*close)(AudioElementEncoder *st);
} decode_creator_t;

#define ENTRY_COUNT 16

typedef struct IA_CORE_ENCODER {
  void *dep_encoder[ENTRY_COUNT];
  void *dep_encoder2[ENTRY_COUNT];
  IAPacket ia_packet[ENTRY_COUNT];
  int channel;
  int stream_count;
  int coupled_stream_count;
  unsigned char enc_stream_map[255];
} IA_CORE_ENCODER;

typedef struct IA_CORE_DECODER {
  void *dep_decoder[ENTRY_COUNT];
  int channel;
  int stream_count;
  int coupled_stream_count;
  unsigned char dec_stream_map[255];
  unsigned char extra_data[10];
  unsigned char extra_data_size;
} IA_CORE_DECODER;

enum QUEUE_STEPS { QUEUE_DMPD, QUEUE_LD, QUEUE_SF, QUEUE_RG, QUEUE_STEP_MAX };

#ifndef AUDIO_ELEMENT_START_ID
#define AUDIO_ELEMENT_START_ID 10
#endif

typedef struct ChannelBasedEnc {
  uint32_t input_sample_rate;
  int frame_size;

  IAChannelLayoutType layout_in;

  uint32_t recon_gain_flag;
  uint32_t output_gain_flag;
  uint32_t scalefactor_mode;

  unsigned char channel_layout_map[IA_CHANNEL_LAYOUT_COUNT];
  unsigned char gaindown_map[MAX_CHANNELS];  // channles that have gain value.
  unsigned char
      recongain_map[MAX_CHANNELS];  // layout that have recon gain value.
  DownMixer *downmixer_ld;
  DownMixer *downmixer_rg;
  DownMixer *downmixer_enc;
  LoudGainMeasure *loudgain;
  Mdhr mdhr;
  UpMixer *upmixer;
  ScalableFactor *sf;

  FileContext fc;

  int the_preskip_frame;
  int the_dec_delay_frame;
  QueuePlus queue_dm[QUEUE_STEP_MAX];  // asc
  QueuePlus queue_wg[QUEUE_STEP_MAX];  // heq
  QueuePlus queue_m[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_s[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_r[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_d[IA_CHANNEL_LAYOUT_COUNT];
  QueuePlus queue_rg[IA_CHANNEL_LAYOUT_COUNT];

  QueuePlus queue_pad_i;
  QueuePlus queue_pad_f;

  // ASC and HEQ
  IAMF_ASC *asc;
  IAMF_HEQ *heq;
} ChannelBasedEnc;

typedef struct SceneBasedEnc {
  // TODO
  uint32_t input_sample_rate;
  int frame_size;
  AmbisonicsConfig ambisonics_config;
} SceneBasedEnc;

#define MAX_SUBSTREAMS 16

typedef struct AudioElementEncoder {
  AudioElementType element_type;
  int element_id;

  uint32_t num_substreams;
  uint32_t audio_substream_id[MAX_SUBSTREAMS];

  uint32_t num_parameters;
  uint32_t param_definition_type[MAX_SUBSTREAMS];
  ParamDefinition param_definition[MAX_SUBSTREAMS];

  int disable_demix;
  uint32_t default_demix_mode;
  uint32_t default_demix_weight;
  int default_demix_is_set;

  uint32_t input_sample_rate;
  int bits_per_sample;
  int sample_format;
  int channels;
  int frame_size;
  int preskip_size;
  int dec_delay_size;
  int codec_id;

  IA_CORE_ENCODER ia_core_encoder[IA_CHANNEL_LAYOUT_COUNT];
  // below interfaces are defined for encoding to get encoded frame
  int (*encode_init)(AudioElementEncoder *st);
  int (*encode_ctl)(AudioElementEncoder *, int, va_list);
  int (*encode_frame)(AudioElementEncoder *, int, int, int, void *,
                      unsigned char *);
  int (*encode_close)(AudioElementEncoder *st);
  int initial_padding;
  int padding;

  // below interfaces are defined for encoding to get recon gain value
  int (*encode_init2)(AudioElementEncoder *st);
  int (*encode_ctl2)(AudioElementEncoder *, int, va_list);
  int (*encode_frame2)(AudioElementEncoder *, int, int, int, void *,
                       unsigned char *);
  int (*encode_close2)(AudioElementEncoder *st);

  IA_CORE_DECODER ia_core_decoder[IA_CHANNEL_LAYOUT_COUNT];
  int (*decode_init)(AudioElementEncoder *st);
  int (*decode_frame)(AudioElementEncoder *, int, int, int, unsigned char *,
                      int, int16_t *);
  int (*decode_close)(AudioElementEncoder *st);

  int channel_groups;  // 1: Non-Scalable format, >1: Scalable format

  int samples;  //
  IamfDataObu substream_data_obu[MAX_SUBSTREAMS];
  IamfDataObu parameter_demixing_data_obu;
  IamfDataObu parameter_recon_gain_data_obu;

  GlobalTimming *global_timming;

  union {
    ChannelBasedEnc channel_based_enc;
    SceneBasedEnc scene_based_enc;
  };
  int redundant_copy;
  struct AudioElementEncoder *next;
} AudioElementEncoder;

struct IAMF_Encoder {
  uint32_t input_sample_rate;
  int bits_per_sample;
  int sample_format;
  int frame_size;
  int preskip_size;
  int codec_id;
  AudioElementEncoder *audio_element_enc;
  ObuIDManager *obu_id_manager;
  // int codec_config_id;

  DescriptorConfig descriptor_config;
  int is_descriptor_changed;

  SyncSyntax sync_syntax;
  int need_place_sync;

  GlobalTimming global_timming;

  int is_standalone;
  int profile;
};

LoudGainMeasure *immersive_audio_encoder_loudgain_create(
    const unsigned char *channel_layout_map, int sample_rate, int frame_size);
int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm,
                                             float *inbuffer,
                                             int channel_layout);
int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float *inbuffer,
                                         int channel_layout, int begin_ch,
                                         int nch);
int immersive_audio_encoder_loudgain_destory(LoudGainMeasure *lm);
#endif /*IAMF_ENCODER_PRIVATE_H*/
