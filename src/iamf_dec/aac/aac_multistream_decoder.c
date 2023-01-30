#include <stdlib.h>
#include <string.h>

#include <aacdecoder_lib.h>

#include "aac_multistream_decoder.h"
#include "IAMF_defines.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "AACMS"

#define MAX_BUFFER_SIZE     MAX_AAC_FRAME_SIZE * 2

struct AACMSDecoder {
    uint32_t    flags;
    int         streams;
    int         coupled_streams;

    HANDLE_AACDECODER *handles;
    INT_PCM     buffer[MAX_BUFFER_SIZE];
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
    int fs = 0;
    CStreamInfo *info;

    for (int i=0; i<st->streams; ++i) {
        ia_logt("stream %d", i);
        valid = size[i];
        err = aacDecoder_Fill(st->handles[i], &buffer[i], &size[i], &valid);
        if (err != AAC_DEC_OK) {
            return IA_ERR_INVALID_PACKET;
        }

        err = aacDecoder_DecodeFrame(st->handles[i], st->buffer, MAX_BUFFER_SIZE, 0);
        if (err == AAC_DEC_NOT_ENOUGH_BITS) {
            return IA_ERR_NOT_ENOUGH_DATA;
        }
        if (err != AAC_DEC_OK) {
            ia_loge("stream %d : fail to decode", i);
            return IA_ERR_INTERNAL;
        }

        info = aacDecoder_GetStreamInfo(st->handles[i]);
        if (info) {
            fs = info->frameSize;
            if (fs > MAX_AAC_FRAME_SIZE) {
                ia_logw("frame size %d , is larger than %u",
                        fs, MAX_AAC_FRAME_SIZE);
                fs = MAX_AAC_FRAME_SIZE;
            }

            ia_logt ("aac decoder %d:", i);
            ia_logt (" > sampleRate : %d.", info->sampleRate);
            ia_logt (" > frameSize  : %d.", info->frameSize);
            ia_logt (" > numChannels: %d.", info->numChannels);
            ia_logt (" > outputDelay: %u.", info->outputDelay);

        } else {
            fs = frame_size;
        }

        if (info) {
            (*copy_channel_out)(out, st->buffer, fs, info->numChannels);
            out += (fs * info->numChannels);
        } else if (i < st->coupled_streams) {
            (*copy_channel_out)(out, st->buffer, fs, 2);
            out += (fs * 2);
        } else {
            (*copy_channel_out)(out, st->buffer, fs, 1);
            out += fs;
        }
    }

    return fs;
}


AACMSDecoder *aac_multistream_decoder_open (uint8_t *config, uint32_t size,
        int streams, int coupled_streams,
        uint32_t flags, int *error)
{
    AACMSDecoder        *st = 0;
    HANDLE_AACDECODER   *handles = 0;
    HANDLE_AACDECODER   handle;

    int     i, ret = 0;

    UCHAR extra_data_s[] = { 0x11, 0x88 };
    UCHAR extra_data_c[] = { 0x11, 0x90 };
    UCHAR *conf[1];
    UINT  clen;

    st = (AACMSDecoder *)malloc(sizeof(AACMSDecoder));
    handles = (HANDLE_AACDECODER *)malloc(sizeof (HANDLE_AACDECODER) * streams);

    if (st) {
        memset(st, 0, sizeof(AACMSDecoder));
    }
    if (handles) {
        memset(handles, 0, sizeof(sizeof (HANDLE_AACDECODER) * streams));
    }

    if (!st || !handles) {
        ia_loge("alloc fail.");
        if (error) {
            *error = IA_ERR_ALLOC_FAIL;
        }
        if (st) {
            aac_multistream_decoder_close(st);
        }
        if (handles) {
            free (handles);
        }
        return 0;
    }

    st->flags = flags;
    st->streams = streams;
    st->coupled_streams = coupled_streams;
    st->handles = handles;

    for (i=0; i<st->streams; ++i) {
        handle = aacDecoder_Open(TT_MP4_RAW, 1);
        if (!handle) {
            ret = IA_ERR_INVALID_STATE;
            break;
        }

        st->handles[i] = handle;
        if (config) {
            conf[0] = config;
            clen = size;
        } else if (i < coupled_streams) {
            conf[0] =  extra_data_c;
            clen = sizeof(extra_data_c);
        } else {
            conf[0] =  extra_data_s;
            clen = sizeof(extra_data_s);
        }

        ret = aacDecoder_ConfigRaw (handle, conf, &clen);
        if (ret != AAC_DEC_OK) {
            ia_loge("aac config raw error code %d", ret);
            ret = IA_ERR_INTERNAL;
            break;
        }
        ret = aacDecoder_SetParam (handle, AAC_CONCEAL_METHOD, 1);
        if (ret != AAC_DEC_OK) {
            ia_loge("aac set parameter error code %d", ret);
            ret = IA_ERR_INTERNAL;
            break;
        }
    }

    if (ret < 0) {
        if (error) {
            *error = ret;
        }
        aac_multistream_decoder_close(st);
        st = 0;
    }

    return st;
}

int aac_multistream_decode_list (AACMSDecoder *st,
                                 uint8_t *buffer[], uint32_t size[],
                                 void *pcm, uint32_t frame_size)
{
    if (st->flags & AUDIO_FRAME_PLANE)
        return aac_ms_decode_list_native (st, buffer, size, pcm, frame_size,
                                          aac_copy_channel_out_plane);
    else {
        return IA_ERR_UNIMPLEMENTED;
    }
}

void aac_multistream_decoder_close (AACMSDecoder *st)
{
    if (st) {
        if (st->handles) {
            for(int i=0; i<st->streams; ++i) {
                if (st->handles[i]) {
                    aacDecoder_Close(st->handles[i]);
                }
            }
            free (st->handles);
        }
        free (st);
    }
}

