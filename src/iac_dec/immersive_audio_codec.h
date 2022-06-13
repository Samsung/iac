#ifndef _IMMERSIVE_AUDIO_CODEC_H_
#define _IMMERSIVE_AUDIO_CODEC_H_

#include <stdint.h>

#include "immersive_audio.h"


typedef struct IACodecContext {
    uint32_t    flags;

    void*       priv;

    uint8_t    *cspec;
    uint32_t    clen;

    uint16_t    delay;
    uint32_t    sample_rate;
    uint32_t    channel_mapping_family;

    uint8_t     ambisonics;

    uint8_t     streams;
    uint8_t     coupled_streams;
    uint8_t     channels;
} IACodecContext;

typedef struct IACodec {
    IACodecID cid;
    uint32_t  flags;
    uint32_t  priv_size;
    IAErrCode (*init) (IACodecContext *ths);
    IAErrCode (*init_final) (IACodecContext *ths);
    int       (*decode_list) (IACodecContext *ths,
                              uint8_t *buf[], uint32_t len[], uint32_t count,
                              void* pcm, const uint32_t frame_size);
    IAErrCode (*close) (IACodecContext *ths);
} IACodec;


#endif /* _IMMERSIVE_AUDIO_CODEC_H_ */
