#ifndef BIT_STREAM_H
#define BIT_STREAM_H

#include <stdint.h>

#define INT8_BITS   8
#define INT16_BITS  16
#define INT32_BITS  32

typedef struct {
    const uint8_t   *data;
    uint32_t    size;
    uint32_t    b8sp; // bytes, less than size;
    uint32_t    b8p; // 0~7
} BitStream;

int32_t  bs(BitStream *b, const uint8_t *data, int size);
uint32_t bs_get32b(BitStream *b, int n);
int32_t  bs_skip(BitStream *b, int n);
void     bs_align(BitStream *b);
uint32_t bs_getA8b(BitStream *b);
uint32_t bs_getA16b(BitStream *b);
uint32_t bs_getA32b(BitStream *b);
uint64_t bs_getAleb128(BitStream *b);
int       bs_getAsleb128i32(BitStream *b);
int32_t  bs_read(BitStream *b, uint8_t *data, int n);
int32_t  bs_readString(BitStream *b, char *data, int n);
uint32_t  bs_tell(BitStream *b);

#endif /* BIT_STREAM_H */
