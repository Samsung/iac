#ifndef IMMERSIVE_AUDIO_DEFINES_H
#define IMMERSIVE_AUDIO_DEFINES_H


/**
 * Codec ID
 * */

typedef enum {
    IA_CODEC_UNKNOWN,
    IA_CODEC_OPUS,
    IA_CODEC_AAC,
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


#define IA_SET_BITRATE(x) IA_SET_BITRATE_REQUEST, __ia_check_int(x)
#define IA_SET_BANDWIDTH(x) IA_SET_BANDWIDTH_REQUEST, __ia_check_int(x)
#define IA_SET_VBR(x) IA_SET_VBR_REQUEST, __ia_check_int(x)
#define IA_SET_COMPLEXITY(x) IA_SET_COMPLEXITY_REQUEST, __ia_check_int(x)
#define IA_GET_LOOKAHEAD(x) IA_GET_LOOKAHEAD_REQUEST, __ia_check_int_ptr(x)

#define IA_SET_RECON_GAIN_FLAG(x) IA_SET_RECON_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_OUTPUT_GAIN_FLAG(x) IA_SET_OUTPUT_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_SCALE_FACTOR_MODE(x) IA_SET_SCALE_FACTOR_MODE_REQUEST, __ia_check_int(x)

#endif /* IMMERSIVE_AUDIO_DEFINES_H */
