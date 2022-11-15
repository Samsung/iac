#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "drc_processor.h"
#include "fixedp11_5.h"

#include "immersive_audio_core_decoder.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_decoder.h"
#include "immersive_audio_decoder_private.h"
#include "immersive_audio_metadata.h"
#include "immersive_audio_obu.h"
#include "immersive_audio_utils.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IADEC"

#define RSHIFT(a) (1 << (a))
#define LAYOUT_FLAG(type) RSHIFT(type)

IAParam* immersive_audio_param_raw_data_new (IAParamID id,
                                             uint8_t *data, uint32_t size)
{
    IAParam *param = 0;
    switch (id) {
        case IA_PARAM_DEMIXING_INFO: {
            param = (IAParam *)malloc(sizeof(IAParam));
            if (param) {
                param->id = id;
                param->raw.data = data;
                param->raw.size = size;
            }
        } break;
    }
    return param;
}

void immersive_audio_param_free(IAParam *param)
{
    if (param)
        free(param);
}



static  int16_t FLOAT2INT16(float x)
{

#define MIN32(a,b) ((a) < (b) ? (a) : (b))   /**< Minimum 32-bit value.   */
#define MAX32(a,b) ((a) > (b) ? (a) : (b))   /**< Maximum 32-bit value.   */
#define float2int(x) lrintf(x)

  x = x * 32768.f;
  x = MAX32(x, -32768);
  x = MIN32(x, 32767);
  return (int16_t)float2int(x);
}

static void swap(void **p1, void **p2) {
    void *p = *p2;
    *p2 = *p1;
    *p1 = p;
}


static IACodecID ia_4cc_get_codec_id (uint32_t id)
{
#define TAG(a, b, c, d) ((a) | (b) << 8 | (c) << 16 | (d) << 24)

    ia_logd ("4CC : %.4s", (uint8_t *)&id);
    switch (id) {
        case TAG('m', 'p', '4', 'a'):
        case TAG('e', 's', 'd', 's'):
            return IA_CODEC_AAC;

        case TAG('o', 'p', 'u', 's'):
        case TAG('d', 'O', 'p', 's'):
            return IA_CODEC_OPUS;

        default:
            return IA_CODEC_UNKNOWN;
    }
}

static int
ia_channel_layout_get_new_channels (IAChannelLayoutType last,
                                    IAChannelLayoutType cur,
                                    IAChannel *new_chs, uint32_t count)
{
    uint32_t chs = 0;

    /**
     * In ChannelGroup for Channel audio: The order conforms to following rules:
     *
     * @ Coupled Substream(s) comes first and followed by non-coupled Substream(s).
     * @ Coupled Substream(s) for surround channels comes first and followed by one(s) for top channels.
     * @ Coupled Substream(s) for front channels comes first and followed by one(s) for side, rear and back channels.
     * @ Coupled Substream(s) for side channels comes first and followed by one(s) for rear channels.
     * @ Center channel comes first and followed by LFE and followed by the other one.
     * */

    if (last == IA_CHANNEL_LAYOUT_INVALID) {
        chs = ia_audio_layer_get_channels (cur, new_chs, count);
    } else {
        uint32_t s1 = ia_channel_layout_get_category_channels_count (last, IA_CH_CATE_SURROUND);
        uint32_t s2 = ia_channel_layout_get_category_channels_count (cur, IA_CH_CATE_SURROUND);
        uint32_t t1 = ia_channel_layout_get_category_channels_count (last, IA_CH_CATE_TOP);
        uint32_t t2 = ia_channel_layout_get_category_channels_count (cur, IA_CH_CATE_TOP);

        if (s1 < 5 && 5 <= s2) {
            new_chs[chs++] = IA_CH_L5;
            new_chs[chs++] = IA_CH_R5;
            ia_logd ("new channels : l5/r5(l7/r7)");
        }
        if (s1 < 7 && 7 <= s2) {
            new_chs[chs++] = IA_CH_SL7;
            new_chs[chs++] = IA_CH_SR7;
            ia_logd ("new channels : sl7/sr7");
        }
        if (t2 != t1 && t2 == 4) {
            new_chs[chs++] = IA_CH_HFL;
            new_chs[chs++] = IA_CH_HFR;
            ia_logd ("new channels : hfl/hfr");
        }
        if (t2 - t1 == 4) {
            new_chs[chs++] = IA_CH_HBL;
            new_chs[chs++] = IA_CH_HBR;
            ia_logd ("new channels : hbl/hbr");
        } else if (!t1 && t2 - t1 == 2) {
            if (s2 < 5) {
                new_chs[chs++] = IA_CH_TL;
                new_chs[chs++] = IA_CH_TR;
                ia_logd ("new channels : tl/tr");
            } else {
                new_chs[chs++] = IA_CH_HL;
                new_chs[chs++] = IA_CH_HR;
                ia_logd ("new channels : hl/hr");
            }
        }

        if (s1 < 3 && 3 <= s2) {
            new_chs[chs++] = IA_CH_C;
            new_chs[chs++] = IA_CH_LFE;
            ia_logd ("new channels : c/lfe");
        }
        if (s1 < 2 && 2 <=s2) {
            new_chs[chs++] = IA_CH_L2;
            ia_logd ("new channel : l2");
        }
    }

    if (chs > count) {
        ia_loge ("too much new channels %d, we only need less than %d channels",
                chs, count);
        chs = IA_ERR_BUFFER_TOO_SMALL;
    }
    return chs;
}


static void
ia_decoder_context_uninit (IADecoderContext *ctx)
{
    if (ctx->layer_info) {
        for (int i=0; i<ctx->layers; ++i) {
            if (ctx->layer_info[i].output_gain_info)
                free (ctx->layer_info[i].output_gain_info);
            if (ctx->layer_info[i].buffers)
                free (ctx->layer_info[i].buffers);
            if (ctx->layer_info[i].recon_gain_info)
                free (ctx->layer_info[i].recon_gain_info);
        }
        free(ctx->layer_info);
    }

    if (ctx->ambix_info) {
        if (ctx->ambix_info->matrix)
            free (ctx->ambix_info->matrix);
        free (ctx->ambix_info);
    }

    memset (ctx, 0, sizeof(IADecoderContext));
}


static IAErrCode
ia_decoder_context_init (IADecoderContext *ctx, IAStaticMeta *meta)
{
    ctx->dmx_mode = -1;
    ctx->layers = meta->channel_audio_layer;
    ctx->ambix = meta->ambisonics_mode;

    ia_logi ("ambisonics mode  %d, audio layers %d", ctx->ambix, ctx->layers);
    if (ctx->ambix) {
        IAAmbisonicsLayerInfo *af = 0;
        uint32_t size = 0;
        void* src = 0;
        af = (IAAmbisonicsLayerInfo *)ia_mallocz(sizeof(IAAmbisonicsLayerInfo));
        if (!af)
            goto alloc_fail;
        ctx->ambix_info = af;

        ctx->channels = af->channels =
            meta->ambix_layer_config.output_channel_count;
        af->streams = meta->ambix_layer_config.substream_count;
        af->coupled_streams = meta->ambix_layer_config.coupled_substream_count;
        if (ctx->ambix == 1) {
            size =  sizeof(uint8_t) * af->channels;
            src = meta->ambix_layer_config.channel_mapping;
        } else if (ctx->ambix == 2) {
            size = sizeof(uint16_t) *
                (af->streams + af->coupled_streams) * af->channels;
            src = meta->ambix_layer_config.demixing_matrix;
        }

        if (src && size) {
            af->matrix = (uint8_t *)malloc (size);
            if (!af->matrix)
                goto alloc_fail;
            memcpy(af->matrix, src, size);
        }

    }

    if (ctx->layers > 0) {
        IAChannelAudioLayerInfo    *lf = 0;
        IAChannelAudioLayerInfo    *plf = 0;
        IAChannelLayoutType         last = IA_CHANNEL_LAYOUT_INVALID;

        int         chs = 0;
        float       gain_db = 0;

        ia_logt ("audio layer info :");
        lf = (IAChannelAudioLayerInfo *)
            ia_mallocz (sizeof(IAChannelAudioLayerInfo) * ctx->layers);
        if (!lf)
            goto alloc_fail;
        ctx->layer_info = lf;

        ctx->streams = ctx->coupled_streams = 0;
        for (int i=0; i<ctx->layers; ++i) {
            plf = &lf[i];
            plf->layout = meta->ch_audio_layer_config[i].loudspeaker_layout;
            plf->streams = meta->ch_audio_layer_config[i].substream_count;
            plf->coupled_streams =
                meta->ch_audio_layer_config[i].coupled_substream_count;
            plf->loudness =
                q_to_float(meta->ch_audio_layer_config[i].loudness, 8);

            ia_logi("audio layer %d :", i);
            ia_logi(" > loudspeaker layout %s(%d) .",
                    ia_channel_layout_name(plf->layout), plf->layout);
            ia_logi(" > sub-stream count %d .", plf->streams);
            ia_logi(" > coupled sub-stream count %d .", plf->coupled_streams);
            ia_logi(" > loudness %f (0x%04x)", plf->loudness,
                    meta->ch_audio_layer_config[i].loudness & U16_MASK);


            if (meta->ch_audio_layer_config[i].output_gain_is_present_flag) {
                plf->output_gain_info = (IAOutputGainInfo *)
                    malloc (sizeof(IAOutputGainInfo));
                if (!plf->output_gain_info)
                    goto alloc_fail;
                plf->output_gain_info->gain_flags =
                    meta->ch_audio_layer_config[i].output_gain_flags;
                gain_db =
                    q_to_float(meta->ch_audio_layer_config[i].output_gain, 8);
                plf->output_gain_info->gain = db2lin(gain_db);
                ia_logi(" > output gain flags 0x%02x",
                        plf->output_gain_info->gain_flags & U8_MASK);
                ia_logi(" > output gain %f (0x%04x), linear gain %f",
                        gain_db,
                        meta->ch_audio_layer_config[i].output_gain & U16_MASK,
                        plf->output_gain_info->gain);
            } else {
                ia_logi(" > no output gain info.");
            }

            plf->buffers = (IABuffer *) malloc (sizeof(IABuffer) * plf->streams);
            if (!plf->buffers)
                goto alloc_fail;

            if (meta->ch_audio_layer_config[i].recon_gain_is_present_flag) {
                plf->recon_gain_info = (IAReconGainInfo2 *)
                    ia_mallocz (sizeof(IAReconGainInfo2));
                if (!plf->recon_gain_info)
                    goto alloc_fail;
                ia_logi(" > wait recon gain info.");
            } else {
                ia_logi(" > no recon gain info.");
            }

            chs += ia_channel_layout_get_new_channels(last, plf->layout,
                    &ctx->channels_order[chs], IA_CH_LAYOUT_MAX_CHANNELS - chs);

            ctx->streams += plf->streams;
            ctx->coupled_streams += plf->coupled_streams;

            ia_logd(" > the total of %d channels", chs);
            last = plf->layout;

            ctx->layout_flags |= LAYOUT_FLAG(plf->layout);
        }
        ia_logi ("audio layer info .");

        ctx->channels =
            ia_channel_layout_get_channels_count (plf->layout);

        ia_logi ("channels %d, streams %d, coupled streams %d.", ctx->channels,
                ctx->streams, ctx->coupled_streams);

        if (chs != ctx->channels) {
            ia_loge ("channels mismatch (%d vs %d).", chs, ctx->channels);
            return IA_ERR_INTERNAL;
        }

        ia_logi("all channels order:");
        for (int c=0; c<ctx->channels; ++c)
            ia_logi("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
                    ctx->channels_order[c]);


        ctx->layer = ctx->layers - 1;
        ctx->layout = ctx->layer_info[ctx->layer].layout;
        ctx->layout_channels = ia_channel_layout_get_channels_count(ctx->layout);

        ia_logi("initialized layer %d, layout %s (%d), layout channel count %d.",
                ctx->layer, ia_channel_layout_name(ctx->layout), ctx->layout,
                ctx->layout_channels);
    }

    return IA_OK;

alloc_fail:
    ia_decoder_context_uninit(ctx);

    return IA_ERR_ALLOC_FAIL;
}


static void
ia_recon_channels_order_update (IAChannelLayoutType layout, IAReconGainInfo2 *re)
{
    int chs = 0;
    static IAReconChannel recon_channel_order[] = {
        IA_CH_RE_L, IA_CH_RE_C, IA_CH_RE_R, IA_CH_RE_LS, IA_CH_RE_RS,
        IA_CH_RE_LTF, IA_CH_RE_RTF,
        IA_CH_RE_LB, IA_CH_RE_RB, IA_CH_RE_LTB, IA_CH_RE_RTB, IA_CH_RE_LFE
    };

    static IAChannel channel_layout_map[IA_CHANNEL_LAYOUT_COUNT][IA_CH_RE_COUNT] = {
        {IA_CH_MONO, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID},
        {IA_CH_L2, IA_CH_INVALID, IA_CH_R2, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID},
        {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
        {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HL, IA_CH_HR,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
        {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HFL, IA_CH_HFR,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE},
        {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
        {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HL, IA_CH_HR,
            IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
        {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HFL, IA_CH_HFR,
            IA_CH_BL7, IA_CH_BR7, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE},
        {IA_CH_L3, IA_CH_C, IA_CH_R3, IA_CH_INVALID, IA_CH_INVALID, IA_CH_TL, IA_CH_TR,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE}
    };

#define RECON_CHANNEL_FLAG(c) RSHIFT(c)

    for (int c=0; c<IA_CH_RE_COUNT; ++c) {
        if (re->flags & RECON_CHANNEL_FLAG(recon_channel_order[c]))
            re->order[chs++] =
                channel_layout_map[layout][recon_channel_order[c]];
    }
}


static IAErrCode
ia_decoder_context_set_recon_gain_info_list (IADecoder *ths,
                                             IAReconGainInfoList *list)
{
    IAReconGainInfo    *src;
    IAReconGainInfo2   *dst;
    int ri = 0;
    IAErrCode ec = IA_OK;
    IADecoderContext *ctx = ths->dctx;

    ia_logt("recon gain info :");
    for (int i=0; i<ths->dctx->layers; ++i) {
        src = &list->recon_gain_info[ri];
        dst = ctx->layer_info[i].recon_gain_info;
        if (dst) {
            ++ri;
            if (i > ths->dctx->layer)
                continue;
            ia_logd("audio layer %d :", i);
            if (dst->flags ^ src->flags) {
                dst->flags = src->flags;
                dst->channels = src->channels;
                ia_recon_channels_order_update (ctx->layer_info[i].layout, dst);
            }
            for (int c=0; c<dst->channels; ++c) {
                dst->recon_gain[c] = qf_to_float(src->recon_gain[c], 8);
            }
            ia_logd(" > recon gain flags 0x%04x", dst->flags);
            ia_logd(" > channel count %d", dst->channels);
            for (int c=0; c<dst->channels; ++c)
                ia_logd(" > > channel %s(%d) : recon gain %f(0x%02x)",
                        ia_channel_name(dst->order[c]), dst->order[c],
                        dst->recon_gain[c], src->recon_gain[c]);
        }
    }
    ia_logt("recon gain info .");

    if (list->count != ri) {
        ec = IA_ERR_INTERNAL;
        ia_loge ("%s : the count (%d) of recon gain doesn't match with static meta (%d).",
                ia_error_code_string(ec), list->count, ri);
    }

    return ec;
}


static IAErrCode ia_decoder_open (IADecoder *ths, IACodecID cid,
                                  uint8_t *spec, uint32_t clen,
                                  uint32_t flags)
{
    IAErrCode ec = IA_OK;
    IADecoderContext *ctx = ths->dctx;
    IACoreDecoder *cdec = 0;
    uint32_t newflags = flags;

    if (ctx->ambix) {
        ec = IA_ERR_UNIMPLEMENTED;
        ia_loge ("%s : ambisonics audio.", ia_error_code_string(ec));
        /**
         * TODO : ths->dec decode ambisonics audio.
         * */
        return ec;
    }

    newflags |= IA_FLAG_SUBSTREAM_CODEC_SPECIFIC;
    for (int i=0; i<ctx->layers; ++i) {

        cdec = ia_core_decoder_open(cid);
        if (!cdec) {
            ec = IA_ERR_INTERNAL;
            ia_loge ("%s : Failed to open the %d-IACoreDecoder (%s).",
                    ia_error_code_string(ec), i+1, ia_codec_name(cid));
            break;
        }

        ths->ldec[i] = cdec;

        ia_core_decoder_set_codec_specific_info (cdec, spec, clen);
        ia_core_decoder_set_streams_info (cdec, ctx->layer_info[i].streams,
                                          ctx->layer_info[i].coupled_streams);

        ec = ia_core_decoder_init (cdec, newflags);
        if (ec != IA_OK) {
            ia_loge ("%s : Failed to set streams info to decoder %s.",
                    ia_error_code_string(ec), ia_codec_name(cid));
            break;
        }
    }

    return ec;
}


static IAChannel
ia_output_gain_channel_map (IAChannelLayoutType type, IAOutputGainChannel gch)
{
    IAChannel ch = IA_CH_INVALID;
    switch (gch) {
        case IA_CH_GAIN_L: {
            switch (type) {
                case IA_CHANNEL_LAYOUT_MONO:
                    ch = IA_CH_MONO;
                    break;
                case IA_CHANNEL_LAYOUT_STEREO:
                    ch = IA_CH_L2;
                    break;
                case IA_CHANNEL_LAYOUT_312:
                    ch = IA_CH_L3;
                    break;
                default:
                    break;
            }
        } break;

        case IA_CH_GAIN_R: {
            switch (type) {
                case IA_CHANNEL_LAYOUT_STEREO:
                    ch = IA_CH_R2;
                    break;
                case IA_CHANNEL_LAYOUT_312:
                    ch = IA_CH_R3;
                    break;
                default:
                    break;
            }
        } break;

        case IA_CH_GAIN_LS: {
            if (ia_channel_layout_get_category_channels_count(type, IA_CH_CATE_SURROUND) == 5)
                ch = IA_CH_SL5;
        } break;

        case IA_CH_GAIN_RS: {
            if (ia_channel_layout_get_category_channels_count(type, IA_CH_CATE_SURROUND) == 5)
                ch = IA_CH_SR5;
        } break;

        case IA_CH_GAIN_LTF: {
            if (ia_channel_layout_get_category_channels_count(type, IA_CH_CATE_SURROUND) < 5)
                ch = IA_CH_TL;
            else
                ch = IA_CH_HL;
        } break;

        case IA_CH_GAIN_RTF: {
            if (ia_channel_layout_get_category_channels_count(type, IA_CH_CATE_SURROUND) < 5)
                ch = IA_CH_TR;
            else
                ch = IA_CH_HR;
        } break;
        default: break;
    }

    return ch;
}

static IAErrCode
ia_decoder_configure_demixer (IADecoder *ths)
{
    IADecoderContext *ctx = ths->dctx;
    IAChannel   chs[IA_CH_LAYOUT_MAX_CHANNELS];
    float       gains[IA_CH_LAYOUT_MAX_CHANNELS];
    uint8_t     flags;
    uint32_t    count = 0;

    demixer_set_channel_layout (ths->demixer, ctx->layout);
    demixer_set_channels_order (ths->demixer, ctx->channels_order,
                                ctx->layout_channels);

    for (int l=0; l<=ctx->layer; ++l) {
        if (ctx->layer_info[l].output_gain_info) {
            flags = ctx->layer_info[l].output_gain_info->gain_flags;
            for (int c=0; c<IA_CH_GAIN_COUNT; ++c) {
                if (flags & RSHIFT(c)) {
                    chs[count] =
                        ia_output_gain_channel_map (ctx->layer_info[l].layout, c);
                    if (chs[count] != IA_CH_INVALID)
                        gains[count++] =
                            ctx->layer_info[l].output_gain_info->gain;
                }
            }
        }
    }

    demixer_set_output_gain (ths->demixer, chs, gains, count);

    ia_logi ("demixer info :");
    ia_logi ("layout %s(%d)", ia_channel_layout_name(ctx->layout), ctx->layout);
    ia_logi ("input channels order :");

    for (int c=0; c<ctx->layout_channels; ++c) {
        ia_logi ("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
                ctx->channels_order[c]);
    }

    ia_logi ("output gain info : ");
    for (int c=0; c<count; ++c) {
        ia_logi ("channel %s(%d) gain %f", ia_channel_name(chs[c]), chs[c], gains[c]);
    }

    return IA_OK;
}


static IAErrCode ia_decoder_set_demixing_info (IADecoder* ths,
                                               uint8_t* data, uint32_t size)
{
    IAOBU               unit;
    IAErrCode           iec = IA_OK;
    IADecoderContext   *ctx;

    if (!ths)
        return IA_ERR_INVALID_STATE;

    if (!data || !size)
        return IA_ERR_BAD_ARG;

    ctx = ths->dctx;

    iec = ia_obu_find_demixing_info (&unit, data, size);
    if (iec == IA_OK) {
        ctx->dmx_mode = ia_demixing_info_parse (unit.payload, unit.psize);
        ia_logd("new demixing mode %d", ctx->dmx_mode);
    }
    return iec;
}

static IAErrCode
ia_decoder_parse_parameters (IADecoder *ths, IAParam* param[], uint32_t count)
{
    IAErrCode           ec = IA_OK;
    IAParam            *p;

    for (int i=0; i<count; ++i) {
        p = param[i];
        switch (p->id) {
            case IA_PARAM_DEMIXING_INFO: {
                ec = ia_decoder_set_demixing_info (ths, p->raw.data, p->raw.size);
                if (ec == IA_OK)
                    ths->dctx->flags |= IA_FLAG_EXTERNAL_DMX_INFO;
            } break;
            default: break;
        }
    }
    return ec;
}

static IAErrCode ia_decoder_packet_parse (IADecoder *ths, uint8_t *data, uint32_t len)
{
    IAErrCode   ec = IA_OK;
    IAOBU       obu;
    uint32_t    pos = 0;
    IABuffer    pkts[IA_CH_LAYOUT_MAX_CHANNELS];
    uint32_t    count = 0;
    int         ret;
    IADecoderContext *ctx = ths->dctx;

    while (pos < len) {
        ret = ia_obu_stream_parse (&obu, data + pos, len - pos);
        if (ret > 0) {
            pos += ret;
            switch (obu.type) {
                case IA_OBU_DEMIXING_INFO: {
                    if (!(ctx->flags & IA_FLAG_EXTERNAL_DMX_INFO))
                        ia_decoder_set_demixing_info (ths, data + pos, len - pos);
                } break;
                case IA_OBU_RECON_GAIN_INFO: {
                    IAReconGainInfoList list;
                    memset (&list, 0, sizeof (IAReconGainInfoList));
                    if (ia_recon_gain_info_parse (&list, obu.payload, obu.psize) > 0)
                        ia_decoder_context_set_recon_gain_info_list(ths, &list);
                } break;
                case IA_OBU_SUBSTREAM: {
                    pkts[count].data = obu.payload;
                    pkts[count++].len = obu.psize;
                } break;
                default:
                    break;
            }
        } else {
            ec = IA_ERR_INVALID_PACKET;
            break;
        }
    }

    if (ec == IA_OK && count > 0) {
        if (ctx->streams != count) {
            ec = IA_ERR_INVALID_PACKET;
            ia_loge("%s, the total of sub-packets is %d (vs %d)",
                    ia_error_code_string(ec), count, ctx->streams);
        } else {
            int idx = 0;
            for (int i=0; i<ctx->layers; ++i) {
                for (int s=0; s<ctx->layer_info[i].streams; ++s) {
                    ctx->layer_info[i].buffers[s] = pkts[idx++];
                }
            }
        }
    }

    return ec;
}


static int ia_decoder_decode(IADecoder *ths, float *pcm, int frame_size)
{
    IADecoderContext *ctx = ths->dctx;
    int ret = 0;
    float *out = pcm;
    IACoreDecoder *dec;

    ia_logt("decode sub-packets.");
    if (!ctx->ambix) {
        ia_logt("audio layer only mode.");
        IAChannelAudioLayerInfo *lf = 0;
        int channels;
        uint8_t *sub_pkt[MAX_STREAMS];
        uint32_t sub_pkt_size[MAX_STREAMS];

        for (int i=0; i<=ctx->layer; ++i) {
            ia_logt("audio layer %d.", i);
            dec = ths->ldec[i];
            lf = &ctx->layer_info[i];
            channels = lf->streams + lf->coupled_streams;
            ia_logd("CG#%d: channels %d, streams %d, out %p, offset %lX, size %lu",
                    i, channels, lf->streams, out, (void *)out - (void *)pcm,
                    sizeof(float) * ctx->frame_size * channels);
            for (int s=0; s<lf->streams; ++s) {
                sub_pkt[s] = lf->buffers[s].data;
                sub_pkt_size[s] = lf->buffers[s].len;
                ia_logd(" > sub-packet %d (%p) size %d", s, sub_pkt[s], sub_pkt_size[s]);
            }
            ret = ia_core_decoder_decode_list (dec, sub_pkt, sub_pkt_size,
                                               lf->streams, out, frame_size);
            if(ret < 0) {
                ia_loge("sub packet %d decode fail.", i);
                break;
            } else if (ret != ctx->frame_size) {
                ia_loge("decoded frame size is not %d (%d).", ctx->frame_size, ret);
                break;
            }
            out += (ret * channels);
        }
    }

    return ret;
}

static IAErrCode
ia_decoder_demixing (IADecoder *ths, float *src, float *dst, uint32_t frame_size)
{
    IAReconGainInfo2 *re;
    IADecoderContext *ctx = ths->dctx;

    re = ctx->layer_info[ctx->layer].recon_gain_info;

    ia_logt ("demixer info update :");
    if (re) {
        demixer_set_recon_gain (ths->demixer, re->channels,
                                re->order, re->recon_gain, re->flags);

        ia_logd ("channel flags 0x%04x", re->flags & U16_MASK);
        for (int c=0; c<re->channels; ++c) {
            ia_logd ("channel %s(%d) recon gain %f",
                    ia_channel_name(re->order[c]), re->order[c],
                    re->recon_gain[c]);
        }
    }
    demixer_set_demixing_mode (ths->demixer, ths->dctx->dmx_mode);
    ia_logd ("demixing mode %d", ths->dctx->dmx_mode);

    return demixer_demixing (ths->demixer, dst, src, frame_size);
}

static void
ia_decoder_frame_drc (IADecoder *ths, float *out, float *in, int frame_size)
{
    IADecoderContext *ctx = ths->dctx;
    drc_process_block(ctx->drc_mode, ctx->layer_info[ctx->layer].loudness,
            in, out, frame_size,
            ia_channel_layout_get_channels_count(ctx->layout), ths->compressor,
            &ths->limiter);
}

static void ia_decoder_plane2stride_out_short (void *dst, const float *src,
                                                int frame_size, int channels)
{
   int16_t *short_dst = (int16_t*)dst;

   ia_logd("channels %d", channels);
   for (int c = 0; c<channels; ++c) {
       if (src) {
          for (int i=0; i<frame_size; i++)
             short_dst[i * channels + c] = FLOAT2INT16(src[frame_size * c + i]);
       } else {
          for (int i=0; i<frame_size; i++)
             short_dst[i * channels + c] = 0;
       }
   }
}


static int ia_decoder_find_layer (IADecoder *ths, IAChannelLayoutType layout)
{
    int ret = -1;
    IADecoderContext *ctx = ths->dctx;
    for (int i=ctx->layers-1; i>=0; --i) {
        if (ctx->layer_info[i].layout == layout) {
            ret = i;
            break;
        }
    }

    return ret;
}


int immersive_audio_channel_layout_get_channels_count (IAChannelLayoutType type)
{
    return ia_channel_layout_get_channels_count(type);
}


IADecoder* immersive_audio_decoder_create (IACodecID cid,
                                         uint8_t* cspec, uint32_t clen,
                                         uint8_t *meta, uint32_t mlen,
                                         uint32_t flags)
{
    IADecoder          *ths = 0;
    IADecoderContext   *ctx = 0;
    IAErrCode           ec = IA_OK;
    IAStaticMeta        sm;
    IACodecID           id = IA_CODEC_UNKNOWN;
    uint8_t            *cfg = cspec;
    uint32_t            csize = clen;

    /* Check codec specification info. */
    if (!cspec || !clen) {
        ec = IA_ERR_BAD_ARG;
        ia_loge ("%s : codec specific info %p and length %u",
                ia_error_code_string(ec), cspec, clen);
        goto termination;
    }

    id = cid;
    /* Check codec id. */
    if (flags & IA_FLAG_CODEC_CONFIG_ISOBMFF) {
        id = ia_4cc_get_codec_id(*((uint32_t *)(cspec + 4)));
    } else if (~flags & IA_FLAG_CODEC_CONFIG_RAW) {
        IAOBU unit;
        IACodecSpecInfo info;
        if (ia_obu_find_codec_specific_info(&unit, cspec, clen) != IA_OK) {
            ec = IA_ERR_BAD_ARG;
            ia_loge ("%s : the parameters doesn't includes the specification info",
                    ia_error_code_string(ec));
            goto termination;
        }

        ia_codec_specific_info_parse(&info, unit.payload, unit.psize);
        id = ia_4cc_get_codec_id(info.cid);
        cfg = info.config;
        csize = info.size;
        flags |= IA_FLAG_CODEC_CONFIG_RAW;
    };

    if (!ia_codec_check(id)) {
        ec = IA_ERR_BAD_ARG;
        ia_loge ("%s : Invalid codec id %u", ia_error_code_string(ec), id);
        goto termination;
    }


    /* Alloc memory for IADecoder and IADecoderContext. */
    ths = (IADecoder *) ia_mallocz (sizeof(IADecoder));
    if (!ths) {
        ec = IA_ERR_ALLOC_FAIL;
        ia_loge ("%s : IADecoder.", ia_error_code_string(ec));
        goto termination;
    }

    ctx = (IADecoderContext *) ia_mallocz (sizeof (IADecoderContext));
    if (!ctx) {
        ec = IA_ERR_ALLOC_FAIL;
        ia_loge ("%s : IADecoderContext.", ia_error_code_string(ec));
        goto termination;
    }

    ths->dctx = ctx;

    /* Parse IA static meta and initialize IADecoderContext. */
    if (meta) {
        uint8_t* raw = 0;
        uint32_t size = mlen;
        if (~flags & IA_FLAG_STATIC_META_RAW) {
            IAOBU unit;
            IAErrCode iec = ia_obu_find_static_meta (&unit, meta, mlen);
            if (iec == IA_OK) {
                raw = unit.payload;
                size = unit.psize;
            } else {
                ia_logw ("%s : can not find valid static meta.",
                        ia_error_code_string(iec));
            }
        } else {
            raw = meta;
            size = mlen;
        }

        memset((void *)&sm, 0, sizeof(IAStaticMeta));
        ec = ia_static_meta_parse (&sm, raw, size);
        if (ec != IA_OK) {
            ia_loge ("%s : Failed to parse static meta data.",
                    ia_error_code_string (ec));
            goto termination;
        }
        ec = ia_decoder_context_init (ctx, &sm);
        if (ec != IA_OK) {
            ia_loge ("%s : Failed to init decoder context.",
                    ia_error_code_string (ec));
            goto termination;
        }

        ctx->frame_size =
            id == IA_CODEC_AAC ? AAC_FRAME_SIZE : OPUS_FRAME_SIZE;
        ctx->delay = id == IA_CODEC_AAC ? AAC_DELAY : OPUS_DELAY;
        ia_logi("frame size is %u, delay %u", ctx->frame_size, ctx->delay);
    }

    /* open true decoder. */
    ec = ia_decoder_open (ths, id, cfg, csize, flags);

    if (ec != IA_OK) {
        ia_loge ("%s : Failed to open internal decoder.",
                ia_error_code_string (ec));
        goto termination;
    }

    /* initialize demixer. */
    ths->demixer = demixer_open(ctx->frame_size, ctx->delay);
    if (!ths->demixer) {
        ec = IA_ERR_INTERNAL;
        ia_loge("%s : demixer is open failed.", ia_error_code_string(ec));
        goto termination;
    }

    ia_decoder_configure_demixer (ths);

    /* initialize limiter. */
    drc_init_limiter(&ths->limiter, 48000, ctx->channels);

    /* initialize buffers. */
    ia_logt ("buffer size %lu.", sizeof(float) * MAX_FRAME_SIZE * ctx->channels);
    for (int i=0; i<DEC_BUF_CNT; ++i) {
        ths->buffer[i] =
            (float *) malloc (sizeof(float) * MAX_FRAME_SIZE * ctx->channels);
        if (!ths->buffer[i]) {
            ec = IA_ERR_ALLOC_FAIL;
            ia_loge ("%s : buffer is allocated failed.", ia_error_code_string(ec));
            goto termination;
        }
    }

termination:
    if (ec < 0) {
        immersive_audio_decoder_destroy (ths);
        ths = 0;
    }

    ia_static_meta_uninit(&sm);
    return ths;
}


int immersive_audio_decoder_decode (IADecoder* ths,
        uint8_t* data, uint32_t len, uint16_t* pcm, uint32_t size,
        IAParam* param[], uint32_t count)
{
    int ret = 0;
    int real_frame_size = 0;
    float *in, *out;
    static int pktidx = 0;
    IADecoderContext *ctx = ths->dctx;

    in = ths->buffer[0];
    out = ths->buffer[1];

    ia_logt("decode audio packet %d : data %p, size %d, out %p, max size %d",
            ++pktidx, data, len , pcm, size);

    /**
     * 0. parse parameters.
     * */
    ctx->flags |= (~IA_FLAG_EXTERNAL_DMX_INFO);

    if (param && count > 0)
        ia_decoder_parse_parameters(ths, param, count);

    /**
     * 1. parse audio complex packet.
     * */
    ia_logt("Parse audio packet.");
    ret = ia_decoder_packet_parse(ths, data, len);
    if (ret != IA_OK) return ret;

    /**
     * 2. Decode audio packet.
     * */
    ia_logt("Decode audio packet.");
    ret = ia_decoder_decode(ths, out, size);
    if (ret <= 0) return ret;

    real_frame_size = ret;
    swap((void **)&in, (void **)&out);

    /**
     * 3. Gain-up, demixing and smoothing.
     * */
    ia_logt("Gain-up, demixing, smoothing.");
    ret = ia_decoder_demixing(ths, in, out, real_frame_size);

    if (ret != IA_OK) return ret;

    swap((void **)&in, (void **)&out);

    /**
     * 4. Loudness normalization, drc and limit.
     * */
    ia_logt("Loudness normalization, drc and limit.");

    ia_decoder_frame_drc(ths, out, in, real_frame_size);

    /**
     * 5. re-order and pack
     * */
    ia_decoder_plane2stride_out_short(pcm, out, real_frame_size,
                                      ths->dctx->layout_channels);

    return real_frame_size;
}


void immersive_audio_decoder_destroy (IADecoder* ths)
{
    if (ths) {
        ia_logt ("close in");
        if (ths->dctx) {
            ia_decoder_context_uninit(ths->dctx);
            free (ths->dctx);
        }

        for (int d=0; d<IA_CHANNEL_LAYOUT_COUNT; ++d)
            if (ths->ldec[d])
                ia_core_decoder_close(ths->ldec[d]);

        if (ths->adec)
            ia_core_decoder_close(ths->adec);

        for (int i=0; i<DEC_BUF_CNT; ++i)
            if (ths->buffer[i])
                free (ths->buffer[i]);

        if (ths->demixer)
            demixer_close(ths->demixer);
        ia_logt ("close demixer");
        audio_effect_peak_limiter_uninit(&ths->limiter);
        ia_logt ("close limiter");

        free (ths);
    }
}

IAErrCode immersive_audio_decoder_set_channel_layout (IADecoder* ths,
                                                      IAChannelLayoutType type)
{
    IAErrCode           ec = IA_OK;
    IADecoderContext   *ctx;


    if (!ths || !ths->dctx)
        return IA_ERR_INVALID_STATE;

    ctx = ths->dctx;

    if (ctx->layers < 1)
        return IA_ERR_BAD_ARG;

    if (!ia_channel_layout_type_check(type)
            || !(ctx->layout_flags & LAYOUT_FLAG(type))) {
        ec = IA_ERR_BAD_ARG;
        ia_loge("%s :  this layout %d is invalid, please get valid layouts firstly",
                ia_error_code_string(ec), type);
        return ec;
    }

    ia_logi("set layout %s(%d)", ia_channel_layout_name(type), type);
    if (type == ctx->layout)
        return ec;

    ctx->layout = type;
    ctx->layer = ia_decoder_find_layer (ths, type);
    ctx->layout_channels = ia_channel_layout_get_channels_count(type);

    ia_logi("target group %d, layout %s(%d)", ctx->layer,
            ia_channel_layout_name(type), type);

    ia_decoder_configure_demixer (ths);
    if (ctx->drc_mode == IA_DRC_MODILE_MODE)
        drc_init_compressor (ths->compressor, 48000, ctx->layout_channels,
                             ctx->frame_size);
    drc_init_limiter (&ths->limiter, 48000, ctx->layout_channels);
    ec = IA_OK;

    return ec;
}

IAErrCode immersive_audio_decoder_set_drc_mode (IADecoder* ths, int mode)
{
    IADecoderContext   *ctx;

    if (!ths || !ths->dctx)
        return IA_ERR_INVALID_STATE;

    if (mode < IA_DRC_AV_MODE || mode > IA_DRC_MODILE_MODE)
        return IA_ERR_BAD_ARG;

    ctx = ths->dctx;

    if (mode == ctx->drc_mode)
        return IA_OK;

    if (mode == IA_DRC_MODILE_MODE) {
        if (!ths->compressor) {
            ths->compressor = audio_effect_compressor_create ();
            if (!ths->compressor)
                return IA_ERR_ALLOC_FAIL;
        }
        drc_init_compressor (ths->compressor, 48000, ctx->layout_channels,
                             ctx->frame_size);
    } else {
        if (ths->compressor) {
            audio_effect_compressor_destroy (ths->compressor);
            ths->compressor = 0;
        }
    }

    ctx->drc_mode = mode;

    return IA_OK;
}


