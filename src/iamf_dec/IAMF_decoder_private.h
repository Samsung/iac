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
 * @file IAMF_decoder_private.h
 * @brief IAMF decoder internal APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DECODER_PRIVATE_H
#define IAMF_DECODER_PRIVATE_H

#include <stdint.h>

#include "IAMF_OBU.h"
#include "IAMF_core_decoder.h"
#include "IAMF_defines.h"
#include "IAMF_types.h"
#include "ae_rdr.h"
#include "audio_effect_peak_limiter.h"
#include "demixer.h"
#include "speex_resampler.h"

#define IAMF_FLAGS_MAGIC_CODE 0x01
#define IAMF_FLAGS_CONFIG 0x02
#define IAMF_FLAGS_RECONFIG 0x04

#define DEC_BUF_CNT 3

typedef enum {
  IA_CH_GAIN_RTF,
  IA_CH_GAIN_LTF,
  IA_CH_GAIN_RS,
  IA_CH_GAIN_LS,
  IA_CH_GAIN_R,
  IA_CH_GAIN_L,
  IA_CH_GAIN_COUNT
} IAOutputGainChannel;

/* >>>>>>>>>>>>>>>>>> DATABASE >>>>>>>>>>>>>>>>>> */

typedef void (*IAMF_Free)(void *);

typedef struct ObjectSet {
  void **items;
  int count;
  int capacity;
  IAMF_Free objFree;
} ObjectSet;

typedef struct MixGainUnit {
  int count;
  float constant_gain;
  float *gains;
} MixGainUnit;

typedef struct MixGain {
  float default_mix_gain;
  int nb_seg;
  MixGainUnit *mix_gain_uints;
} MixGain;

typedef struct ParameterItem {
  uint64_t id;
  uint64_t type;
  uint64_t timestamp;

  uint64_t parent_id;
  IAMF_Parameter *parameter;
  ParameterBase *param_base;

  MixGain *mix_gain;
} ParameterItem;

typedef struct ElementItem {
  uint64_t timestamp;
  uint64_t id;

  IAMF_CodecConf *codecConf;
  IAMF_Element *element;

  ParameterItem *demixing;
  ParameterItem *reconGain;
  ParameterItem *mixGain;

  uint32_t recon_gain_flags;
} ElementItem;

typedef struct SyncItem {
  uint64_t id;
  uint64_t start;
  int type;
} SyncItem;

typedef struct ElementViewer {
  ElementItem *items;
  int count;
} ElementViewer;

typedef struct ParameterViewer {
  ParameterItem **items;
  int count;
} ParameterViewer;

typedef struct SyncViewer {
  uint64_t start_global_time;
  SyncItem *items;
  int count;
} SyncViewer;

typedef struct IAMF_DataBase {
  IAMF_Object *version;
  IAMF_Object *sync;

  ObjectSet *codecConf;
  ObjectSet *element;
  ObjectSet *mixPresentation;
  ObjectSet *parameters;  // composed of ParameterObject.

  ElementViewer eViewer;
  ParameterViewer pViewer;
  SyncViewer sViewer;
} IAMF_DataBase;

/* <<<<<<<<<<<<<<<<<< DATABASE <<<<<<<<<<<<<<<<<< */

typedef struct LayoutInfo {
  IAMF_SP_LAYOUT sp;
  IAMF_Layout layout;
  int channels;
} LayoutInfo;

typedef struct IAMF_OutputGain {
  uint32_t flags;
  float gain;
} IAMF_OutputGain;

typedef struct IAMF_ReconGain {
  uint16_t flags;
  uint16_t nb_channels;
  float recon_gain[IA_CH_RE_COUNT];
  IAChannel order[IA_CH_RE_COUNT];
} IAMF_ReconGain;

typedef struct SubLayerConf {
  uint8_t layout;
  uint8_t nb_channels;
  uint8_t nb_substreams;
  uint8_t nb_coupled_substreams;
  float loudness;
  IAMF_OutputGain *output_gain;
  IAMF_ReconGain *recon_gain;
} SubLayerConf;

typedef struct ChannelLayerContext {
  int nb_layers;
  SubLayerConf *conf_s;

  int layer;
  int layout;
  int channels;
  IAChannel channels_order[IA_CH_LAYOUT_MAX_CHANNELS];

  int dmx_mode;
  int recon_gain_flags;
} ChannelLayerContext;

typedef struct AmbisonicsContext {
  int mode;
  uint8_t *mapping;
  int mapping_size;
} AmbisonicsContext;

typedef struct IAMF_Stream {
  uint64_t element_id;
  uint64_t codecConf_id;
  IACodecID codec_id;

  int sampling_rate;
  uint8_t scheme;  // audio element type: 0, CHANNEL_BASED; 1, SCENE_BASED

  int nb_channels;
  int nb_substreams;
  int nb_coupled_substreams;

  LayoutInfo *final_layout;
  void *priv;

  uint64_t timestamp;  // sync time

  uint64_t trimming_start;
  uint64_t trimming_end;
} IAMF_Stream;

typedef struct ScalableChannelDecoder {
  int nb_layers;
  IACoreDecoder **sub_decoders;
  Demixer *demixer;
} ScalableChannelDecoder;

typedef struct AmbisonicsDecoder {
  IACoreDecoder *decoder;
} AmbisonicsDecoder;

#define IAMF_PACKET_EXTRA_DEMIX_MODE 0x01
#define IAMF_PACKET_EXTRA_RECON_GAIN 0x02
#define IAMF_PACKET_EXTRA_MIX_GAIN 0x04

typedef struct Packet {
  uint32_t pmask;

  uint8_t **sub_packets;
  uint32_t *sub_packet_sizes;
  uint32_t nb_sub_packets;
  uint32_t count;

  uint64_t strim;
  uint64_t etrim;

  uint32_t uflags;

  uint64_t dts;
} Packet;

typedef struct IAMF_StreamDecoder {
  union {
    ScalableChannelDecoder *scale;
    AmbisonicsDecoder *ambisonics;
  };

  IAMF_Stream *stream;
  float *buffers[DEC_BUF_CNT];

  Packet packet;
  uint32_t frame_size;

} IAMF_StreamDecoder;

typedef struct IAMF_Mixer {
  uint64_t *element_ids;
  int *trimming_start;
  int *trimming_end;
  int nb_elements;
  int channels;
  int samples;
  float **frames;
  int count;
  int enable_mix;
} IAMF_Mixer;

typedef struct IAMF_Presentation {
  IAMF_MixPresentation *obj;

  IAMF_Stream **streams;
  uint32_t nb_streams;
  IAMF_StreamDecoder **decoders;
  IAMF_StreamDecoder *prepared_decoder;
  SpeexResamplerState *resampler;

  IAMF_Mixer mixer;
} IAMF_Presentation;

typedef struct IAMF_DecoderContext {
  IAMF_DataBase db;
  uint32_t flags;

  LayoutInfo *output_layout;
  int output_samples;

  IAMF_Presentation *presentation;
  char *mix_presentation_label;

  float loudness;
  AudioEffectPeakLimiter limiter;
  float threshold_db;

  // PTS
  uint32_t duration;
  uint32_t pts;
  uint32_t pts_time_base;
  uint32_t last_frame_size;

  uint64_t global_time;
  uint32_t time_precision;

  IAMF_extradata metadata;

} IAMF_DecoderContext;

struct IAMF_Decoder {
  IAMF_DecoderContext ctx;
};

#endif /* IAMF_DECODER_PRIVATE_H */
