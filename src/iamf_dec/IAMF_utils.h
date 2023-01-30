#ifndef IAMF_UITLS_H
#define IAMF_UITLS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "IAMF_defines.h"
#include "IAMF_types.h"

#define IA_CH_CATE_SURROUND     0x100
#define IA_CH_CATE_WEIGHT       0X200
#define IA_CH_CATE_TOP          0X400

#define IAMF_MALLOC(type, n) ((type *) malloc (sizeof (type) * (n)))
#define IAMF_REALLOC(type, p, n) ((type *) realloc (p, sizeof (type) * (n)))
#define IAMF_MALLOCZ(type, n) ((type *) iamf_mallocz (sizeof (type) * (n)))

void *ia_mallocz(uint32_t size);
void  ia_freep(void **p);

void *iamf_mallocz(uint32_t size);
void  iamf_freep(void **p);

int ia_codec_check (IACodecID cid);
const char *ia_codec_name (IACodecID cid);

const char *ia_error_code_string (IAErrCode ec);

int ia_channel_layout_type_check (IAChannelLayoutType type);
const char *ia_channel_layout_name (IAChannelLayoutType type);
int ia_channel_layout_get_channels_count (IAChannelLayoutType type);
int ia_channel_layout_get_channels (IAChannelLayoutType type,
                                    IAChannel *channels, uint32_t count);
int ia_channel_layout_get_category_channels_count (IAChannelLayoutType type,
        uint32_t categorys);

int ia_audio_layer_get_channels (IAChannelLayoutType type,
                                 IAChannel *channels, uint32_t count);
const char *ia_channel_name (IAChannel ch);

int bit1_count (uint32_t value);

#endif /* IAMF_UITLS_H */
