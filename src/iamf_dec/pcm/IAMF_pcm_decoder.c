#include "bitstreamrw.h"
#include "IAMF_codec.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "math.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_PCM"


static IAErrCode iamf_pcm_init (IACodecContext *ths)
{
    IAErrCode ec = IA_OK;
    uint8_t *config = ths->cspec;

    if (!ths->cspec || ths->clen <= 0) {
        return IAMF_ERR_BAD_ARG;
    }

    ths->sample_rate = get_uint32be (config, 0);
    ths->sample_size = get_uint8(config, 4);

    if (ths->sample_size == 24) {
        ia_loge("Unimplement for 24 bits pcm.");
        ec = IAMF_ERR_UNIMPLEMENTED;
        return ec;
    }

    return ec;
}

static int iamf_pcm_decode_list (IACodecContext *ths,
                                uint8_t *buf[], uint32_t len[],
                                uint32_t count,
                                void *pcm, const uint32_t frame_size)
{
    float  *fpcm = (float *)pcm;
    short  *in = 0;
    int     c = 0, cc;

    if (count != ths->streams) {
        return IA_ERR_BAD_ARG;
    }

    ia_logd("cs %d, s %d, frame size %d", ths->coupled_streams, ths->streams, frame_size);

    for (; c < ths->coupled_streams; ++c) {
        in = (short *) buf[c];
        for (int s=0; s<frame_size; ++s) {
            for (int lf=0; lf<2; ++lf) {
                fpcm[frame_size * (c * 2 + lf) + s] = in[s*2+lf] / 32768.f;
            }
        }
    }

    cc = ths->coupled_streams;
    for (; c < ths->streams; ++c) {
        in = (short *) buf[c];
        for (int s=0; s<frame_size; ++s) {
            fpcm[frame_size * (cc + c) + s] = in[s] / 32768.f;
        }
    }
    return frame_size;
}

static IAErrCode iamf_pcm_close (IACodecContext *ths)
{
    return IAMF_OK;
}

const IACodec iamf_pcm_decoder = {
    .cid            = IA_CODEC_PCM,
    .init           = iamf_pcm_init,
    .decode_list    = iamf_pcm_decode_list,
    .close          = iamf_pcm_close,
};
