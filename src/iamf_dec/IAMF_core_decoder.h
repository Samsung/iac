#ifndef IAMF_CORE_DECODER_H_
#define IAMF_CORE_DECODER_H_

#include <stdint.h>
#include "IAMF_defines.h"

typedef struct IACoreDecoder IACoreDecoder;

IACoreDecoder *ia_core_decoder_open (IACodecID cid);
void ia_core_decoder_close (IACoreDecoder *ths);

IAErrCode ia_core_decoder_init (IACoreDecoder *ths);

IAErrCode ia_core_decoder_set_codec_conf (IACoreDecoder *ths,
        uint8_t *spec, uint32_t len);
IAErrCode ia_core_decoder_set_streams_info (IACoreDecoder *ths,
        uint32_t mode, uint8_t channels, uint8_t streams, uint8_t coupled_streams,
        uint8_t mapping[], uint32_t mapping_size);

int ia_core_decoder_decode_list (IACoreDecoder *ths,
                                 uint8_t *buffers[], uint32_t *sizes, uint32_t count,
                                 float *out, uint32_t frame_size);

#endif /* IAMF_CORE_DECODER_H_ */
