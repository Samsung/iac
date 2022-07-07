#ifndef _IMMERSIVE_AUDIO_OBU_H_
#define _IMMERSIVE_AUDIO_OBU_H_

#include <stdint.h>

#include "immersive_audio_defines.h"

typedef enum {
    IA_OBU_IA_STREAM_INDICATOR,
    IA_OBU_CODEC_SPECIFIC_INFO,
    IA_OBU_IA_STATIC_META,
    IA_OBU_TEMPORAL_DELIMITOR,
    IA_OBU_DEMIXING_INFO,
    IA_OBU_RECON_GAIN_INFO,
    IA_OBU_SUBSTREAM
} IAOBUType;

typedef struct IAOBU {
    IAOBUType   type;
    uint8_t    *payload;
    uint32_t    psize;
} IAOBU;

IAErrCode ia_obu_find_codec_specific_info (IAOBU *, uint8_t *, uint32_t);
IAErrCode ia_obu_find_static_meta (IAOBU *, uint8_t *, uint32_t);
IAErrCode ia_obu_find_demixing_info (IAOBU *, uint8_t *, uint32_t);

int ia_obu_stream_parse (IAOBU *, uint8_t *, uint32_t);

#endif /* _IMMERSIVE_AUDIO_OBU_H_ */
