#ifndef _A2B_ENDIAN_H
#define _A2B_ENDIAN_H
#include <stdint.h>
static inline uint32_t bswap32(const uint32_t u32)
{
#ifndef WORDS_BIGENDIAN
#if defined (__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
	return __builtin_bswap32(u32);
#else
	return (u32 << 24) | ((u32 << 8) & 0xFF0000) | ((u32 >> 8) & 0xFF00) | (u32 >> 24);
#endif
#else
	return u32;
#endif
}

static inline uint16_t bswap16(const uint16_t u16)
{
#ifndef WORDS_BIGENDIAN
#if defined (__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)))
	return __builtin_bswap16(u16);
#else
	return (u16 << 8) | (u16 >> 8);
#endif
#else
	return u16;
#endif
}

static inline uint16_t bswap64(const uint64_t u64)
{
#ifndef WORDS_BIGENDIAN
#if defined (__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)))
	return __builtin_bswap16(u64);
#else
	return bswap32(u64) + 0ULL << 32 | bswap32(u64 >> 32);
#endif
#else
	return u64;
#endif
}

#define be64(x) bswap64(x)
#define be32(x) bswap32(x)
#define be16(x) bswap16(x)

#endif

