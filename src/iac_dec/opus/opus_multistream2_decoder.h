#ifndef _OPUS_MULTISTREAM2_DECODER_H_
#define _OPUS_MULTISTREAM2_DECODER_H_

#include <stdint.h>

typedef struct OpusMS2Decoder OpusMS2Decoder;

OpusMS2Decoder *opus_multistream2_decoder_create (int Fs, int streams,
                                                  int coupled_streams,
                                                  uint32_t flags, int *error);

int opus_multistream2_decode_list (OpusMS2Decoder *st,
                                   uint8_t* buffer[], uint32_t len[],
                                   void *pcm, uint32_t frame_size);

void opus_multistream2_decoder_destroy (OpusMS2Decoder *st);

#endif /* _OPUS_MULTISTREAM2_DECODER_H_ */
