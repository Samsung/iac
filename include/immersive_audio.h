#ifndef _IMMERSIVE_AUDIO_H_
#define _IMMERSIVE_AUDIO_H_


/**
 * Codec ID
 * */

typedef enum {
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
} IAErrCode;


/**
 * IA channel layout type.
 * */

typedef enum {
    IA_CH_LAYOUT_TYPE_INVALID = -1,
    IA_CH_LAYOUT_TYPE_MONO = 0,
    IA_CH_LAYOUT_TYPE_STEREO,
    IA_CH_LAYOUT_TYPE_5_1,
    IA_CH_LAYOUT_TYPE_5_1_2,
    IA_CH_LAYOUT_TYPE_5_1_4,
    IA_CH_LAYOUT_TYPE_7_1,
    IA_CH_LAYOUT_TYPE_7_1_2,
    IA_CH_LAYOUT_TYPE_7_1_4,
    IA_CH_LAYOUT_TYPE_3_1_2,
    IA_CH_LAYOUT_TYPE_COUNT
} IAChannelLayoutType;

enum {
    //  codec specific flag
    IA_FLAG_CODEC_CONFIG_ISOBMFF        = 0x1,
    IA_FLAG_CODEC_CONFIG_RAW            = 0x2,

    IA_FLAG_STATIC_META_RAW             = 0x100,
    IA_FLAG_EXTERNAL_DMX_INFO           = 0x1000,

    IA_FLAG_SUBSTREAM_CODEC_SPECIFIC    = 0x10000,
};

#endif /* _IMMERSIVE_AUDIO_H_ */
