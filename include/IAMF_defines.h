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
 * @file IAMF_defines.h
 * @brief AMF Common defines
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef IAMF_DEFINES_H
#define IAMF_DEFINES_H

/**
 * Audio Element Type
 * */

typedef enum {
  AUDIO_ELEMENT_INVALID = -1,
  AUDIO_ELEMENT_CHANNEL_BASED,
  AUDIO_ELEMENT_SCENE_BASED,
  AUDIO_ELEMENT_COUNT
} AudioElementType;

typedef enum AmbisonicsMode {
  AMBISONICS_MONO,
  AMBISONICS_PROJECTION
} AmbisonicsMode;

typedef enum IAMF_LayoutType {
  IAMF_LAYOUT_TYPE_NOT_DEFINED = 0,
  IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL,
  IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION,
  IAMF_LAYOUT_TYPE_BINAURAL,
} IAMF_LayoutType;

typedef enum IAMF_SoundSystem {
  SOUND_SYSTEM_A,        // 0+2+0, 0
  SOUND_SYSTEM_B,        // 0+5+0, 1
  SOUND_SYSTEM_C,        // 2+5+0, 1
  SOUND_SYSTEM_D,        // 4+5+0, 1
  SOUND_SYSTEM_E,        // 4+5+1, 1
  SOUND_SYSTEM_F,        // 3+7+0, 2
  SOUND_SYSTEM_G,        // 4+9+0, 1
  SOUND_SYSTEM_H,        // 9+10+3, 2
  SOUND_SYSTEM_I,        // 0+7+0, 1
  SOUND_SYSTEM_J,        // 4+7+0, 1
  SOUND_SYSTEM_EXT_712,  // 2+7+0, 1
  SOUND_SYSTEM_EXT_312,  // 2+3+0, 1
} IAMF_SoundSystem;

typedef enum IAMF_ParameterType {
  IAMF_PARAMETER_TYPE_MIX_GAIN = 0,
  IAMF_PARAMETER_TYPE_DEMIXING,
  IAMF_PARAMETER_TYPE_RECON_GAIN,
} IAMF_ParameterType;

/**
 *  Layout Syntax:
 *
 *  class layout() {
 *    unsigned int (2) layout_type;
 *
 *    if (layout_type == LOUDSPEAKERS_SP_LABEL) {
 *      unsigned int (6) num_loudspeakers;
 *      for (i = 0; i < num_loudspeakers; i++) {
 *        unsigned int (8) sp_label;
 *      }
 *    } else if (layout_type == LOUDSPEAKERS_SS_CONVENTION) {
 *      unsigned int (4) sound_system;
 *      unsigned int (2) reserved;
 *    } else if (layout_type == BINAURAL or NOT_DEFINED) {
 *      unsigned int (6) reserved;
 *    }
 *  }
 *
 * */
typedef struct IAMF_Layout {
  union {
    struct {
      uint8_t num_loudspeakers : 6;
      uint8_t type : 2;
      uint8_t *sp_label;
    } sp_labels;

    struct {
      uint8_t reserved : 2;
      uint8_t sound_system : 4;
      uint8_t type : 2;
    } sound_system;

    struct {
      uint8_t reserved : 6;
      uint8_t type : 2;
    } binaural;

    struct {
      uint8_t reserved : 6;
      uint8_t type : 2;
    };
  };
} IAMF_Layout;

/**
 *
 *  Loudness Info Syntax:
 *
 *  class loudness_info() {
 *    unsigned int (8) info_type;
 *    signed int (16) integrated_loudness;
 *    signed int (16) digital_peak;
 *
 *    if (info_type & 1) {
 *      signed int (16) true_peak;
 *    }
 *  }
 *
 * */
typedef struct IAMF_LoudnessInfo {
  uint8_t info_type;
  int16_t integrated_loudness;
  int16_t digital_peak;
  int16_t true_peak;
} IAMF_LoudnessInfo;

/**
 * Codec ID
 * */
typedef enum {
  IAMF_CODEC_UNKNOWN = 0,
  IAMF_CODEC_OPUS,
  IAMF_CODEC_AAC,
  IAMF_CODEC_FLAC,
  IAMF_CODEC_PCM,
  IAMF_CODEC_COUNT
} IACodecID;

/**
 * Error codes.
 * */

enum {
  IAMF_OK = 0,
  IAMF_ERR_BAD_ARG = -1,
  IAMF_ERR_BUFFER_TOO_SMALL = -2,
  IAMF_ERR_INTERNAL = -3,
  IAMF_ERR_INVALID_PACKET = -4,
  IAMF_ERR_INVALID_STATE = -5,
  IAMF_ERR_UNIMPLEMENTED = -6,
  IAMF_ERR_ALLOC_FAIL = -7,
  IAMF_ERR_NEED_MORE_DATA = -8,
};

/**
 * IA channel layout type.
 * */

typedef enum {
  IA_CHANNEL_LAYOUT_INVALID = -1,
  IA_CHANNEL_LAYOUT_MONO = 0,  // 1.0.0
  IA_CHANNEL_LAYOUT_STEREO,    // 2.0.0
  IA_CHANNEL_LAYOUT_510,       // 5.1.0
  IA_CHANNEL_LAYOUT_512,       // 5.1.2
  IA_CHANNEL_LAYOUT_514,       // 5.1.4
  IA_CHANNEL_LAYOUT_710,       // 7.1.0
  IA_CHANNEL_LAYOUT_712,       // 7.1.2
  IA_CHANNEL_LAYOUT_714,       // 7.1.4
  IA_CHANNEL_LAYOUT_312,       // 3.1.2
  IA_CHANNEL_LAYOUT_BINAURAL,  // binaural
  IA_CHANNEL_LAYOUT_COUNT
} IAChannelLayoutType;

////////////////////////// OPUS and AAC codec
/// control/////////////////////////////////////
#define __ia_check_int(x) (((void)((x) == (int32_t)0)), (int32_t)(x))
#define __ia_check_int_ptr(ptr) ((ptr) + ((ptr) - (int32_t *)(ptr)))
//#define __ia_check_void_ptr(ptr) ((ptr) + ((ptr) - (void*)(ptr)))
//#define __ia_check_void_ptr(ptr) (void*)(ptr)

#define IA_BANDWIDTH_FULLBAND 1105 /**<20 kHz bandpass @hideinitializer*/

#define IA_APPLICATION_AUDIO 2049

#define IA_SET_BITRATE_REQUEST 4002
#define IA_SET_BANDWIDTH_REQUEST 4008
#define IA_SET_VBR_REQUEST 4006
#define IA_SET_COMPLEXITY_REQUEST 4010
#define IA_GET_LOOKAHEAD_REQUEST 4027

#define IA_SET_RECON_GAIN_FLAG_REQUEST 3000
#define IA_SET_OUTPUT_GAIN_FLAG_REQUEST 3001
#define IA_SET_SCALE_FACTOR_MODE_REQUEST 3002
#define IA_SET_STANDALONE_REPRESENTATION_REQUEST 3003

#define IA_SET_BITRATE(x) IA_SET_BITRATE_REQUEST, __ia_check_int(x)
#define IA_SET_BANDWIDTH(x) IA_SET_BANDWIDTH_REQUEST, __ia_check_int(x)
#define IA_SET_VBR(x) IA_SET_VBR_REQUEST, __ia_check_int(x)
#define IA_SET_COMPLEXITY(x) IA_SET_COMPLEXITY_REQUEST, __ia_check_int(x)
#define IA_GET_LOOKAHEAD(x) IA_GET_LOOKAHEAD_REQUEST, __ia_check_int_ptr(x)

#define IA_SET_RECON_GAIN_FLAG(x) \
  IA_SET_RECON_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_OUTPUT_GAIN_FLAG(x) \
  IA_SET_OUTPUT_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_SCALE_FACTOR_MODE(x) \
  IA_SET_SCALE_FACTOR_MODE_REQUEST, __ia_check_int(x)
#define IA_SET_STANDALONE_REPRESENTATION(x) \
  IA_SET_STANDALONE_REPRESENTATION_REQUEST, __ia_check_int(x)

#endif /* IAMF_DEFINES_H */
