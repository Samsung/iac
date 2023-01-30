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
    NOT_DEFINED = 0,
    LOUDSPEAKERS_SP_LABEL = 1,
    LOUDSPEAKERS_SS_CONVENTION = 2,
    BINAURAL = 3,

    TARGET_LAYOUT_TYPE_NOT_DEFINED = 0,
    TARGET_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL,
    TARGET_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION,
    TARGET_LAYOUT_TYPE_BINAURAL,

    IAMF_LAYOUT_TYPE_NOT_DEFINED = 0,
    IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL,
    IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION,
    IAMF_LAYOUT_TYPE_BINAURAL,
} IAMF_LayoutType;


typedef enum IAMF_SoundSystem {
  SOUND_SYSTEM_A,  // 0+2+0, 0
  SOUND_SYSTEM_B,  // 0+5+0, 1
  SOUND_SYSTEM_C,  // 2+5+0, 1
  SOUND_SYSTEM_D,  // 4+5+0, 1
  SOUND_SYSTEM_E,  // 4+5+1, 1
  SOUND_SYSTEM_F,  // 3+7+0, 2
  SOUND_SYSTEM_G,  // 4+9+0, 1
  SOUND_SYSTEM_H,  // 9+10+3, 2
  SOUND_SYSTEM_I,  // 0+7+0, 1
  SOUND_SYSTEM_J,  // 4+7+0, 1
  SOUND_SYSTEM_EXT_712, // 2+7+0, 1
  SOUND_SYSTEM_EXT_312, // 2+3+0, 1
} IAMF_SoundSystem;

typedef enum IAMF_ParameterType {
    PARAMETER_TYPE_MIX_GAIN,
    PARAMETER_TYPE_DEMIXING,
    PARAMETER_TYPE_RECON_GAIN,

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
            uint8_t     num_loudspeakers:6;
            uint8_t     type:2;
            uint8_t    *sp_label;
        } sp_labels;

        struct {
            uint8_t     reserved:2;
            uint8_t     sound_system:4;
            uint8_t     type:2;
        } sound_system;

        struct {
            uint8_t     reserved:6;
            uint8_t     type:2;
        } binaural;

        struct {
            uint8_t     reserved:6;
            uint8_t     type:2;
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
    IA_CODEC_UNKNOWN,
    IA_CODEC_OPUS,
    IA_CODEC_AAC,
    IA_CODEC_PCM,
    IA_CODEC_COUNT
} IACodecID;


/**
 * IA error codes.
 * */

typedef enum {
    IA_OK                       =    0,
    IA_ERR_BAD_ARG              =   -1,
    IA_ERR_BUFFER_TOO_SMALL     =   -2,
    IA_ERR_INTERNAL             =   -3,
    IA_ERR_INVALID_PACKET       =   -4,
    IA_ERR_INVALID_STATE        =   -5,
    IA_ERR_UNIMPLEMENTED        =   -6,
    IA_ERR_ALLOC_FAIL           =   -7,
    IA_ERR_NOT_ENOUGH_DATA      =   -8,

    IAMF_OK                     =    0,
    IAMF_ERR_BAD_ARG            =   -1,
    IAMF_ERR_BUFFER_TOO_SMALL   =   -2,
    IAMF_ERR_INTERNAL           =   -3,
    IAMF_ERR_INVALID_PACKET     =   -4,
    IAMF_ERR_INVALID_STATE      =   -5,
    IAMF_ERR_UNIMPLEMENTED      =   -6,
    IAMF_ERR_ALLOC_FAIL         =   -7,
    IAMF_ERR_NEED_MORE_DATA     =   -8,

} IAErrCode;


/**
 * IA channel layout type.
 * */

typedef enum {
    IA_CHANNEL_LAYOUT_INVALID = -1,
    IA_CHANNEL_LAYOUT_MONO = 0,    //1.0.0
    IA_CHANNEL_LAYOUT_STEREO,      //2.0.0
    IA_CHANNEL_LAYOUT_510,         //5.1.0
    IA_CHANNEL_LAYOUT_512,         //5.1.2
    IA_CHANNEL_LAYOUT_514,         //5.1.4
    IA_CHANNEL_LAYOUT_710,         //7.1.0
    IA_CHANNEL_LAYOUT_712,         //7.1.2
    IA_CHANNEL_LAYOUT_714,         //7.1.4
    IA_CHANNEL_LAYOUT_312,         //3.1.2
    IA_CHANNEL_LAYOUT_BINAURAL,    //binaural
    IA_CHANNEL_LAYOUT_COUNT
} IAChannelLayoutType;

enum {
    //  codec specific flag
    IA_FLAG_CODEC_CONFIG_ISOBMFF        = 0x1,
    IA_FLAG_CODEC_CONFIG_RAW            = 0x2,

    IA_FLAG_STATIC_META_RAW             = 0x100,
    IA_FLAG_EXTERNAL_DMX_INFO           = 0x1000,

    IA_FLAG_SUBSTREAM_CODEC_SPECIFIC    = 0x10000,
};


////////////////////////// OPUS and AAC codec control/////////////////////////////////////
#define __ia_check_int(x) (((void)((x) == (int32_t)0)), (int32_t)(x))
#define __ia_check_int_ptr(ptr) ((ptr) + ((ptr) - (int32_t*)(ptr)))
//#define __ia_check_void_ptr(ptr) ((ptr) + ((ptr) - (void*)(ptr)))
//#define __ia_check_void_ptr(ptr) (void*)(ptr)

#define IA_BANDWIDTH_FULLBAND              1105 /**<20 kHz bandpass @hideinitializer*/

#define IA_APPLICATION_AUDIO               2049

#define IA_SET_BITRATE_REQUEST             4002
#define IA_SET_BANDWIDTH_REQUEST           4008
#define IA_SET_VBR_REQUEST                 4006
#define IA_SET_COMPLEXITY_REQUEST          4010
#define IA_GET_LOOKAHEAD_REQUEST           4027

#define IA_SET_RECON_GAIN_FLAG_REQUEST         3000
#define IA_SET_OUTPUT_GAIN_FLAG_REQUEST         3001
#define IA_SET_SCALE_FACTOR_MODE_REQUEST         3002
#define IA_SET_STANDALONE_REPRESENTATION_REQUEST         3003




#define IA_SET_BITRATE(x) IA_SET_BITRATE_REQUEST, __ia_check_int(x)
#define IA_SET_BANDWIDTH(x) IA_SET_BANDWIDTH_REQUEST, __ia_check_int(x)
#define IA_SET_VBR(x) IA_SET_VBR_REQUEST, __ia_check_int(x)
#define IA_SET_COMPLEXITY(x) IA_SET_COMPLEXITY_REQUEST, __ia_check_int(x)
#define IA_GET_LOOKAHEAD(x) IA_GET_LOOKAHEAD_REQUEST, __ia_check_int_ptr(x)

#define IA_SET_RECON_GAIN_FLAG(x) IA_SET_RECON_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_OUTPUT_GAIN_FLAG(x) IA_SET_OUTPUT_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_SCALE_FACTOR_MODE(x) IA_SET_SCALE_FACTOR_MODE_REQUEST, __ia_check_int(x)
#define IA_SET_STANDALONE_REPRESENTATION(x) IA_SET_STANDALONE_REPRESENTATION_REQUEST, __ia_check_int(x)



#endif /* IAMF_DEFINES_H */
