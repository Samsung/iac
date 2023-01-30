#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "opus.h"
#include "IAMF_types.h"
#include "IAMF_debug.h"
#include "opus_multistream2_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "OPUSM2"

typedef void (*opus_copy_channels_out_func)(void *dst, const float *src,
        int frame_size, int channels);

struct OpusMS2Decoder {
    uint32_t    flags;

    int         streams;
    int         coupled_streams;

    void       *buffer;
};

static inline int align(int i)
{
    struct foo {
        char c;
        union {
            void *p;
            opus_int32 i;
            float v;
        } u;
    };
    unsigned int alignment = offsetof(struct foo, u);
    return ((i + alignment - 1) / alignment) * alignment;
}


static opus_int32 opus_ms2_decoder_get_size(int nb_streams,
        int nb_coupled_streams)
{
    int coupled_size;
    int mono_size;

    coupled_size = opus_decoder_get_size(2);
    mono_size = opus_decoder_get_size(1);
    return align(sizeof(OpusMS2Decoder))
           + nb_coupled_streams * align(coupled_size)
           + (nb_streams-nb_coupled_streams) * align(mono_size);
}

static int opus_ms2_decoder_init(OpusMS2Decoder *st, opus_int32 Fs,
                                 int streams, int coupled_streams,
                                 uint32_t flags)
{
    int coupled_size;
    int mono_size;
    int i, ret;
    char *ptr;

    st->streams = streams;
    st->coupled_streams = coupled_streams;
    st->flags = flags;

    ptr = (char *)st + align(sizeof(OpusMS2Decoder));
    coupled_size = opus_decoder_get_size(2);
    mono_size = opus_decoder_get_size(1);

    for (i=0; i<st->coupled_streams; i++) {
        ret=opus_decoder_init((OpusDecoder *)ptr, Fs, 2);
        if(ret!=OPUS_OK) {
            return ret;
        }
        ptr += align(coupled_size);
    }
    for (; i<st->streams; i++) {
        ret=opus_decoder_init((OpusDecoder *)ptr, Fs, 1);
        if(ret!=OPUS_OK) {
            return ret;
        }
        ptr += align(mono_size);
    }
    return OPUS_OK;
}


static int opus_ms2_decode_list_native(OpusMS2Decoder *st,
                                       uint8_t *buffer[],
                                       opus_int32 size[],
                                       void *pcm,
                                       int frame_size,
                                       opus_copy_channels_out_func copy_channel_out)
{
    int coupled_size;
    int mono_size;
    int s, ret = 0;
    char *ptr;
    char *p = (char *)pcm;

    if (frame_size <= 0) {
        return OPUS_BAD_ARG;
    }

    long ts = sizeof(short);
    if (st->flags & AUDIO_FRAME_FLOAT) {
        ts = sizeof(float);
    }

    if (!st->buffer) {
        void *buf = 0;
        buf = malloc (ts * 2 * frame_size);
        if (!buf) {
            return OPUS_ALLOC_FAIL;
        }
        st->buffer = buf;
    }

    ptr = (char *)st + align(sizeof(OpusMS2Decoder));
    coupled_size = opus_decoder_get_size(2);
    mono_size = opus_decoder_get_size(1);

    for (s=0; s<st->streams; s++) {
        OpusDecoder *dec;

        dec = (OpusDecoder *)ptr;
        ptr += (s < st->coupled_streams) ? align(coupled_size) : align(mono_size);

        if (st->flags & AUDIO_FRAME_FLOAT) {
            ret = opus_decode_float(dec, buffer[s], size[s], st->buffer, frame_size, 0);
        } else {
            ret = opus_decode(dec, buffer[s], size[s], st->buffer, frame_size, 0);
        }

        ia_logt("stream %d decoded result %d", s, ret);
        if (ret <= 0) {
            return ret;
        }
        frame_size = ret;
        if (s < st->coupled_streams) {
            (*copy_channel_out)((void *)p, st->buffer, ret, 2);
            p += (ts * ret * 2);
        } else {
            (*copy_channel_out)((void *)p, st->buffer, ret, 1);
            p += (ts * ret);
        }
    }

    return ret;
}

void opus_copy_channel_out_float_plane(void *dst, const float *src,
                                       int frame_size, int channels)
{
    ia_logt("copy frame %d, channels %d  dst %p src %p.", frame_size, channels, dst,
            src);
    if (channels == 1) {
        memcpy (dst, src, sizeof(float) * frame_size);
    } else if (channels == 2) {
        float *pcm = (float *)dst;
        for (int s=0; s<frame_size; ++s) {
            pcm[s] = src[channels * s];
            pcm[s + frame_size] = src[channels * s + 1];
        }
    }
}

OpusMS2Decoder *opus_multistream2_decoder_create (int Fs,
        int streams,
        int coupled_streams,
        uint32_t flags,
        int *error)
{
    int ret;
    OpusMS2Decoder *st;
    int size;
    if ((coupled_streams>streams) || (streams<1) || (coupled_streams<0) ||
            (streams>255-coupled_streams)) {
        if (error) {
            *error = OPUS_BAD_ARG;
        }
        return NULL;
    }

    size = opus_ms2_decoder_get_size(streams, coupled_streams);
    st = (OpusMS2Decoder *) malloc (size);
    if (st==NULL) {
        if (error) {
            *error = OPUS_ALLOC_FAIL;
        }
        return NULL;
    }
    memset (st, 0, size);

    ret = opus_ms2_decoder_init(st, Fs, streams, coupled_streams, flags);
    if (error) {
        *error = ret;
    }
    if (ret != OPUS_OK) {
        free(st);
        st = NULL;
    }
    return st;
}


int opus_multistream2_decode_list (OpusMS2Decoder *st,
                                   uint8_t *buffer[], uint32_t size[],
                                   void *pcm, uint32_t frame_size)
{
    if (st->flags & AUDIO_FRAME_PLANE && st->flags & AUDIO_FRAME_FLOAT)
        return opus_ms2_decode_list_native (st, buffer, (opus_int32 *)size, pcm,
                                            frame_size,
                                            opus_copy_channel_out_float_plane);
    else {
        ia_logt("test");
        return OPUS_UNIMPLEMENTED;
    }
}

void opus_multistream2_decoder_destroy (OpusMS2Decoder *st)
{
    if (st->buffer) {
        free (st->buffer);
    }
    free (st);
}
