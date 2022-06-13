#include <stdlib.h>
#include <string.h>

#include <aacdecoder_lib.h>

#include "immersive_audio_debug.h"
#include "immersive_audio_types.h"
#include "aac_multistream_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "AACMS"

struct AACMSDecoder {
    uint32_t    flags;
    int         streams;
    int         coupled_streams;

    HANDLE_AACDECODER *handles;
    INT_PCM     *buffer;
};

typedef void (*aac_copy_channel_out_func)(void *dst, const void *src,
                                          int frame_size, int channes);

void aac_copy_channel_out_plane(void *dst, const void *src,
                                int frame_size, int channels)
{
    if (channels == 1) {
        memcpy (dst, src, sizeof(INT_PCM) * frame_size);
    } else if (channels == 2) {
        INT_PCM *in, *out;
        in = (INT_PCM *)src;
        out = (INT_PCM *)dst;
        for (int s=0; s<frame_size; ++s) {
            out[s] = in[channels * s];
            out[s + frame_size] = in[channels * s + 1];
        }
    }
}


static int aac_ms_decode_list_native(AACMSDecoder *st,
                                     uint8_t *buffer[],
                                     uint32_t size[],
                                     void *pcm,
                                     int frame_size,
                                     aac_copy_channel_out_func copy_channel_out)
{
    UINT valid;
    AAC_DECODER_ERROR err;
    INT_PCM *out = (INT_PCM *)pcm;

    if (frame_size <= 0) {
        return -1;
    }

    if (!st->buffer) {
        void *buf = 0;
        buf = malloc (sizeof(INT_PCM) * 2 * frame_size);
        if (!buf)
            return -1;
        st->buffer = buf;
        ia_logt("allocate buffer size %ld", sizeof(INT_PCM) * 2 * frame_size);
    }

    for (int i=0; i<st->streams; ++i) {
        ia_logt("stream %d", i);
        valid = size[i];
        err = aacDecoder_Fill(st->handles[i], &buffer[i], &size[i], &valid);
        if (err != AAC_DEC_OK)
            return -1;

        err = aacDecoder_DecodeFrame(st->handles[i], st->buffer, frame_size * 2, 0);
        if (err != AAC_DEC_OK)
            return -1;

        if (i < st->coupled_streams) {
            (*copy_channel_out)(out, st->buffer, frame_size, 2);
            out += (frame_size * 2);
        } else {
            (*copy_channel_out)(out, st->buffer, frame_size, 1);
            out += frame_size;
        }

    }
    return frame_size;
}

AACMSDecoder *aac_multistream_decoder_open (int streams, int coupled_streams,
                                            uint32_t flags, int *error)
{
    int i, ret = 0;
    AACMSDecoder *st = 0;
    HANDLE_AACDECODER *handles;
    HANDLE_AACDECODER handle;

    UCHAR extra_data_s[] = { 0x11, 0x88 };
    UCHAR extra_data_c[] = { 0x11, 0x90 };
    UCHAR *conf[1];
    UINT  clen;

    st = (AACMSDecoder *)malloc(sizeof(AACMSDecoder));
    handles = (HANDLE_AACDECODER *)malloc(sizeof (HANDLE_AACDECODER) * streams);
    if (!st || !handles) {
        ia_loge("alloc fail.");
        if (error)
            *error = 0; // TODO
        if (st)
            aac_multistream_decoder_close(st);
        return 0;
    }
    memset(st, 0, sizeof(AACMSDecoder));
    memset(handles, 0, sizeof(sizeof (HANDLE_AACDECODER) * streams));

    st->flags = flags;
    st->streams = streams;
    st->coupled_streams = coupled_streams;
    st->handles = handles;

    for (i=0; i<st->streams; ++i) {
        handle = aacDecoder_Open(TT_MP4_RAW, 1);
        if (!handle) {
            ret = -1;
            break;
        }

        st->handles[i] = handle;
        if (i < coupled_streams) {
            conf[0] =  extra_data_c;
            clen = sizeof(extra_data_c);
        } else {
            conf[0] =  extra_data_s;
            clen = sizeof(extra_data_s);
        }

        ret = aacDecoder_ConfigRaw (handle, conf, &clen);
        if (ret != AAC_DEC_OK) {
            ia_loge("aac config raw error code %d", ret);
            ret = -ret;
            break;
        }
        ret = aacDecoder_SetParam (handle, AAC_CONCEAL_METHOD, 1);
        if (ret != AAC_DEC_OK) {
            ia_loge("aac set parameter error code %d", ret);
            ret = -ret;
            break;
        }
    }

    if (ret < 0) {
        aac_multistream_decoder_close(st);
        st = 0;
    }

    return st;
}

int aac_multistream_decode_list (AACMSDecoder *st,
                                 uint8_t* buffer[], uint32_t size[],
                                 void *pcm, uint32_t frame_size)
{
    if (st->flags & AUDIO_FRAME_PLANE)
        return aac_ms_decode_list_native (st, buffer, size, pcm, frame_size,
                                          aac_copy_channel_out_plane);
    else
        return -1;
}

void aac_multistream_decoder_close (AACMSDecoder *st)
{
    if (st->handles) {
        for(int i=0; i<st->streams; ++i) {
            if (st->handles[i])
                aacDecoder_Close(st->handles[i]);
        }
        free (st->handles);
    }

    if (st->buffer)
        free (st->buffer);

    free (st);
}

