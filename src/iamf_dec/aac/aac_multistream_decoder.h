#ifndef _AAC_MULTISTREAM2_DECODER_H_
#define _AAC_MULTISTREAM2_DECODER_H_

#include <stdint.h>

typedef struct AACMSDecoder AACMSDecoder;

AACMSDecoder *aac_multistream_decoder_open (uint8_t *config, uint32_t size,
        int streams, int coupled_streams,
        uint32_t flags, int *error);

int aac_multistream_decode_list (AACMSDecoder *st,
                                 uint8_t *buffer[], uint32_t len[],
                                 void *pcm, uint32_t frame_size);

void aac_multistream_decoder_close (AACMSDecoder *st);

#endif /* _AAC_MULTISTREAM2_DECODER_H_ */
