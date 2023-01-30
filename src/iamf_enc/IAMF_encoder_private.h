#ifndef IAMF_ENCODER_PRIVATE_H
#define IAMF_ENCODER_PRIVATE_H

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

#include "obu_multiway_tree.h"

typedef struct AudioElementEncoder AudioElementEncoder;

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

#define MAX_AUDIO_ELEMENT_NUM 2

typedef struct {
  uint32_t ia_code;
  uint32_t version;
  uint32_t profile_version;

  int obu_redundant_copy;
}StartCode;

typedef struct {
  uint32_t codec_config_id;
  uint32_t codec_id;

  int size_of_decoder_config;
  unsigned char decoder_config[128];
  uint32_t num_samples_per_frame;
  int32_t roll_distance;

  int obu_redundant_copy;
}CodecConfig;

typedef struct {
  int loudspeaker_layout;
  int output_gain_is_present_flag;
  int recon_gain_is_present_flag;
  int substream_count;
  int coupled_substream_count;
  //int loudness;
  LoudnessInfo loudness;
  int output_gain_flags;
  int output_gain;
}ChannelAudioLayerConfig;

typedef struct {
  uint32_t num_layers;
  uint32_t reserved;
  ChannelAudioLayerConfig channel_audio_layer_config[IA_CHANNEL_LAYOUT_COUNT];
}ScalableChannelLayoutConfig;

/*
typedef struct {
  uint32_t output_channel_count;
  uint32_t substream_count;
  uint32_t channel_mapping[12]; //todo
}AmbisonicsMonoConfig;

typedef struct {
  uint32_t output_channel_count;
  uint32_t substream_count;
  uint32_t coupled_substream_count;
  uint32_t channel_mapping[12]; //todo
  uint32_t  demixing_matrix[DEMIXING_MATRIX_SIZE_MAX]; // todo DEMIXING_MATRIX_SIZE_MAX?
}AmbisonicsProjectionConfig;
*/
typedef struct {
  uint32_t ambisonics_mode;
  union {
    AmbisonicsMonoConfig ambisonics_mono_config;
    AmbisonicsProjectionConfig ambisonics_projection_config;
  };
}AmbisonicsConfig;


typedef struct {
  int parameter_id;
  int time_base;
}ParamDefinition;

typedef struct {
  uint32_t audio_element_id; //leb128()
  uint32_t audio_element_type; // 3
  uint32_t resevered; // 5

  uint32_t codec_config_id;

  uint32_t num_substreams;
  uint32_t audio_substream_id[22];

  uint32_t num_parameters;
  uint32_t param_definition_type[22];
  ParamDefinition param_definition[22];
  union {
    ScalableChannelLayoutConfig scalable_channel_layout_config;
    AmbisonicsConfig ambisonics_config;
  };

  int obu_redundant_copy;
}AudioElement;


typedef struct DescriptorConfig {
  StartCode start_code;
  CodecConfig codec_config;

  uint32_t num_mix_presentations;
  int element_mix_gain_para[MAX_AUDIO_ELEMENT_NUM][100];
  int output_mix_gain_para[MAX_AUDIO_ELEMENT_NUM];
  MixPresentation mix_presentation[MAX_AUDIO_ELEMENT_NUM];

  uint32_t num_audio_elements;
  AudioElement audio_element[MAX_AUDIO_ELEMENT_NUM];

}DescriptorConfig;

#define MAX_NUM_OBU_IDS 100
typedef struct SyncSyntax {
  uint32_t global_offset;
  uint32_t num_obu_ids;

  uint32_t obu_id[MAX_NUM_OBU_IDS];
  uint32_t obu_data_type[MAX_NUM_OBU_IDS];
  uint32_t reinitialize_decoder[MAX_NUM_OBU_IDS];
  int32_t relative_offset[MAX_NUM_OBU_IDS];

  uint32_t concatenation_rule;
}SyncSyntax;

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
  AUDIO_PRESKIP_SIZE_AAC = 720,
  AUDIO_PRESKIP_SIZE_PCM = 0
} AudioPreskipSize;

typedef enum {
  PARAMETER_DEFINITION_MIX_GAIN = 0,
  PARAMETER_DEFINITION_DEMIXING_INFO,
  PARAMETER_DEFINITION_RECON_GAIN_INFO
}ParameterDefinitionType;

typedef struct {
  AudioLoudMeter loudmeter[MAX_CHANNELS];
  AudioTruePeakMeter peakmeter[MAX_CHANNELS]; //for loudness measure
  AudioTruePeakMeter peakmeter2[MAX_CHANNELS]; //for gain measure.
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
  unsigned char gaindown_flag[IA_CHANNEL_LAYOUT_COUNT]; // channel layout that have gain value.
}LoudGainMeasure;

typedef struct
{
  int opcode;
  int(*init)(AudioElementEncoder *st);
  int(*control)(AudioElementEncoder *, int, va_list);
  int(*encode)(AudioElementEncoder *, int, int, int, int16_t *, unsigned char*);
  int(*close)(AudioElementEncoder *st);
} encode_creator_t;

typedef struct
{
  int opcode;
  int(*init)(AudioElementEncoder *st);
  int(*decode)(AudioElementEncoder *, int, int, int, unsigned char*, int, int16_t *);
  int(*close)(AudioElementEncoder *st);
} decode_creator_t;

#define ENTRY_COUNT 16
typedef struct {
  void *dep_encoder[ENTRY_COUNT];
  void *dep_encoder2[ENTRY_COUNT];
  int channel;
  int stream_count;
  int coupled_stream_count;
  unsigned char enc_stream_map[255];
}IA_CORE_ENCODER;

typedef struct {
  void *dep_decoder[ENTRY_COUNT];
  int channel;
  int stream_count;
  int coupled_stream_count;
  unsigned char dec_stream_map[255];
  unsigned char extra_data[10];
  unsigned char extra_data_size;
}IA_CORE_DECODER;


enum QUEUE_STEPS {
  QUEUE_DMPD,
  QUEUE_LD,
  QUEUE_SF,
  QUEUE_RG,
  QUEUE_STEP_MAX
};

#ifndef AUDIO_ELEMENT_START_ID
#define AUDIO_ELEMENT_START_ID 10
#endif

typedef struct DemixingInfo {
  uint32_t buffersize;
  uint32_t entries;
  uint8_t dmixp_mode[8];
  uint32_t dmixp_mode_count;
  uint32_t dmixp_mode_ponter;// last mode

  uint32_t *dmixp_mode_group[2];
  uint32_t dmixp_mode_group_size;
}DemixingInfo;

typedef struct ChannelBasedEnc {
  uint32_t input_sample_rate;
  int frame_size;

  IAChannelLayoutType layout_in;

  uint32_t recon_gain_flag;
  uint32_t output_gain_flag;
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

  DemixingInfo demixing_info;
}ChannelBasedEnc;

typedef struct SceneBasedEnc {
  //TODO
  uint32_t input_sample_rate;
  int frame_size;
  AmbisonicsConfig ambisonics_config;
}SceneBasedEnc;

typedef struct AudioElementEncoder {
  AudioElementType element_type;
  int element_id;


  uint32_t num_substreams;
  uint32_t audio_substream_id[22];

  uint32_t num_parameters;
  uint32_t param_definition_type[22];
  ParamDefinition param_definition[22];


  uint32_t input_sample_rate;
  int channels;
  int frame_size;
  int preskip_size;
  int codec_id;

  IA_CORE_ENCODER ia_core_encoder[IA_CHANNEL_LAYOUT_COUNT];
  //below interfaces are defined for encoding to get encoded frame
  uint8_t *samples;
  int(*encode_init)(AudioElementEncoder *st);
  int(*encode_ctl)(AudioElementEncoder *, int, va_list);
  int(*encode_frame)(AudioElementEncoder *, int, int, int, int16_t *, unsigned char*);
  int(*encode_close)(AudioElementEncoder *st);
  int initial_padding;

  //below interfaces are defined for encoding to get recon gain value
  uint8_t *samples2;
  int(*encode_init2)(AudioElementEncoder *st);
  int(*encode_ctl2)(AudioElementEncoder *, int, va_list);
  int(*encode_frame2)(AudioElementEncoder *, int, int, int, int16_t *, unsigned char*);
  int(*encode_close2)(AudioElementEncoder *st);

  IA_CORE_DECODER ia_core_decoder[IA_CHANNEL_LAYOUT_COUNT];
  int(*decode_init)(AudioElementEncoder *st);
  int(*decode_frame)(AudioElementEncoder *, int, int, int, unsigned char*, int, int16_t *);
  int(*decode_close)(AudioElementEncoder *st);

  int channel_groups; //1: Non-Scalable format, >1: Scalable format
  
  int size_of_audio_element_obu;
  int size_of_audio_frame_obu;
  int size_of_parameter_demixing;
  int size_of_parameter_recon_gain;
  unsigned char* audio_element_obu;
  unsigned char* audio_frame_obu;
  unsigned char* parameter_demixing_obu;
  unsigned char* parameter_recon_gain_obu;

  union {
    ChannelBasedEnc channel_based_enc;
    SceneBasedEnc scene_based_enc;
  };
  int redundant_copy;
  struct AudioElementEncoder *next;
}AudioElementEncoder;


struct IAMF_Encoder {
  uint32_t input_sample_rate;
  int bits_per_sample;
  int frame_size;
  int preskip_size;
  int codec_id;
  AudioElementEncoder *audio_element_enc;
  ObuNode *root_node;
  //int codec_config_id;

  DescriptorConfig descriptor_config;
  int is_descriptor_changed;

  SyncSyntax sync_syntax;
  int need_place_sync;

  int is_standalone;
} ;


LoudGainMeasure * immersive_audio_encoder_loudgain_create(const unsigned char *channel_layout_map, int sample_rate, int frame_size);
int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout);
int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int begin_ch, int nch);
int immersive_audio_encoder_loudgain_destory(LoudGainMeasure *lm);
#endif /*IAMF_ENCODER_PRIVATE_H*/
