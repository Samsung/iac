#ifndef IAMF_CODEC_H_
#define IAMF_CODEC_H_

#include <stdint.h>

#include "IAMF_defines.h"


typedef struct IACodecContext {

    void       *priv;

    uint8_t    *cspec;
    uint32_t    clen;

    uint16_t    delay;
    uint32_t    sample_rate;
    uint32_t    sample_size;
    uint32_t    channel_mapping_family;

    uint8_t     streams;
    uint8_t     coupled_streams;
    uint8_t     channels;
} IACodecContext;

typedef struct IACodec {
    IACodecID cid;
    uint32_t  flags;
    uint32_t  priv_size;
    IAErrCode (*init) (IACodecContext *ths);
    int       (*decode_list) (IACodecContext *ths,
                              uint8_t *buf[], uint32_t len[], uint32_t count,
                              void *pcm, const uint32_t frame_size);
    IAErrCode (*close) (IACodecContext *ths);
} IACodec;


#endif /* IAMF_CODEC_H_ */
