#ifndef _IMMERSIVE_AUDIO_UTILS_H_
#define _IMMERSIVE_AUDIO_UTILS_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "immersive_audio.h"
#include "immersive_audio_types.h"

#define IA_CH_CATE_SURROUND     0x100
#define IA_CH_CATE_WEIGHT       0X200
#define IA_CH_CATE_TOP          0X400

void* ia_mallocz(uint32_t size);
void  ia_freep(void** p);

int ia_codec_check (IACodecID cid);
const char* ia_codec_name (IACodecID cid);

const char* ia_error_code_string (IAErrCode ec);

int ia_channel_layout_type_check (IAChannelLayoutType type);
const char* ia_channel_layout_name (IAChannelLayoutType type);
int ia_channel_layout_get_channels_count (IAChannelLayoutType type);
int ia_channel_layout_get_channels (IAChannelLayoutType type,
                                    IAChannel *channels, uint32_t count);
int ia_channel_layout_get_category_channels_count (IAChannelLayoutType type,
                                                   uint32_t categorys);

int ia_audio_layer_get_channels (IAChannelLayoutType type,
                                 IAChannel *channels, uint32_t count);
const char* ia_channel_name (IAChannel ch);

int leb128_read (uint8_t *data, int32_t len, uint64_t* size);
int bit1_count (uint32_t value);

#endif /* _IMMERSIVE_AUDIO_UTILS_H_ */
