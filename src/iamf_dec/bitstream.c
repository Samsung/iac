#include <assert.h>
#include <string.h>

#include "bitstream.h"

int32_t
bs(BitStream *b, const uint8_t *data, int size)
{
    b->data = data;
    b->size = size;
    b->b8sp = b->b8p = 0;

    return 0;
}

static uint32_t
bs_getLastA32b(BitStream *b)
{
    uint32_t ret = 0;

    for (int i=0; i<4; ++i) {
        ret <<= INT8_BITS;
        ret |= b->data[b->b8sp + i];
    }

    return ret;
}

uint32_t
bs_get32b(BitStream *b, int n)
{
    uint32_t ret = 0;
    uint32_t nb8p = 0, nn;

    assert (n <= INT32_BITS);

    ret = bs_getLastA32b(b);
    if (n + b->b8p > INT32_BITS) {
        nb8p = n + b->b8p - INT32_BITS;
        nn = INT32_BITS - b->b8p;
    } else {
        nn = n;
    }

    ret >>= INT32_BITS - nn - b->b8p;
    if (nn < INT32_BITS) {
        ret &= ~((~0) << nn);
    }
    b->b8p += nn;
    b->b8sp += (b->b8p / INT8_BITS);
    b->b8p %= INT8_BITS;

    if (nb8p) {
        uint32_t nret = bs_get32b(b, nb8p);
        ret <<= nb8p;
        ret |= nret;
    }

    return ret;
}

int32_t
bs_skip(BitStream *b, int n)
{
    b->b8p += n;
    b->b8sp += (b->b8p / INT8_BITS);
    b->b8p %= INT8_BITS;

    return 0;
}


void
bs_align(BitStream *b)
{
    if (b->b8p) {
        ++b->b8sp;
        b->b8p = 0;
    }
}

uint32_t bs_getA8b(BitStream *b)
{
    uint32_t ret;

    bs_align(b);
    ret = b->data[b->b8sp];
    ++b->b8sp;
    return ret;
}

uint32_t bs_getA16b(BitStream *b)
{
    uint32_t ret = bs_getA8b(b);
    ret <<= INT8_BITS;
    ret |= bs_getA8b(b);
    return ret;
}

uint32_t bs_getA32b(BitStream *b)
{
    uint32_t ret = bs_getA16b(b);
    ret <<= INT16_BITS;
    ret |= bs_getA16b(b);
    return ret;
}

uint64_t bs_getAleb128(BitStream *b)
{
    uint64_t    ret = 0;
    uint32_t    i;
    uint8_t     byte;

    bs_align(b);

    for (i = 0; i < 8; i++ ) {
        byte = b->data[b->b8sp + i];
        ret |= ( ((uint64_t)byte & 0x7f) << (i*7) );
        if ( !(byte & 0x80) ) {
            break;
        }
    }
    ++i;
    b->b8sp += i;
    return ret;

}

int bs_getAsleb128i32(BitStream *b)
{
    uint32_t    ret = 0;
    int         i, val = 0;
    uint8_t     byte;

    bs_align(b);

    for (i = 0; i < 4; i++ ) {
        byte = b->data[b->b8sp + i];
        ret |= ( ((uint32_t)byte & 0x7f) << (i*7) );
        if ( !(byte & 0x80) ) {
            val = (int)ret;
            val = val << (32 - i*7) >> (32 - i*7);
            break;
        }
    }
    ++i;
    b->b8sp += i;
    return val;
}

int32_t  bs_read(BitStream *b, uint8_t *data, int n)
{
    bs_align(b);
    memcpy (data, &b->data[b->b8sp], n);
    b->b8sp += n;
    return n;
}

int32_t  bs_readString(BitStream *b, char *data, int n)
{
    int len = 0;
    bs_align(b);
    len = strlen((char *)&b->data[b->b8sp]) + 1;
    memcpy (data, &b->data[b->b8sp], len);
    b->b8sp += len;
    return len;
}

uint32_t  bs_tell(BitStream *b)
{
    return b->b8p ? b->b8sp + 1 : b->b8sp;
}


