#ifndef _IMMERSIVE_AUDIO_CORE_DECODER_H_
#define _IMMERSIVE_AUDIO_CORE_DECODER_H_

#include <stdint.h>
#include "immersive_audio.h"

typedef struct IACoreDecoder IACoreDecoder;

IACoreDecoder* ia_core_decoder_open (IACodecID cid);
void ia_core_decoder_close (IACoreDecoder* ths);

IAErrCode ia_core_decoder_init (IACoreDecoder* ths,
                                uint8_t* spec, uint32_t len, uint32_t flags);
IAErrCode ia_core_decoder_set_streams_info (IACoreDecoder* ths,
                                            uint8_t streams,
                                            uint8_t coupled_streams);

int ia_core_decoder_decode_list (IACoreDecoder* ths,
                            uint8_t* buffers[], uint32_t *sizes, uint32_t count,
                            float* out, uint32_t frame_size);

#endif /* _IMMERSIVE_AUDIO_CORE_DECODER_H_ */
