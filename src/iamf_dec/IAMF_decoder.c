#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "IAMF_OBU.h"
#include "IAMF_debug.h"
#include "IAMF_decoder.h"
#include "IAMF_decoder_private.h"
#include "IAMF_utils.h"
#include "ae_rdr.h"
#include "bitstream.h"
#include "demixer.h"
#include "fixedp11_5.h"

#define RSHIFT(a) (1 << (a))
#define INAVLID_MIX_PRESENTATION_INDEX -1
#define INVALID_ID (uint64_t)(-1)

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_DEC"

#define SR 0
#if SR
#include <stdio.h>
#include "wavwriter2.h"
#define N_AUDIO_STREAM 10

struct stream_log_t {
  void *wav;
  int element_id;
  int nchannels;
};

int _dec_stream_count = 0;
int _rec_stream_count = 0;
int _ren_stream_count = 0;
int _mix_stream_count = 0;
struct stream_log_t _dec_stream_log[N_AUDIO_STREAM];
struct stream_log_t _rec_stream_log[N_AUDIO_STREAM];
struct stream_log_t _ren_stream_log[N_AUDIO_STREAM];
struct stream_log_t _mix_stream_log;
#endif


/* ----------------------------- Utils ----------------------------- */

static void swap(void **p1, void **p2)
{
    void *p = *p2;
    *p2 = *p1;
    *p1 = p;
}

/* ----------------------------- Internal methods ------------------ */

static int16_t FLOAT2INT16(float x)
{

#define MIN32(a,b) ((a) < (b) ? (a) : (b))   /**< Minimum 32-bit value.   */
#define MAX32(a,b) ((a) > (b) ? (a) : (b))   /**< Maximum 32-bit value.   */
#define float2int(x) lrintf(x)

    x = x * 32768.f;
    x = MAX32(x, -32768);
    x = MIN32(x, 32767);
    return (int16_t)float2int(x);
}

static void iamf_decoder_plane2stride_out_short (void *dst, const float *src,
        int frame_size, int channels)
{
    int16_t *short_dst = (int16_t *)dst;

    ia_logd("channels %d", channels);
    for (int c = 0; c<channels; ++c) {
        if (src) {
            for (int i=0; i<frame_size; i++) {
                short_dst[i * channels + c] = FLOAT2INT16(src[frame_size * c + i]);
            }
        } else {
            for (int i=0; i<frame_size; i++) {
                short_dst[i * channels + c] = 0;
            }
        }
    }
}
#if SR
static void ia_decoder_plane2stride_out_float(void *dst, const float *src,
    int frame_size, int channels)
{
    float *float_dst = (float *)dst;

    ia_logd("channels %d", channels);
    for (int c = 0; c<channels; ++c) {
        if (src) {
            for (int i = 0; i<frame_size; i++) {
                float_dst[i * channels + c] = src[frame_size * c + i];
            }
        }
        else {
            for (int i = 0; i<frame_size; i++) {
                float_dst[i * channels + c] = 0;
            }
        }
    }
}
#endif



static int iamf_sound_system_valid(IAMF_SoundSystem ss)
{
    return ss >= SOUND_SYSTEM_A && ss <= SOUND_SYSTEM_EXT_312;
}

static int iamf_sound_system_channels_count_without_lfe (IAMF_SoundSystem ss)
{
    static int ss_channels[] = { 2, 5, 7, 9, 10, 10, 13, 22, 7, 11, 9, 5 };
    return ss_channels[ss];
}

static int iamf_sound_system_lfe1 (IAMF_SoundSystem ss)
{
    return ss != SOUND_SYSTEM_A;
}

static int iamf_sound_system_lfe2 (IAMF_SoundSystem ss)
{
    return  ss == SOUND_SYSTEM_F || ss == SOUND_SYSTEM_H;
}

static uint32_t iamf_sound_system_get_rendering_id (IAMF_SoundSystem ss)
{
    static IAMF_SOUND_SYSTEM ss_rids[] = { BS2051_A, BS2051_B, BS2051_C, BS2051_D, BS2051_E, BS2051_F, BS2051_G, BS2051_H, BS2051_I, BS2051_J, IAMF_712, IAMF_312 };
    return ss_rids[ss];
}

static uint32_t iamf_layer_layout_get_rendering_id (int layer_layout)
{
    static IAMF_SOUND_SYSTEM l_rids[] = { IAMF_MONO, IAMF_STEREO, IAMF_51, IAMF_512, IAMF_514, IAMF_71, IAMF_712, IAMF_714, IAMF_312, IAMF_BINAURAL };
    return l_rids[layer_layout];
}

static int iamf_layer_layout_lfe1 (int layer_layout)
{
    return layer_layout > IA_CHANNEL_LAYOUT_STEREO
           && layer_layout < IA_CHANNEL_LAYOUT_BINAURAL;
}

static IAMF_SoundSystem iamf_layer_layout_convert_sound_system (int layout)
{
    static IAMF_SoundSystem layout2ss[] = { -1, SOUND_SYSTEM_A, SOUND_SYSTEM_B, SOUND_SYSTEM_C, SOUND_SYSTEM_D, SOUND_SYSTEM_I, SOUND_SYSTEM_EXT_712, SOUND_SYSTEM_J, SOUND_SYSTEM_EXT_712 };
    if (ia_channel_layout_type_check(layout))
        return layout2ss[layout];
    return -1;
}

#if 0
static int
iamf_layout_channels_count_without_lfe (LayoutInfo *layout)
{
    int ret = 0;
    if (!layout->sp.sp_type) {
        ret = iamf_sound_system_channels_count_without_lfe(layout->sound_system);
    } else {
        ; // TODO custom layout.
    }
    return ret;
}
#endif

static int iamf_layout_lfe1 (IAMF_Layout *layout)
{
    if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
        return iamf_sound_system_lfe1(layout->sound_system.sound_system);
    }
    return 0;

}

static int iamf_layout_lfe2 (IAMF_Layout *layout)
{
    if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
        return iamf_sound_system_lfe1(layout->sound_system.sound_system);
    }
    return 0;
}

static int iamf_layout_channels_count (IAMF_Layout *layout)
{
    int ret = 0;
    if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
        ret = iamf_sound_system_channels_count_without_lfe(layout->sound_system.sound_system);
        ret += iamf_sound_system_lfe1(layout->sound_system.sound_system);
        ret += iamf_sound_system_lfe2(layout->sound_system.sound_system);
        ia_logd("sound system %x, channels %d", layout->sound_system.sound_system, ret);
    } else if (layout->type == IAMF_LAYOUT_TYPE_BINAURAL) {
        ret = 2;
        ia_logd("binaural channels %d", ret);
    }

    return ret;
}

static int iamf_layout_lfe_check (IAMF_Layout *layout)
{
    int ret = 0;
    ret += iamf_layout_lfe1(layout);
    ret += iamf_layout_lfe2(layout);
    return !!ret;
}


static void iamf_layout_reset (IAMF_Layout *layout)
{
    if (layout) {
        if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL &&
                layout->sp_labels.sp_label)  {
            free (layout->sp_labels.sp_label);
        }
        memset(layout, 0, sizeof(IAMF_Layout));
    }
}

static int iamf_layout_copy (IAMF_Layout *dst, IAMF_Layout *src)
{
    memcpy (dst, src, sizeof(IAMF_Layout));
    if (src->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
        dst->sp_labels.sp_label = IAMF_MALLOCZ(uint8_t, src->sp_labels.num_loudspeakers);
        if (dst->sp_labels.sp_label) {
            for (int i=0; i<src->sp_labels.num_loudspeakers; ++i)
                dst->sp_labels.sp_label[i] = src->sp_labels.sp_label[i];
        }
    }
    return IAMF_OK;
}

static int iamf_layout_copy2 (IAMF_Layout *dst, TargetLayout *src)
{
    dst->type = src->type;
    if (src->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
        SoundSystemLayout *layout = SOUND_SYSTEM_LAYOUT(src);
        dst->sound_system.sound_system = layout->sound_system;
    }
    return IAMF_OK;
}

static void iamf_layout_dump (IAMF_Layout *layout)
{
    if (layout) {
        ia_logt("layout type %d", layout->type);
        if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL)  {
            ia_logt("number sp labels %d", layout->sp_labels.num_loudspeakers);
            for (int i=0; i<layout->sp_labels.num_loudspeakers; ++i)
                ia_logt("sp label %d : %d", i, layout->sp_labels.sp_label[i] & U8_MASK);
        } else if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
            ia_logt("sound system %d", layout->sound_system.sound_system);
        }
    }
}

static void iamf_layout_info_free (LayoutInfo *layout)
{
    if (layout) {
        if (layout->sp.sp_layout.predefined_sp)
            free (layout->sp.sp_layout.predefined_sp);
        iamf_layout_reset(&layout->layout);
        free (layout);
    }
}

static void
iamf_recon_channels_order_update (IAChannelLayoutType layout, IAMF_ReconGain *re)
{
    int chs = 0;
    static IAReconChannel recon_channel_order[] = {
        IA_CH_RE_L, IA_CH_RE_C, IA_CH_RE_R, IA_CH_RE_LS, IA_CH_RE_RS,
        IA_CH_RE_LTF, IA_CH_RE_RTF,
        IA_CH_RE_LB, IA_CH_RE_RB, IA_CH_RE_LTB, IA_CH_RE_RTB, IA_CH_RE_LFE
    };

    static IAChannel channel_layout_map[IA_CHANNEL_LAYOUT_COUNT][IA_CH_RE_COUNT] = {
        {
            IA_CH_MONO, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID
        },
        {
            IA_CH_L2, IA_CH_INVALID, IA_CH_R2, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID
        },
        {
            IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE
        },
        {
            IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HL, IA_CH_HR,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE
        },
        {
            IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HFL, IA_CH_HFR,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE
        },
        {
            IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_INVALID, IA_CH_INVALID,
            IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE
        },
        {
            IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HL, IA_CH_HR,
            IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE
        },
        {
            IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HFL, IA_CH_HFR,
            IA_CH_BL7, IA_CH_BR7, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE
        },
        {
            IA_CH_L3, IA_CH_C, IA_CH_R3, IA_CH_INVALID, IA_CH_INVALID, IA_CH_TL, IA_CH_TR,
            IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE
        }
    };

#define RECON_CHANNEL_FLAG(c) RSHIFT(c)

    for (int c=0; c<IA_CH_RE_COUNT; ++c) {
        if (re->flags & RECON_CHANNEL_FLAG(recon_channel_order[c]))
            re->order[chs++] =
                channel_layout_map[layout][recon_channel_order[c]];
    }
}


static int
iamf_channel_layout_get_new_channels (IAChannelLayoutType last,
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
        uint32_t s1 = ia_channel_layout_get_category_channels_count (last,
                      IA_CH_CATE_SURROUND);
        uint32_t s2 = ia_channel_layout_get_category_channels_count (cur,
                      IA_CH_CATE_SURROUND);
        uint32_t t1 = ia_channel_layout_get_category_channels_count (last,
                      IA_CH_CATE_TOP);
        uint32_t t2 = ia_channel_layout_get_category_channels_count (cur,
                      IA_CH_CATE_TOP);

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
        chs = IAMF_ERR_BUFFER_TOO_SMALL;
    }
    return chs;
}

static IAChannel
iamf_output_gain_channel_map (IAChannelLayoutType type, IAOutputGainChannel gch)
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
    }
    break;

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
    }
    break;

    case IA_CH_GAIN_LS: {
        if (ia_channel_layout_get_category_channels_count(type,
                IA_CH_CATE_SURROUND) == 5) {
            ch = IA_CH_SL5;
        }
    }
    break;

    case IA_CH_GAIN_RS: {
        if (ia_channel_layout_get_category_channels_count(type,
                IA_CH_CATE_SURROUND) == 5) {
            ch = IA_CH_SR5;
        }
    }
    break;

    case IA_CH_GAIN_LTF: {
        if (ia_channel_layout_get_category_channels_count(type,
                IA_CH_CATE_SURROUND) < 5) {
            ch = IA_CH_TL;
        } else {
            ch = IA_CH_HL;
        }
    }
    break;

    case IA_CH_GAIN_RTF: {
        if (ia_channel_layout_get_category_channels_count(type,
                IA_CH_CATE_SURROUND) < 5) {
            ch = IA_CH_TR;
        } else {
            ch = IA_CH_HR;
        }
    }
    break;
    default:
        break;
    }

    return ch;
}


static IACodecID iamf_codec_4cc_get_codecID (uint32_t id)
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

    case TAG('l', 'p', 'c', 'm'):
        return IA_CODEC_PCM;

    default:
        return IA_CODEC_UNKNOWN;
    }
}

static int iamf_codec_get_delay (IACodecID cid)
{
    if (cid == IA_CODEC_AAC)
        return AAC_DELAY;
    else if (cid == IA_CODEC_OPUS)
        return OPUS_DELAY;
    return 0;
}

/* ----------------------------- Internal Interfaces--------------- */

static uint32_t iamf_decoder_internal_read_descriptors_OBUs (
    IAMF_DecoderHandle handle, const uint8_t *data, uint32_t size);
static int32_t iamf_decoder_internal_add_descrptor_OBU (
    IAMF_DecoderHandle handle, IAMF_OBU *obu);
static IAMF_StreamDecoder *iamf_stream_decoder_open(IAMF_Stream *stream,
        IAMF_CodecConf *conf);
static int iamf_decoder_internal_deliver (IAMF_DecoderHandle handle,
        IAMF_Frame *obj);
static int iamf_stream_scale_decoder_decode (IAMF_StreamDecoder *decoder,
        float *pcm);
static int iamf_stream_scale_demixer_configure (IAMF_StreamDecoder *decoder);
static int
iamf_stream_scale_decoder_update_parameters (IAMF_StreamDecoder *decoder,
        IAMF_DataBase *db, uint64_t timestamp);
static int32_t
iamf_stream_scale_decoder_demix (IAMF_StreamDecoder *decoder, float *src,
                                 float *dst, uint32_t frame_size);
static int iamf_stream_ambisonics_decoder_decode (IAMF_StreamDecoder *decoder,
        float *pcm);


/* >>>>>>>>>>>>>>>>>> DATABASE >>>>>>>>>>>>>>>>>> */

static void iamf_database_reset (IAMF_DataBase *db);
static IAMF_CodecConf*
iamf_database_get_codec_conf (IAMF_DataBase *db, uint64_t cid);
static ElementItem*
iamf_database_element_get_item (IAMF_DataBase *db, uint64_t eid);

static void iamf_object_free (void *obj)
{
    IAMF_object_free(IAMF_OBJ(obj));
}

static ObjectSet* iamf_object_set_new (IAMF_Free func)
{
    ObjectSet *s = IAMF_MALLOCZ(ObjectSet, 1);
    if (s) {
        s->objFree = func;
    }

    return s;
}

static void iamf_object_set_free (ObjectSet *s)
{
    if (s) {
        if (s->objFree) {
            for (int i=0; i<s->count; ++i)
                s->objFree(s->items[i]);
            if(s->items)
              free(s->items);
        }
        free (s);
    }
}

#define CAP_DEFAULT 6
static int iamf_object_set_add (ObjectSet *s, void *item)
{
    if (!item)
        return IAMF_ERR_BAD_ARG;

    if (s->count == s->capacity) {
        void **cap = 0;
        if (!s->count) {
            cap = IAMF_MALLOCZ (void *, CAP_DEFAULT);
        } else {
            cap = IAMF_REALLOC (void *, s->items, s->capacity + CAP_DEFAULT);
        }
        if (!cap)
            return IAMF_ERR_ALLOC_FAIL;
        s->items = cap;
        s->capacity += CAP_DEFAULT;
    }

    s->items[s->count++] = item;
    return IAMF_OK;
}

static uint32_t
iamf_object_element_get_recon_gain_flags (IAMF_Element *e)
{
    uint32_t ret = 0;

    if (e->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED
            && e->channels_conf) {
        for (int i=0; i<e->channels_conf->nb_layers; ++i) {
            if (e->channels_conf->layer_conf_s
                    && e->channels_conf->layer_conf_s->recon_gain_flag)
                ret |= RSHIFT(i);
        }
    }
    return ret;
}


static ParameterSegment*
iamf_object_parameter_get_segment(IAMF_Parameter *obj, uint64_t timestamp)
{
    ParameterSegment   *seg;
    uint64_t            ts = 0;

    if (!obj)
        return 0;

    seg = obj->segments[0];
    for (int i=1; i<obj->nb_segments; ++i) {
        ts += seg->segment_interval;
        if (ts > timestamp)
            break;
        seg = obj->segments[i];
    }
    return seg;
}


static uint64_t
iamf_database_object_get_timestamp (IAMF_DataBase *db, uint64_t id)
{
    Viewer     *v = db->syncViewer;
    SyncItem   *si = (SyncItem *)v->items;
    uint64_t    ts = 0;

    for (int i=0; i<v->count; ++i) {
        if (si[i].id == id) {
            ts = si[i].start;
            break;
        }
    }

    return ts;
}

static ParameterItem*
iamf_database_parameter_viewer_get_item (ParameterViewer *viewer, uint64_t pid)
{
    ParameterItem* pi = 0;
    for (int i=0; i<viewer->count; ++i) {
        if (viewer->items[i]->id == pid) {
            pi = viewer->items[i];
            break;
        }
    }
    return pi;
}


static int
iamf_database_parameter_viewer_add_item (IAMF_DataBase *db, uint64_t pid, uint64_t type, void *parent)
{
    ParameterViewer    *pv = &db->pViewer;
    ParameterItem      *pi = 0;
    ParameterItem     **pis = 0;
    ElementItem        *ei = 0;

    pi = iamf_database_parameter_viewer_get_item (pv, pid);

    if (pi)
        return IAMF_OK;

    pis = IAMF_REALLOC(ParameterItem *, pv->items, pv->count + 1);
    if (!pis) {
        return IAMF_ERR_ALLOC_FAIL;
    }
    pv->items = pis;
    pis[pv->count] = 0;

    pi = IAMF_MALLOCZ(ParameterItem, 1);
    if (!pi) {
        return IAMF_ERR_ALLOC_FAIL;
    }

    pis[pv->count++] = pi;
    ia_logt("add parameter item %p, its id %lu, and count is %d", pi, pid, pv->count);

    pi->id = pid;
    pi->type = type;
    pi->parent = parent;
    if (type == PARAMETER_TYPE_DEMIXING) {
        ei = (ElementItem *)pi->parent;
        ei->demixing = pi;
    } else if (type == PARAMETER_TYPE_RECON_GAIN) {
        ei = (ElementItem *)pi->parent;
        ei->reconGain = pi;
    }
    return IAMF_OK;
}

static int
iamf_database_parameter_viewer_add (IAMF_DataBase *db, IAMF_Object *obj)
{
    ParameterViewer    *pv = &db->pViewer;
    IAMF_Parameter     *p = (IAMF_Parameter *)obj;
    ParameterItem      *pi = 0;

    pi = iamf_database_parameter_viewer_get_item (pv, p->id);
    if (pi) {
        ElementItem *ei;
        if (pi->parameter) {
            for (int i=0; i<db->parameters->count; ++i) {
                if (pi->parameter == db->parameters->items[i]) {

                    iamf_object_free(db->parameters->items[i]);
                    db->parameters->items[i] = obj;
                }
            }
        } else {
            iamf_object_set_add(db->parameters, obj);
        }
        pi->parameter = p;
        ei = (ElementItem *)pi->parent;
        if (ei)
            pi->timestamp = ei->timestamp;
        ia_logd ("parameter id %lu, timestamp update %lu", pi->id, pi->timestamp);
    } else {
        iamf_object_set_add(db->parameters, obj);
    }

    return IAMF_OK;
}

static void iamf_database_parameter_viewer_free (ParameterViewer *v)
{
    if (v) {
        if (v->items) {
            for (int i=0; i<v->count; ++i)
                free(v->items[i]);
            free(v->items);
        }
        memset(v, 0, sizeof(ParameterViewer));
    }
}

static uint64_t
iamf_database_parameter_viewer_get_type (IAMF_DataBase *db, uint64_t pid)
{
    uint64_t    type = INVALID_ID;
    for (int i=0; i<db->pViewer.count; ++i) {
        if (db->pViewer.items[i]->id == pid) {
            type = db->pViewer.items[i]->type;
            break;
        }
    }
    return type;
}

static int
iamf_database_element_viewer_add (IAMF_DataBase *db, IAMF_Object *obj)
{
    int             ret = IAMF_OK;
    ElementItem    *eItem = 0;
    ElementViewer  *v = &db->eViewer;
    IAMF_Element   *e = (IAMF_Element *)obj;

    eItem = IAMF_REALLOC (ElementItem, v->items, v->count + 1);
    if (!eItem)
        return IAMF_ERR_ALLOC_FAIL;

    v->items = eItem;
    ret = iamf_object_set_add (db->element, (void *)obj);
    if (ret != IAMF_OK) {
        return ret;
    }

    eItem = &v->items[v->count++];
    memset (eItem, 0, sizeof (ElementItem));

    eItem->id = e->element_id;
    eItem->element = IAMF_ELEMENT(obj);
    eItem->codecConf = iamf_database_get_codec_conf (db, e->codec_config_id);
    eItem->recon_gain_flags = iamf_object_element_get_recon_gain_flags(e);

    for (int i=0; i<e->nb_parameters; ++i) {
        iamf_database_parameter_viewer_add_item (db, e->parameters[i]->id,
                e->parameters[i]->type, (void *)eItem);
    }

    return ret;
}

static void iamf_database_element_viewer_reset (ElementViewer *v)
{
    if (v) {
        if (v->items)
            free(v->items);
        memset(v, 0, sizeof(ElementViewer));
    }
}

static int iamf_database_sync_time_update(IAMF_DataBase *db)
{
    ElementViewer  *ev = &db->eViewer;
    ElementItem    *ei = (ElementItem *)ev->items;

    ParameterViewer    *pv = &db->pViewer;
    ParameterItem      *pi;

    for (int i=0; i<ev->count; ++i) {
        ei->timestamp = iamf_database_object_get_timestamp(db, IAMF_frame_get_obu_type(ei->element->substream_ids[0]));
    }

    for (int i=0; i<pv->count; ++i) {
        pi = pv->items[i];
        pi->timestamp = iamf_database_object_get_timestamp(db, pi->id);
    }

    return IAMF_OK;
}

static uint64_t iamf_database_sync_viewer_max_start_timestamp (Viewer *v)
{
    uint64_t    ret = 0;
    SyncItem   *si;
    if (!v)
        return 0;
    si = (SyncItem *)v->items;
    ret = 0;
    for (int i=0; i<v->count; ++i)
        if (!si[i].type && ret < si[i].start)
            ret = si[i].start;
    return ret;
}

static void iamf_database_sync_viewer_free (Viewer *viewer)
{
    if (viewer) {
        if (viewer->items)
            free (viewer->items);
        free (viewer);
    }
}

static int iamf_database_sync_viewer_update (IAMF_DataBase *db, IAMF_Object *obj)
{
    Viewer     *v;
    IAMF_Sync  *s = (IAMF_Sync *)obj;
    int         ret = IAMF_OK;
    SyncItem   *si;

    v = IAMF_MALLOCZ(Viewer, 1);
    if (!v) {
        ia_loge("Fail to allocate memory for Sync Viewer.");
        ret = IAMF_ERR_ALLOC_FAIL;
        goto sync_fail;
    }

    si = IAMF_MALLOCZ(SyncItem, s->nb_obu_ids);
    if (!si) {
        ia_loge("Fail to allocate memory for Sync Items.");
        ret = IAMF_ERR_ALLOC_FAIL;
        goto sync_fail;
    }

    v->count = s->nb_obu_ids;
    v->items = si;

    for (int i=0; i<v->count; ++i) {
        si[i].id = s->objs[i].obu_id;
        si[i].type = s->objs[i].obu_data_type;
        si[i].start = iamf_database_sync_viewer_max_start_timestamp(db->syncViewer) + s->objs[i].relative_offset + s->global_offset;
        ia_logi("Item id %lu: type %d, start time %lu", si[i].id, s->objs[i].obu_data_type & U8_MASK, si[i].start);
    }

    if (db->sync)
        iamf_object_free(db->sync);
    if (db->syncViewer)
        iamf_database_sync_viewer_free (db->syncViewer);

    db->sync = obj;
    db->syncViewer = v;

    iamf_database_sync_time_update(db);

    return IAMF_OK;

sync_fail:
    iamf_database_sync_viewer_free (v);
    return ret;
}


static int
iamf_database_mix_presentation_get_label_index (IAMF_DataBase *db, const char* label)
{
    int                     idx = INAVLID_MIX_PRESENTATION_INDEX;
    IAMF_MixPresentation   *mp;
    for (int i=0; i<db->mixPresentation->count; ++i) {
        mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
        if (!strcmp(label, mp->mix_presentation_friendly_label)) {
            idx = i;
            break;
        }
    }
    return idx;
}

int iamf_database_init (IAMF_DataBase *db)
{
    memset (db, 0, sizeof (IAMF_DataBase));

    db->codecConf = iamf_object_set_new(iamf_object_free);
    db->element = iamf_object_set_new(iamf_object_free);
    db->mixPresentation = iamf_object_set_new(iamf_object_free);
    db->parameters = iamf_object_set_new(iamf_object_free);
    db->syncViewer = IAMF_MALLOCZ(Viewer, 1);

    if (!db->codecConf || !db->element || !db->mixPresentation || !db->parameters || !db->syncViewer) {
        iamf_database_reset(db);
        return IAMF_ERR_ALLOC_FAIL;
    }
    return 0;
}

void iamf_database_reset (IAMF_DataBase *db)
{
    if (db) {
        if (db->version)
            iamf_object_free(db->version);

        if (db->sync)
            iamf_object_free(db->sync);

        if (db->codecConf)
            iamf_object_set_free (db->codecConf);

        if (db->element)
            iamf_object_set_free (db->element);

        if (db->mixPresentation)
            iamf_object_set_free (db->mixPresentation);

        if (db->parameters)
            iamf_object_set_free (db->parameters);

        if (db->eViewer.items)
            iamf_database_element_viewer_reset (&db->eViewer);

        if (db->pViewer.items)
            iamf_database_parameter_viewer_free (&db->pViewer);

        iamf_database_sync_viewer_free (db->syncViewer);

        memset (db, 0, sizeof(IAMF_DataBase));
    }
}

static int iamf_database_add_object (IAMF_DataBase *db, IAMF_Object *obj)
{
    int ret = IAMF_OK;
    switch (obj->type) {
        case IAMF_OBU_MAGIC_CODE:
            if (db->version) {
                ia_logw("WARNING : Receive Multiple START CODE OBUs !!!");
                free (db->version);
            }
            db->version = obj;
            break;
        case IAMF_OBU_CODEC_CONFIG:
            ret = iamf_object_set_add (db->codecConf, (void *) obj);
            break;
        case IAMF_OBU_AUDIO_ELEMENT:
            ret = iamf_database_element_viewer_add (db, obj);
            break;
        case IAMF_OBU_MIX_PRESENTATION:
            ret = iamf_object_set_add (db->mixPresentation, (void *)obj);
            break;
        case IAMF_OBU_PARAMETER_BLOCK:
            ret = iamf_database_parameter_viewer_add (db, obj);
            break;
        case IAMF_OBU_SYNC:
            ret = iamf_database_sync_viewer_update (db, obj);
            break;
        default:
            ia_logd("IAMF Object %s (%d) is not needed in database.",
                    IAMF_OBU_type_string(obj->type), obj->type);
            IAMF_object_free(obj);
    }
    return ret;
}

IAMF_CodecConf *
iamf_database_get_codec_conf (IAMF_DataBase *db, uint64_t cid)
{
    IAMF_CodecConf *ret = 0;

    if (db->codecConf) {
        IAMF_CodecConf *c = 0;
        for (uint32_t i=0; i<db->codecConf->count; ++i) {
            c = (IAMF_CodecConf *)db->codecConf->items[i];
            if (c->codec_conf_id == cid) {
                ret = c;
                break;
            }
        }
    }
    return ret;
}


static IAMF_Element *
iamf_database_get_element (IAMF_DataBase *db, uint64_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    return ei ? ei->element : 0;
}


static IAMF_Element*
iamf_database_get_element_by_parameterID (IAMF_DataBase *db, uint64_t pid)
{
    IAMF_Element   *elem = 0;
    IAMF_Element   *e = 0;
    for (int i=0; i<db->eViewer.count; ++i) {
        e = IAMF_ELEMENT(db->eViewer.items[i].element);
        for (int p=0; p<e->nb_parameters; ++p) {
            if (pid == e->parameters[p]->id) {
                elem = e;
                break;
            }
        }
    }
    return elem;
}


ElementItem* iamf_database_element_get_item (IAMF_DataBase *db, uint64_t eid)
{
    ElementItem    *ei = 0;
    IAMF_Element   *e = 0;
    for (int i=0; i<db->eViewer.count; ++i) {
        e = (IAMF_Element *)db->eViewer.items[i].element;
        if (e && e->element_id == eid) {
            ei = &db->eViewer.items[i];
            break;
        }
    }
    return ei;
}

static IAMF_CodecConf*
iamf_database_element_get_codec_conf (IAMF_DataBase *db, uint64_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    return ei ? ei->codecConf : 0;
}

static int
iamf_database_element_get_substream_index (IAMF_DataBase *db, uint64_t element_id,
        uint64_t substream_id)
{
    IAMF_Element *obj = iamf_database_get_element(db, element_id);
    int ret = -1;

    for (int i=0; i<obj->nb_substreams; ++i) {
        if (obj->substream_ids[i] == substream_id) {
            ret = i;
            break;
        }
    }
    return ret;
}

static uint64_t
iamf_database_element_get_timestamp (IAMF_DataBase *db, uint32_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    return ei ? ei->timestamp : INVALID_ID;
}

static uint32_t
iamf_database_element_get_recon_gain_flags (IAMF_DataBase *db, uint32_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    return ei ? ei->recon_gain_flags : 0;
}

static int
iamf_database_element_time_elapse (IAMF_DataBase *db, uint64_t eid, uint64_t duration)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    SyncItem       *si = (SyncItem *)db->syncViewer->items;

    if (!ei)
        return IAMF_ERR_BAD_ARG;
    ei->timestamp += duration;

    for (int i=0; i<ei->element->nb_substreams; ++i) {
        for (int a=0; a<db->syncViewer->count; ++a) {
            if (ei->element->substream_ids[i] == si[a].id) {
                si[a].start = ei->timestamp;
                break;
            }
        }
    }

    return IAMF_OK;
}

static int
iamf_database_element_get_demix_mode (IAMF_DataBase *db, uint64_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    ParameterItem  *pi = 0;
    DemixingSegment    *seg = 0;
    uint64_t            start = 0;
    if (!ei)
        return IAMF_ERR_BAD_ARG;

    pi = ei->demixing;
    if (!pi)
        return IAMF_ERR_BAD_ARG;

    if (ei->timestamp > pi->timestamp)
        start = ei->timestamp - pi->timestamp;
    seg = (DemixingSegment *)iamf_object_parameter_get_segment(pi->parameter, start);

    if (seg)
        return seg->demixing_mode;
    return IAMF_ERR_INTERNAL;
}

static ReconGainList*
iamf_database_element_get_recon_gain_list (IAMF_DataBase *db, uint64_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    ParameterItem  *pi = 0;
    ReconGainSegment   *seg = 0;
    uint64_t            start = 0;
    if (!ei)
        return 0;

    pi = ei->reconGain;
    if (!pi)
        return 0;

    if (ei->timestamp > pi->timestamp)
        start = ei->timestamp - pi->timestamp;
    seg = (ReconGainSegment *)iamf_object_parameter_get_segment(pi->parameter, start);

    if (seg)
        return &seg->list;

    return 0;
}

static int
iamf_database_element_set_mix_gain_parameter (IAMF_DataBase *db, uint64_t eid, uint64_t pid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    ParameterItem  *pi = iamf_database_parameter_viewer_get_item(&db->pViewer, pid);

    if (ei) {
        ei->mixGain = pi;
    }

    return IAMF_OK;
}

static float
iamf_database_element_get_mix_gain (IAMF_DataBase *db, uint64_t eid)
{
    ElementItem    *ei = iamf_database_element_get_item(db, eid);
    ParameterItem  *pi;

    if (!ei)
        return 1.0;

    pi = ei->mixGain;
    if (!pi)
        return 1.0;

    if (pi->parameter) {
        ia_logw("Do not support dynamic mix gain.");
        return 1.0;
    }

    return pi->defaultValue.mixGain;
}

static int
iamf_database_element_info_query (void *obj, uint64_t eid, uint32_t option)
{
    IAMF_DataBase  *db = (IAMF_DataBase *) obj;
    ElementItem    *ei;
    int             ret = IAMF_ERR_INTERNAL;

    if (!db)
        return IAMF_ERR_BAD_ARG;

    ei = iamf_database_element_get_item(db, eid);
    if (!ei)
        return IAMF_ERR_BAD_ARG;

    switch (option) {
        case OPTION_ELEMENT_TYPE:
            if (ei->element)
                ret = ei->element->element_type;
            break;
        case OPTION_ELEMENT_CHANNELS:
            ret = 0;
            break;
        default:
            ia_loge("invalid option %u of element %lu", option, eid);
            break;
    }

    return ret;
}

/* <<<<<<<<<<<<<<<<<< DATABASE <<<<<<<<<<<<<<<<<< */



/* <<<<<<<<<<<<<<<<<< STREAM DECODER MIXER <<<<<<<<<<<<<<<<<< */

static int iamf_stream_set_output_layout (IAMF_Stream *s, LayoutInfo *layout);
static uint32_t iamf_stream_mode_ambisonics (uint32_t ambisonics_mode);
static void iamf_stream_free (IAMF_Stream *s);
static void iamf_stream_decoder_close (IAMF_StreamDecoder *d);
static void iamf_mixer_reset(IAMF_Mixer *m);

static void iamf_presentation_free (IAMF_Presentation *pst)
{
    if (pst) {
        for (int i=0; i<pst->nb_streams; ++i) {
            if (pst->decoders[i]) {
              iamf_stream_decoder_close(pst->decoders[i]);
            }

            if (pst->streams[i]) {
                iamf_stream_free(pst->streams[i]);
            }
        }
        free(pst->decoders);
        free(pst->streams);
        iamf_mixer_reset (&pst->mixer);
        free(pst);
    }
}

static IAMF_Stream*
iamf_presentation_take_stream (IAMF_Presentation *pst, uint64_t eid)
{
    IAMF_Stream    *stream = 0;

    if (!pst)
        return 0;

    for (int i=0; i<pst->nb_streams; ++i) {
        if (pst->streams[i]->element_id == eid) {
            stream = pst->streams[i];
            pst->streams[i] = 0;
            break;
        }
    }

    return stream;
}

static IAMF_StreamDecoder*
iamf_presentation_take_decoder (IAMF_Presentation *pst, IAMF_Stream *stream)
{
    IAMF_StreamDecoder *decoder = 0;
    for (int i=0; i<pst->nb_streams; ++i) {
        if (pst->decoders[i]->stream == stream) {
            decoder = pst->decoders[i];
            pst->decoders[i] = 0;
            break;
        }
    }

    return decoder;
}

static int
iamf_presentation_reuse_stream (IAMF_Presentation *dst, IAMF_Presentation *src, uint64_t eid)
{
    IAMF_Stream            *stream = 0;
    IAMF_StreamDecoder     *decoder = 0;
    IAMF_Stream           **streams;
    IAMF_StreamDecoder    **decoders;

    if (!dst || !src)
        return IAMF_ERR_BAD_ARG;

    stream = iamf_presentation_take_stream (src, eid);
    if (!stream)
        return IAMF_ERR_INTERNAL;

    decoder = iamf_presentation_take_decoder (src, stream);
    if (!decoder)
        return IAMF_ERR_INTERNAL;

    streams = IAMF_REALLOC(IAMF_Stream *, dst->streams, dst->nb_streams + 1);
    if (!streams)
        return IAMF_ERR_INTERNAL;
    dst->streams = streams;

    decoders = IAMF_REALLOC(IAMF_StreamDecoder *, dst->decoders, dst->nb_streams + 1);
    if (!decoders)
        return IAMF_ERR_INTERNAL;
    dst->decoders = decoders;

    dst->streams[dst->nb_streams] = stream;
    dst->decoders[dst->nb_streams] = decoder;
    ++dst->nb_streams;
    ia_logd ("reuse stream with element id %lu", eid);

    return 0;
}

void iamf_stream_free (IAMF_Stream *s)
{
    if (s) {
        if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED)  {
            ChannelLayerContext    *ctx = s->priv;
            if (ctx) {
                if (ctx->conf_s) {
                    for (int i=0; i<ctx->nb_layers; ++i) {
                        if (ctx->conf_s[i].output_gain) {
                            free (ctx->conf_s[i].output_gain);
                        }
                        if (ctx->conf_s[i].recon_gain) {
                            free (ctx->conf_s[i].recon_gain);
                        }
                    }
                    free (ctx->conf_s);
                }
                free (ctx);
            }
        }
        else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
          AmbisonicsContext    *ctx = s->priv;
          free(ctx);
        }
        free (s);
    }
}

static IAMF_Stream *
iamf_stream_new (IAMF_Element *elem, IAMF_CodecConf *conf, LayoutInfo *layout)
{
    IAMF_Stream *stream = IAMF_MALLOCZ (IAMF_Stream, 1);
    if (!stream)
        goto stream_fail;

    stream->element_id = elem->element_id;
    stream->scheme = elem->element_type;
    stream->codecConf_id = conf->codec_conf_id;
    stream->codec_id = iamf_codec_4cc_get_codecID(conf->codec_id);
    stream->nb_substreams = elem->nb_substreams;
    stream->timestamp = 0;
    stream->duration = 0;
    stream->pts = 0;
    stream->dts = 0;

    ia_logd("codec conf id %lu", conf->codec_conf_id);
    if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
        ChannelLayerContext    *ctx = IAMF_MALLOCZ(ChannelLayerContext, 1);
        SubLayerConf           *sub_conf;
        ChannelLayerConf       *layer_conf;
        ScalableChannelLayoutConf  *layers_conf = elem->channels_conf;
        float                   gain_db;
        int                     chs = 0;
        IAChannelLayoutType     last = IA_CHANNEL_LAYOUT_INVALID;


        if (!ctx) {
            goto stream_fail;
        }

        stream->priv = (void *)ctx;
        ctx->nb_layers = layers_conf->nb_layers;
        if (ctx->nb_layers) {
            sub_conf = IAMF_MALLOCZ(SubLayerConf, ctx->nb_layers);
            if (!sub_conf) {
                goto stream_fail;
            }

            ctx->conf_s = sub_conf;
            for (int i=0; i<ctx->nb_layers; ++i) {
                sub_conf = &ctx->conf_s[i];
                layer_conf = &layers_conf->layer_conf_s[i];
                sub_conf->layout = layer_conf->loudspeaker_layout;
                sub_conf->nb_substreams = layer_conf->nb_substreams;
                sub_conf->nb_coupled_substreams = layer_conf->nb_coupled_substreams;
                sub_conf->nb_channels = sub_conf->nb_substreams +
                                        sub_conf->nb_coupled_substreams;

                ia_logi("audio layer %d :", i);
                ia_logi(" > loudspeaker layout %s(%d) .",
                        ia_channel_layout_name(sub_conf->layout), sub_conf->layout);
                ia_logi(" > sub-stream count %d .", sub_conf->nb_substreams);
                ia_logi(" > coupled sub-stream count %d .", sub_conf->nb_coupled_substreams);

                if (layer_conf->output_gain_flag) {
                    sub_conf->output_gain = IAMF_MALLOCZ(IAMF_OutputGain, 1);
                    if (!sub_conf->output_gain) {
                        ia_loge("Fail to allocate memory for output gain of sub config.");
                        goto stream_fail;
                    }
                    sub_conf->output_gain->flags = layer_conf->output_gain_info->output_gain_flag;
                    gain_db =
                        q_to_float(layer_conf->output_gain_info->output_gain, 8);
                    sub_conf->output_gain->gain = db2lin(gain_db);
                    ia_logi(" > output gain flags 0x%02x",
                            sub_conf->output_gain->flags & U8_MASK);
                    ia_logi(" > output gain %f (0x%04x), linear gain %f",
                            gain_db, layer_conf->output_gain_info->output_gain & U16_MASK,
                            sub_conf->output_gain->gain);
                } else {
                    ia_logi(" > no output gain info.");
                }

                if (layer_conf->recon_gain_flag) {
                    sub_conf->recon_gain = IAMF_MALLOCZ(IAMF_ReconGain, 1);
                    if (!sub_conf->recon_gain) {
                        ia_loge("Fail to allocate memory for recon gain of sub config.");
                        goto stream_fail;
                    }
                    ctx->recon_gain_flags |= RSHIFT(i);
                    ia_logi(" > wait recon gain info.");
                } else {
                    ia_logi(" > no recon gain info.");
                }

                chs += iamf_channel_layout_get_new_channels(last, sub_conf->layout,
                        &ctx->channels_order[chs], IA_CH_LAYOUT_MAX_CHANNELS - chs);

                stream->nb_coupled_substreams += sub_conf->nb_coupled_substreams;

                ia_logi(" > the total of %d channels", chs);
                last = sub_conf->layout;
            }
        }
        stream->nb_channels = stream->nb_substreams + stream->nb_coupled_substreams;

        ia_logi ("channels %d, streams %d, coupled streams %d.", stream->nb_channels,
                 stream->nb_substreams, stream->nb_coupled_substreams);


        ia_logi("all channels order:");
        for (int c=0; c<stream->nb_channels; ++c)
            ia_logi("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
                    ctx->channels_order[c]);


        ctx->layer = ctx->nb_layers - 1;
        iamf_stream_set_output_layout(stream, layout);
        ctx->layout = ctx->conf_s[ctx->layer].layout;
        ctx->channels = ia_channel_layout_get_channels_count(ctx->layout);

        ia_logi("initialized layer %d, layout %s (%d), layout channel count %d.",
                ctx->layer, ia_channel_layout_name(ctx->layout), ctx->layout,
                ctx->channels);

    } else {
        AmbisonicsConf     *aconf = elem->ambisonics_conf;
        AmbisonicsContext  *ctx;
        stream->nb_channels = aconf->output_channel_count;
        stream->nb_substreams = aconf->substream_count;
        stream->nb_coupled_substreams = aconf->coupled_substream_count;

        ctx = IAMF_MALLOCZ(AmbisonicsContext, 1);
        if (!ctx) {
            goto stream_fail;
        }

        stream->priv = (void *)ctx;
        ctx->mode = iamf_stream_mode_ambisonics(aconf->ambisonics_mode);
        ctx->mapping = aconf->mapping;
        ctx->mapping_size = aconf->mapping_size;

        iamf_stream_set_output_layout(stream, layout);
        ia_logi("stream mode %d", ctx->mode);
    }
    return stream;

stream_fail:

    if (stream)
        iamf_stream_free (stream);
    return 0;
}

uint32_t iamf_stream_mode_ambisonics (uint32_t ambisonics_mode)
{
    if (ambisonics_mode == AMBISONICS_MODE_MONO)
        return STREAM_MODE_AMBISONICS_MONO;
    else if (ambisonics_mode == AMBISONICS_MODE_PROJECTION)
        return STREAM_MODE_AMBISONICS_PROJECTION;
    return STREAM_MODE_AMBISONICS_NONE;
}

int iamf_stream_set_output_layout (IAMF_Stream *s, LayoutInfo *layout)
{
    s->final_layout = layout;

    if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
        ChannelLayerContext    *ctx = (ChannelLayerContext *)s->priv;
        if (ctx) {
            if (ctx->nb_layers == 1) {
                return IAMF_OK;
            }

            //use the layout that matches the playback layout
            for (int i=0; i<ctx->nb_layers; ++i) {
                if (iamf_layer_layout_convert_sound_system(ctx->conf_s[i].layout) == layout->layout.sound_system.sound_system) {
                    ctx->layer = i;
                    ia_logi("scalabel channels layer is %d", i);
                    return IAMF_OK;
                }
            }

            //select next highest available layout
            int playback_channels = IAMF_layout_sound_system_channels_count(layout->layout.sound_system.sound_system);
            for (int i = 0; i<ctx->nb_layers; ++i) {
                int channels = ia_channel_layout_get_channels_count(ctx->conf_s[i].layout);
                if (channels > playback_channels) {
                    ctx->layer = i;
                    ia_logi("scalabel channels layer is %d", i);
                    return IAMF_OK;
                }
            }
        }
    }

    return IAMF_OK;
}

static int32_t
iamf_stream_enable (IAMF_DecoderHandle handle, IAMF_Element *elem)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_DataBase          *db  = &ctx->db;
    IAMF_Presentation      *pst = ctx->presentation;
    uint64_t                element_id;
    IAMF_Stream            *stream = 0;
    IAMF_Stream           **streams;
    IAMF_CodecConf   *conf;
    IAMF_StreamDecoder     *decoder = 0;
    IAMF_StreamDecoder    **decoders;

    ia_logd("enable element id %lu", elem->element_id);
    element_id = elem->element_id;
    conf = iamf_database_element_get_codec_conf(db, element_id);
    ia_logd("codec conf id %lu", conf->codec_conf_id);

    stream = iamf_stream_new(elem, conf, ctx->output_layout);
    if (!stream)
        goto stream_enable_fail;

    decoder = iamf_stream_decoder_open(stream, conf);
    if (!decoder)
        goto stream_enable_fail;

    streams = IAMF_REALLOC(IAMF_Stream *, pst->streams, pst->nb_streams + 1);
    if (!streams)
        goto stream_enable_fail;
    pst->streams = streams;

    decoders = IAMF_REALLOC(IAMF_StreamDecoder *, pst->decoders, pst->nb_streams + 1);
    if (!decoders)
        goto stream_enable_fail;
    pst->decoders = decoders;

    pst->streams[pst->nb_streams] = stream;
    pst->decoders[pst->nb_streams] = decoder;
    ++pst->nb_streams;

    return 0;

  stream_enable_fail:
    if (decoder)
      iamf_stream_decoder_close(decoder);

    if (stream)
        iamf_stream_free (stream);

    return IAMF_ERR_ALLOC_FAIL;
}

static IACoreDecoder *
iamf_stream_sub_decoder_open (int mode, int channels, int nb_streams,
        int nb_coupled_streams, uint8_t* mapping, int mapping_size,
        IAMF_CodecConf *conf)
{
    IACodecID cid;
    IACoreDecoder *cDecoder;

    cid = iamf_codec_4cc_get_codecID (conf->codec_id);
    cDecoder = ia_core_decoder_open(cid);

    ia_core_decoder_set_codec_conf (cDecoder, conf->decoder_conf,
                                    conf->decoder_conf_size);
    ia_logd("mode %d, channels %d, streams %d, coupled streams %d, mapping size  %d",
            mode, channels, nb_streams, nb_coupled_streams, mapping_size);
    ia_core_decoder_set_streams_info (cDecoder, mode, channels, nb_streams, nb_coupled_streams, mapping, mapping_size);
    ia_core_decoder_init (cDecoder);

    return cDecoder;
}

void iamf_stream_decoder_close (IAMF_StreamDecoder *d)
{
    if (d) {
        IAMF_Stream *s = d->stream;
        if (d->packets)
            free (d->packets);
        if (d->sizes)
            free (d->sizes);

        for (int i=0; i<DEC_BUF_CNT; ++i) {
            if (d->buffers[i])
                free (d->buffers[i]);
        }

        if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
            if (d->scale) {
                if (d->scale->sub_decoders) {
                    for (int i=0; i<d->scale->nb_layers; ++i)
                        ia_core_decoder_close (d->scale->sub_decoders[i]);
                    free(d->scale->sub_decoders);
                }
                if (d->scale->demixer)
                    demixer_close(d->scale->demixer);
                free (d->scale);
            }
        } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
            if (d->ambisonics) {
                if (d->ambisonics->decoder)
                    ia_core_decoder_close (d->ambisonics->decoder);
                free(d->ambisonics);
            }
        }
        free (d);
    }
}

IAMF_StreamDecoder *
iamf_stream_decoder_open(IAMF_Stream *stream, IAMF_CodecConf *conf)
{
    IAMF_StreamDecoder *decoder;
    int                 channels = 0;

    decoder = IAMF_MALLOCZ(IAMF_StreamDecoder, 1);

    if (!decoder)
        goto open_fail;

    decoder->stream = stream;
    decoder->packets = IAMF_MALLOCZ(uint8_t *, stream->nb_substreams);
    decoder->sizes = IAMF_MALLOCZ(uint32_t, stream->nb_substreams);
    decoder->frame_size = conf->nb_samples_per_frame;

    if (!decoder->packets || !decoder->sizes)
        goto open_fail;

    ia_logt("check channels.");
    channels = iamf_layout_channels_count (&stream->final_layout->layout);
    ia_logd("final target channels vs stream original channels (%d vs %d).",
            channels, stream->nb_channels);
    if (channels < stream->nb_channels) {
        channels = stream->nb_channels;
    }
    for (int i=0; i<DEC_BUF_CNT; ++i) {
        decoder->buffers[i] = IAMF_MALLOC(float, MAX_FRAME_SIZE * channels);
        if (!decoder->buffers[i])
            goto open_fail;
    }

    if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
        ScalableChannelDecoder *scale = IAMF_MALLOCZ(ScalableChannelDecoder, 1);
        ChannelLayerContext    *ctx = (ChannelLayerContext *)stream->priv;
        IACoreDecoder         **sub_decoders;
        IACoreDecoder          *sub;

        if (!scale)
            goto open_fail;

        decoder->scale = scale;
        scale->nb_layers = ctx->layer + 1;
        sub_decoders = IAMF_MALLOCZ(IACoreDecoder *, scale->nb_layers);
        if (!sub_decoders)
            goto open_fail;
        scale->sub_decoders = sub_decoders;
        ia_logd("open sub decdoers.");
        for (int i=0; i<scale->nb_layers; ++i) {
            sub = iamf_stream_sub_decoder_open(STREAM_MODE_AMBISONICS_NONE,
                    ctx->conf_s[i].nb_channels, ctx->conf_s[i].nb_substreams,
                    ctx->conf_s[i].nb_coupled_substreams, 0, 0, conf);
            if (!sub)
                goto open_fail;
            sub_decoders[i] = sub;
        }

        ia_logd("open demixer.");
        scale->demixer = demixer_open(conf->nb_samples_per_frame, iamf_codec_get_delay(stream->codec_id));
        if (!scale->demixer)
            goto open_fail;
        iamf_stream_scale_demixer_configure(decoder);
    } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
        AmbisonicsDecoder  *a = IAMF_MALLOCZ(AmbisonicsDecoder, 1);
        AmbisonicsContext  *ctx = (AmbisonicsContext *)stream->priv;
        if (!a)
            goto open_fail;

        ia_logd("open sub decdoers.");
        a->decoder = iamf_stream_sub_decoder_open(ctx->mode, stream->nb_channels, stream->nb_substreams, stream->nb_coupled_substreams, ctx->mapping, ctx->mapping_size, conf);

        if (!a->decoder)
            goto open_fail;
        decoder->ambisonics = a;
    }

    return decoder;

open_fail:
    if (decoder)
        iamf_stream_decoder_close (decoder);
    return 0;
}

static int
iamf_stream_decoder_receive_packet(IAMF_StreamDecoder *decoder,
                                   int substream_index, IAMF_Frame *packet)
{
    uint8_t             *pkt = 0;

    ia_logd("stream decoder %lu , recevie sub stream %d",
            decoder->stream->element_id, substream_index);
    if (!decoder->packets[substream_index]) {
        pkt = IAMF_MALLOC(uint8_t, packet->size);
        memcpy(pkt, packet->data, packet->size);
        decoder->packets[substream_index] = pkt;
        decoder->sizes[substream_index] = packet->size;
        ++decoder->count;
    } else {
        return -1;
    }
    return 0;
}

static int
iamf_stream_decoder_update_parameters (IAMF_StreamDecoder *decoder,
                                       IAMF_DataBase *db, uint64_t ts)
{
    if (decoder->stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
        return iamf_stream_scale_decoder_update_parameters(decoder, db, ts);
    }
    return 0;
}

static int
iamf_stream_decoder_decode (IAMF_StreamDecoder *decoder, float *pcm)
{
    int ret = 0;
    IAMF_Stream *stream = decoder->stream;
    if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
        float *buffer = decoder->buffers[2];
        ret = iamf_stream_scale_decoder_decode (decoder, buffer);
        iamf_stream_scale_decoder_demix (decoder, buffer, pcm, ret);
    } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED)
        ret = iamf_stream_ambisonics_decoder_decode (decoder, pcm);
    return ret;
}

static int
iamf_stream_decoder_flush (IAMF_StreamDecoder *decoder)
{
    IAMF_Stream *stream = decoder->stream;
    for (int i=0; i<stream->nb_substreams; ++i) {
        if (decoder->packets[i]) {
            free(decoder->packets[i]);
        }
    }
    memset (decoder->packets, 0, sizeof (uint8_t *) * stream->nb_substreams);
    memset (decoder->sizes, 0, sizeof (uint32_t ) * stream->nb_substreams);
    decoder->count = 0;
    return 0;
}

static int
iamf_stream_scale_decoder_update_recon_gain (IAMF_StreamDecoder *decoder,
        ReconGainList *list)
{
    ReconGain      *src;
    IAMF_ReconGain *dst;
    int             ret = 0;
    int             ri = 0;
    IAMF_Stream            *stream = decoder->stream;
    ChannelLayerContext    *ctx = (ChannelLayerContext *)stream->priv;

    if (!list)
        return IAMF_ERR_BAD_ARG;

    ia_logt("recon gain info : list %p, count %d, recons %p", list, list->count,
            list->recon);
    for (int i=0; i<ctx->nb_layers; ++i) {
        src = &list->recon[ri];
        dst = ctx->conf_s[i].recon_gain;
        if (dst) {
            ++ri;
            if (i > ctx->nb_layers) {
                continue;
            }
            ia_logd("audio layer %d :", i);
            ia_logd("dst %p, src %p ", dst, src);
            if (dst->flags ^ src->flags) {
                dst->flags = src->flags;
                dst->nb_channels = src->channels;
                iamf_recon_channels_order_update (ctx->conf_s[i].layout, dst);
            }
            for (int c=0; c<dst->nb_channels; ++c) {
                dst->recon_gain[c] = qf_to_float(src->recon_gain[c], 8);
            }
            ia_logd(" > recon gain flags 0x%04x", dst->flags);
            ia_logd(" > channel count %d", dst->nb_channels);
            for (int c=0; c<dst->nb_channels; ++c)
                ia_logd(" > > channel %s(%d) : recon gain %f(0x%02x)",
                        ia_channel_name(dst->order[c]), dst->order[c],
                        dst->recon_gain[c], src->recon_gain[c]);
        }
    }
    ia_logt("recon gain info .");

    if (list->count != ri) {
        ret = IAMF_ERR_INTERNAL;
        ia_loge ("%s : the count (%d) of recon gain doesn't match with static meta (%d).",
                 ia_error_code_string(ret), list->count, ri);
    }

    return ret;
}

int iamf_stream_scale_decoder_update_parameters (IAMF_StreamDecoder *decoder,
        IAMF_DataBase *db, uint64_t ts)
{
    IAMF_Stream            *stream = decoder->stream;
    ChannelLayerContext    *ctx = (ChannelLayerContext *) stream->priv;
    uint64_t                eid = stream->element_id;
    ReconGainList          *recon = 0;

    ia_logt("Find element %lu", eid);

    ctx->dmx_mode = iamf_database_element_get_demix_mode(db, eid);
    ia_logt("update demix mode %u", ctx->dmx_mode);

    recon = iamf_database_element_get_recon_gain_list (db, eid);
    iamf_stream_scale_decoder_update_recon_gain (decoder, recon);
    ia_logt("update recon gain count %p", recon);

    return 0;
}

static int
iamf_stream_scale_decoder_decode (IAMF_StreamDecoder *decoder, float *pcm)
{
    IAMF_Stream     *stream = decoder->stream;
    ChannelLayerContext     *ctx = (ChannelLayerContext *)stream->priv;
    ScalableChannelDecoder *scale = decoder->scale;
    int ret = 0;
    float *out = pcm;
    IACoreDecoder *dec;

    ia_logt("decode sub-packets.");
    if (scale->nb_layers) {
        ia_logt("audio layer only mode.");
        uint32_t substream_offset = 0;

        for (int i=0; i<=ctx->layer; ++i) {
            ia_logt("audio layer %d.", i);
            dec = scale->sub_decoders[i];
            ia_logd("CG#%d: channels %d, streams %d, decoder %p, out %p, offset %lX, size %lu",
                    i, ctx->conf_s[i].nb_channels, ctx->conf_s[i].nb_substreams,
                    dec, out, (void *)out - (void *)pcm,
                    sizeof(float) * decoder->frame_size * ctx->conf_s[i].nb_channels);
            for (int k=0; k<ctx->conf_s[i].nb_substreams; ++k) {
                ia_logd(" > sub-packet %d (%p) size %d", k,
                        decoder->packets[substream_offset+k],
                        decoder->sizes[substream_offset+k]);
            }
            ret = ia_core_decoder_decode_list (dec,
                                               &decoder->packets[substream_offset],
                                               &decoder->sizes[substream_offset],
                                               ctx->conf_s[i].nb_substreams, out,
                                               decoder->frame_size);
            if(ret < 0) {
                ia_loge("sub packet %d decode fail.", i);
                break;
            } else if (ret != decoder->frame_size) {
                ia_loge("decoded frame size is not %d (%d).", decoder->frame_size, ret);
                break;
            }
            out += (ret * ctx->conf_s[i].nb_channels);
            substream_offset += ctx->conf_s[i].nb_substreams;
        }
    }

    return ret;
}

static int32_t
iamf_stream_scale_decoder_demix (IAMF_StreamDecoder *decoder, float *src,
                                 float *dst, uint32_t frame_size)
{
    IAMF_Stream            *stream = decoder->stream;
    ScalableChannelDecoder *scale = decoder->scale;
    ChannelLayerContext    *ctx = (ChannelLayerContext *)stream->priv;

    Demixer                *demixer = scale->demixer;
    IAMF_ReconGain         *re = ctx->conf_s[ctx->layer].recon_gain;

    ia_logt ("demixer info update :");
    if (re) {
        demixer_set_recon_gain (demixer, re->nb_channels,
                                re->order, re->recon_gain, re->flags);

        ia_logd ("channel flags 0x%04x", re->flags & U16_MASK);
        for (int c=0; c<re->nb_channels; ++c) {
            ia_logd ("channel %s(%d) recon gain %f",
                     ia_channel_name(re->order[c]), re->order[c], re->recon_gain[c]);
        }
    }
    demixer_set_demixing_mode (scale->demixer, ctx->dmx_mode);
    ia_logd ("demixing mode %d", ctx->dmx_mode);

    return demixer_demixing (scale->demixer, dst, src, frame_size);
}

int iamf_stream_scale_demixer_configure (IAMF_StreamDecoder *decoder)
{
    IAMF_Stream    *stream = decoder->stream;
    ScalableChannelDecoder     *scale = decoder->scale;
    Demixer        *demixer = scale->demixer;
    IAChannel       chs[IA_CH_LAYOUT_MAX_CHANNELS];
    float           gains[IA_CH_LAYOUT_MAX_CHANNELS];
    uint8_t         flags;
    uint32_t        count = 0;
    SubLayerConf               *layer_conf;
    ChannelLayerContext        *ctx = (ChannelLayerContext *)stream->priv;


    demixer_set_channel_layout (demixer, ctx->layout);
    demixer_set_channels_order (demixer, ctx->channels_order, ctx->channels);

    for (int l=0; l<=ctx->layer; ++l) {
        layer_conf = &ctx->conf_s[l];
        if (layer_conf->output_gain) {
            flags = layer_conf->output_gain->flags;
            for (int c=0; c<IA_CH_GAIN_COUNT; ++c) {
                if (flags & RSHIFT(c)) {
                    chs[count] = iamf_output_gain_channel_map (layer_conf->layout, c);
                    if (chs[count] != IA_CH_INVALID) {
                        gains[count++] = layer_conf->output_gain->gain;
                    }
                }
            }
        }
    }

    demixer_set_output_gain (demixer, chs, gains, count);

    ia_logi ("demixer info :");
    ia_logi ("layout %s(%d)", ia_channel_layout_name(ctx->layout),
             ctx->layout);
    ia_logi ("input channels order :");

    for (int c=0; c<ctx->channels; ++c) {
        ia_logi ("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
                 ctx->channels_order[c]);
    }

    ia_logi ("output gain info : ");
    for (int c=0; c<count; ++c) {
        ia_logi ("channel %s(%d) gain %f", ia_channel_name(chs[c]), chs[c], gains[c]);
    }

    return 0;
}

static int iamf_stream_ambisionisc_order (int channels)
{
    if (channels == 4)
        return IAMF_FOA;
    else if (channels == 9)
        return IAMF_SOA;
    else if (channels == 16)
        return IAMF_TOA;
    return 0;
}

int iamf_stream_ambisonics_decoder_decode (IAMF_StreamDecoder *decoder, float *pcm)
{
    IAMF_Stream        *stream = decoder->stream;
    AmbisonicsDecoder  *amb = decoder->ambisonics;
    int ret = 0;
    IACoreDecoder *dec;

    dec = amb->decoder;
    for (int k=0; k<stream->nb_substreams; ++k) {
        ia_logd(" > sub-packet %d (%p) size %d", k, decoder->packets[k], decoder->sizes[k]);
    }
    ret = ia_core_decoder_decode_list (dec, decoder->packets, decoder->sizes, stream->nb_substreams, pcm, decoder->frame_size);
    if(ret < 0) {
        ia_loge("ambisonics stream packet decode fail.");
    } else if (ret != decoder->frame_size) {
        ia_loge("decoded frame size is not %d (%d).", decoder->frame_size, ret);
    }

    return ret;
}

static int
iamf_stream_render(IAMF_Stream *stream, float *in, float *out, int frame_size)
{
    int ret = IAMF_OK;
    int inchs;
    int outchs = iamf_layout_channels_count(&stream->final_layout->layout);
    float **sout = IAMF_MALLOCZ(float *, outchs);
    float **sin = 0;
    lfe_filter_t *plfe = 0;

    ia_logd ("output channels %d", outchs);
    if (!sout) {
        ret = IAMF_ERR_ALLOC_FAIL;
        goto render_end;
    }

    for (int i=0; i<outchs; ++i) {
        sout[i] = &out[frame_size * i];
    }

    if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
        ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
        struct m2m_rdr_t m2m;
        IAMF_SP_LAYOUT lin;
        IAMF_PREDEFINED_SP_LAYOUT pin;

        inchs = ia_channel_layout_get_channels_count(ctx->layout);
        sin = IAMF_MALLOC(float *, inchs);

        for (int i=0; i<inchs; ++i) {
            sin[i] = &in[frame_size * i];
        }


        lin.sp_type = 0;
        lin.sp_layout.predefined_sp = &pin;
        pin.system = iamf_layer_layout_get_rendering_id(ctx->layout);
        pin.lfe1 = iamf_layer_layout_lfe1(ctx->layout);
        pin.lfe2 = 0;

        IAMF_element_renderer_get_M2M_matrix(&lin, &stream->final_layout->sp, &m2m);
        IAMF_element_renderer_render_M2M(&m2m, sin, sout, frame_size);
    } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
        struct h2m_rdr_t h2m;
        IAMF_HOA_LAYOUT hin;

        inchs = stream->nb_channels;
        sin = IAMF_MALLOCZ(float *, inchs);

        if (!sin) {
            ret = IAMF_ERR_ALLOC_FAIL;
            goto render_end;
        }
        for (int i=0; i<inchs; ++i) {
            sin[i] = &in[frame_size * i];
        }

        hin.order = iamf_stream_ambisionisc_order(inchs);
        ia_logd("ambisonics order is %d", hin.order);
        if (!hin.order) {
            ret = IAMF_ERR_INTERNAL;
            goto render_end;
        }

        hin.lfe_on = 1; // turn on LFE of HOA ##SR

        IAMF_element_renderer_get_H2M_matrix(&hin, stream->final_layout->sp.sp_layout.predefined_sp, &h2m);
        if (hin.lfe_on && iamf_layout_lfe_check(&stream->final_layout->layout)) {
            plfe = &stream->final_layout->sp.lfe_f;
        }
        IAMF_element_renderer_render_H2M(&h2m, sin, sout, frame_size, plfe);
    }

render_end:

    if (sin) {
        free(sin);
    }
    if (sout) {
        free(sout);
    }
    return ret;
}

void iamf_mixer_reset(IAMF_Mixer *m)
{
    if (m->element_ids)
        free (m->element_ids);

    if (m->frames)
        free (m->frames);

    memset (m, 0, sizeof (IAMF_Mixer));
}

static int iamf_mixer_default_output_sample (IACodecID cid)
{
    int ret = OPUS_FRAME_SIZE;
    if (cid == IA_CODEC_AAC) {
        ret = AAC_FRAME_SIZE;
    }
    return ret;
}

static int iamf_mixer_init (IAMF_DecoderHandle handle)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_Presentation      *pst = ctx->presentation;
    IAMF_Mixer             *mixer = &pst->mixer;
    int cnt = pst->nb_streams;

    if (!cnt) {
        return IAMF_ERR_INTERNAL;
    }

    memset (mixer, 0, sizeof (IAMF_Mixer));
    mixer->nb_elements = cnt;
    mixer->element_ids = IAMF_MALLOCZ (uint64_t, cnt);
    mixer->frames = IAMF_MALLOCZ (float *, cnt);
    if (!mixer->element_ids || !mixer->frames) {
        iamf_mixer_reset(mixer);
        return IAMF_ERR_ALLOC_FAIL;
    }
    for (int i=0; i<cnt; ++i) {
        mixer->element_ids[i] = pst->streams[i]->element_id;
    }
    mixer->count = 0;
    mixer->channels = iamf_layout_channels_count(&ctx->output_layout->layout);
    mixer->samples = ctx->output_samples;
    if (!mixer->samples) {
        mixer->samples = iamf_mixer_default_output_sample (pst->streams[0]->codec_id);
    }

    return 0;
}

static int
iamf_mixer_add_frame(IAMF_Mixer *mixer, uint64_t element_id, float *in,
                     int samples)
{
    if (samples != mixer->samples) {
        return IAMF_ERR_BAD_ARG;
    }

    ia_logd("element id %lu frame", element_id);
    for (int i=0; i<mixer->nb_elements; ++i) {
        if (mixer->element_ids[i] == element_id) {
            if (!mixer->frames[i]) {
                ++mixer->count;
            }
            mixer->frames[i] = in;
            break;
        }
    }
    ia_logd("frame count %d vs element count %d", mixer->count, mixer->nb_elements);

    if (mixer->count == mixer->nb_elements) {
        mixer->enable_mix = 1;
    }
    return 0;
}

static int
iamf_mixer_mix(IAMF_Mixer *mixer, float *out)
{
    uint32_t offset = 0;
    ia_logd("samples %d, channels %d", mixer->samples, mixer->channels);
    memset (out, 0, sizeof(float) * mixer->samples * mixer->channels);

    for (int c=0; c<mixer->channels; ++c) {
        offset = (c * mixer->samples);
        for (int i=0; i<mixer->samples; ++i) {
            for (int s=0; s<mixer->nb_elements; ++s) {
                out[offset + i] += mixer->frames[s][offset + i];
            }
        }
    }

    mixer->count = 0;
    memset(mixer->frames, 0, sizeof(uint8_t *) * mixer->nb_elements);
    mixer->enable_mix = 0;
    return mixer->samples;
}


/* >>>>>>>>>>>>>>>>>> STREAM DECODER MIXER >>>>>>>>>>>>>>>>>> */

static void iamf_extra_data_reset(IAMF_extradata *data);

static int32_t
iamf_decoder_internal_reset (IAMF_DecoderHandle handle)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;

    iamf_database_reset (&ctx->db);
    iamf_extra_data_reset(&ctx->metadata);
    if (ctx->presentation)
        iamf_presentation_free (ctx->presentation);
    if (ctx->mix_presentation_label)
        free(ctx->mix_presentation_label);
    if (ctx->output_layout)
        iamf_layout_info_free(ctx->output_layout);
    audio_effect_peak_limiter_uninit (&ctx->limiter);
    memset(handle, 0, sizeof(struct IAMF_Decoder));

    return 0;
}

static int32_t
iamf_decoder_internal_init (IAMF_DecoderHandle handle, const uint8_t *data,
                            uint32_t size, uint32_t *rsize)
{
    int32_t     ret = 0;
    uint32_t    pos = 0, consume = 0;
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_OBU    obj;

    if (~ctx->flags & IAMF_FLAGS_MAGIC_CODE) {
        // search magic code obu
        ia_logi("without magic code flag.");
        while (pos < size) {
            consume = IAMF_OBU_split(data, size, &obj);
            if (!consume || obj.type == IAMF_OBU_MAGIC_CODE) {
                if (!consume) {
                    ia_loge("consume size 0.");
                } else {
                    ia_logt("consume size %d, obu type (%d) %s", consume, obj.type,
                            IAMF_OBU_type_string(obj.type));
                    if (obj.type == IAMF_OBU_MAGIC_CODE) {
                        ia_logi("type is magic code.");
                    }
                }
                break;
            }
            pos += consume;
            consume = 0;
        }
    }

    if (consume || ctx->flags & IAMF_FLAGS_MAGIC_CODE) {
        pos += iamf_decoder_internal_read_descriptors_OBUs (handle, data + pos,
                size - pos);
    }

    if (~ctx->flags & IAMF_FLAGS_CONFIG) {
        ret = IAMF_ERR_NEED_MORE_DATA;
    }

    *rsize = pos;
    ia_logt("read size %d pos", pos);
    return ret;
}

uint32_t
iamf_decoder_internal_read_descriptors_OBUs (IAMF_DecoderHandle handle,
        const uint8_t *data, uint32_t size)
{
    IAMF_OBU    obu;
    uint32_t    pos = 0, ret = 0, rsize = 0;

    ia_logt("handle %p, data %p, size %d", handle, data, size);
    while (pos < size) {
        ret = IAMF_OBU_split(data + pos, size - pos, &obu);
        if (!ret) {
            ia_logw("consume size is 0.");
            break;
        }
        rsize = ret;
        ia_logt("consume size %d, obu type (%d) %s", ret, obu.type,
                IAMF_OBU_type_string(obu.type));
        if (IAMF_OBU_is_descrptor_OBU (&obu)) {
            ret = iamf_decoder_internal_add_descrptor_OBU (handle, &obu);
            if (ret == IAMF_OK && obu.type == IAMF_OBU_MAGIC_CODE) {
                handle->ctx.flags |= IAMF_FLAGS_MAGIC_CODE;
            }
        } else {
            handle->ctx.flags |= IAMF_FLAGS_CONFIG;
            break;
        }
        pos += rsize;
    }
    return pos;
}

uint32_t
iamf_decoder_internal_parse_OBUs (IAMF_DecoderHandle handle,
                                  const uint8_t *data, uint32_t size)
{
    IAMF_OBU    obu;
    uint32_t    pos = 0, ret = 0;

    ia_logd("handle %p, data %p, size %d", handle, data, size);
    while (pos < size) {
        ret = IAMF_OBU_split(data + pos, size - pos, &obu);
        if (!ret) {
            ia_logt("need more data.");
            break;
        }

        if (obu.type == IAMF_OBU_PARAMETER_BLOCK) {
            uint64_t    pid = IAMF_OBU_get_object_id (&obu);
            ia_logd("get parameter with id %lu", pid);
            if (pid != INVALID_ID) {
                IAMF_Element    *e = IAMF_ELEMENT(iamf_database_get_element_by_parameterID(&handle->ctx.db, pid));
                if (e) {
                    IAMF_ParameterParam ext;
                    IAMF_ObjectParameter *param = IAMF_OBJECT_PARAM(&ext);
                    IAMF_Object *obj;

                    ia_logd("the element id for parameter %lu", e->element_id);
                    if (e->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED && e->channels_conf) {
                        memset(&ext, 0, sizeof(IAMF_ObjectParameter));
                        ext.base.type = IAMF_OBU_PARAMETER_BLOCK;
                        ext.parameter_type = iamf_database_parameter_viewer_get_type (&handle->ctx.db, pid);
                        ext.nb_layers = e->channels_conf->nb_layers;
                        ext.recon_gain_flags = iamf_database_element_get_recon_gain_flags(&handle->ctx.db, e->element_id);
                    }
                    obj = IAMF_object_new (&obu, param);
                    iamf_database_add_object (&handle->ctx.db, obj);
                }
            }
        } else if (obu.type >= IAMF_OBU_AUDIO_FRAME && obu.type < IAMF_OBU_MAGIC_CODE) {
            IAMF_Object *obj = IAMF_object_new (&obu, 0);
            IAMF_Frame *o = (IAMF_Frame *)obj;
            iamf_decoder_internal_deliver(handle, o);
            IAMF_object_free (obj);
        } else if (obu.type == IAMF_OBU_MAGIC_CODE) {
            ia_logi("*********** FOUND NEW MAGIC CODE **********");
            handle->ctx.flags = IAMF_FLAGS_RECONFIG;
            break;
        } else if (obu.type == IAMF_OBU_SYNC) {
            ia_logi("*********** FOUND SYNC OBU **********");
            IAMF_Object            *obj = IAMF_object_new (&obu, 0);
            IAMF_Stream            *s;
            IAMF_DecoderContext    *ctx = &handle->ctx;
            IAMF_Presentation      *pst = ctx->presentation;

            iamf_database_add_object (&handle->ctx.db, obj);

            // update timestamps of all streams
            for (int i=0; i<pst->nb_streams; ++i) {
                s = pst->streams[i];
                s->timestamp = iamf_database_element_get_timestamp(&ctx->db, s->element_id);
            }
        }
        pos += ret;

        if (handle->ctx.presentation->prepared_decoder) {
            break;
        }
    }
    return pos;
}


int32_t
iamf_decoder_internal_add_descrptor_OBU (IAMF_DecoderHandle handle,
        IAMF_OBU *obu)
{
    IAMF_DataBase  *db;
    IAMF_Object     *obj;

    db = &handle->ctx.db;
    if (obu->type == IAMF_OBU_MIX_PRESENTATION) {
        IAMF_MixPresentationParam   mpp;
        IAMF_ObjectParameter    *param = 0;
        mpp.base.type = IAMF_OBU_MIX_PRESENTATION;
        mpp.obj = db;
        mpp.e_query = iamf_database_element_info_query;
        param = IAMF_OBJECT_PARAM(&mpp);
        obj = IAMF_object_new (obu, param);
    } else
        obj = IAMF_object_new (obu, 0);
    if (!obj) {
        ia_loge("fail to new object for %s(%d)",
                IAMF_OBU_type_string(obu->type), obu->type);
        return IAMF_ERR_ALLOC_FAIL;
    }

    return iamf_database_add_object (db, obj);
}

int iamf_decoder_internal_deliver (IAMF_DecoderHandle handle,
                                   IAMF_Frame *obj)
{
    IAMF_DataBase          *db = &handle->ctx.db;
    IAMF_Presentation      *pst = handle->ctx.presentation;
    int                     idx = -1, i;
    IAMF_Stream            *stream;
    IAMF_StreamDecoder     *decoder;

    for (i=0; i<pst->nb_streams; ++i) {
        idx = iamf_database_element_get_substream_index(db, pst->streams[i]->element_id,
                obj->id);
        if (idx > -1) {
            break;
        }
    }
    ia_logd("frame id %lu and its stream id %lu, and index %d", obj->id, pst->streams[i]->element_id, idx);

    if (idx > -1) {
        stream = pst->streams[i];
        decoder = pst->decoders[i];

        iamf_stream_decoder_receive_packet (decoder, idx, obj);

        if (decoder->count == stream->nb_substreams) {
            pst->prepared_decoder = decoder;
        }
    }

    return 0;
}


static int
iamf_decoder_mix_presentation_matching_calculation(IAMF_MixPresentation *obj, LayoutInfo *layout)
{
    int                 score = 0;
    SoundSystemLayout  *ss;
    SubMixPresentation *sub;

    if (obj->num_sub_mixes) {
        for (int n=0; n<obj->num_sub_mixes; ++n) {
            sub = &obj->sub_mixes[n];

            if (sub->num_layouts) {
                int s;
                for (int i=0; i<sub->num_layouts; ++i) {
                    s = 0;
                    if (sub->layouts[i]->type == TARGET_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
                        ss = SOUND_SYSTEM_LAYOUT(sub);
                        if (ss->sound_system == layout->layout.sound_system.sound_system) {
                            score = 100;
                            break;
                        } else {
                            int chs = IAMF_layout_sound_system_channels_count(ss->sound_system);
                            s = 50;
                            if (layout->channels < chs) {
                                s += (chs - layout->channels);
                            } else {
                                s -= (layout->channels - chs);
                            }
                        }
                    }
                    if (s > score)
                        score = s;
                }
            }
        }
    }

    return score;
}

static IAMF_MixPresentation *
iamf_decoder_get_best_mix_presentation (IAMF_DecoderHandle handle)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_DataBase          *db = &ctx->db;
    IAMF_MixPresentation   *mp = 0, *obj;

    if (db->mixPresentation->count > 0) {
        if (db->mixPresentation->count == 1) {
            mp = IAMF_MIX_PRESENTATION (db->mixPresentation->items[0]);
        } else if (ctx->mix_presentation_label) {
            int idx = iamf_database_mix_presentation_get_label_index(db, ctx->mix_presentation_label);
            if (idx == INAVLID_MIX_PRESENTATION_INDEX)
                idx = 0;
            mp = IAMF_MIX_PRESENTATION (db->mixPresentation->items[idx]);
        } else {
            int max_percentage = 0, sub_percentage;

            for (int i=0; i<db->mixPresentation->count; ++i) {
                obj = IAMF_MIX_PRESENTATION (db->mixPresentation->items[i]);
                sub_percentage = iamf_decoder_mix_presentation_matching_calculation(obj, ctx->output_layout);
                if (max_percentage < sub_percentage)
                    mp = obj;
            }
        }
    }
    return mp;
}

static IAMF_Element *
iamf_decoder_get_best_element (IAMF_DecoderHandle handle)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_DataBase          *db = &ctx->db;

    if (db->eViewer.count > 0) {
        return (IAMF_Element *)db->eViewer.items[0].element;
    }
    return 0;
}

static int
iamf_decoder_enable_mix_presentation_by_element (IAMF_DecoderHandle handle, IAMF_Element *element)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_Presentation      *pst;

    if (!element)
        return IAMF_ERR_INTERNAL;

    pst = IAMF_MALLOCZ(IAMF_Presentation, 1);
    if (!pst)
        return IAMF_ERR_ALLOC_FAIL;

    iamf_presentation_free (ctx->presentation);
    ctx->presentation = pst;

    ia_logd("enable mix presentation with element id %lu", element->element_id);
    iamf_stream_enable (handle, element);

    return IAMF_OK;
}

static int
iamf_decoder_enable_mix_presentation (IAMF_DecoderHandle handle,
                                      IAMF_MixPresentation *mixp)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_DataBase          *db  = &ctx->db;
    IAMF_Element           *elem;
    IAMF_CodecConf         *cc;
    IAMF_Presentation      *old = ctx->presentation;
    IAMF_Presentation      *pst;
    SubMixPresentation     *sub;
    uint64_t                pid;
    ParameterItem          *pi = 0;

    pst = IAMF_MALLOCZ(IAMF_Presentation, 1);
    if (!pst)
        return IAMF_ERR_ALLOC_FAIL;

    pst->obj = mixp;
    ctx->presentation = pst;

    ia_logd("enable mix presentation id %lu, %p", mixp->mix_presentation_id, mixp);

    // There is only one sub mix in the mix presentation for simple and base
    // profiles. so the sub mix is selected the first.
    sub = mixp->sub_mixes;
    for (uint32_t i=0; i<sub->nb_elements; ++i) {
        elem = iamf_database_get_element (db, sub->conf_s[i].element_id);
        cc = iamf_database_element_get_codec_conf(db, elem->element_id);
        pid = sub->conf_s[i].conf_m.gain.base.id;
        pi = iamf_database_parameter_viewer_get_item (&db->pViewer, pid);
        if (!pi && iamf_database_parameter_viewer_add_item(db, pid, PARAMETER_TYPE_MIX_GAIN, 0) == IAMF_OK) {
            float   gain_db;
            pi = iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
            gain_db = q_to_float(sub->conf_s[i].conf_m.gain.mix_gain, 8);
            pi->defaultValue.mixGain = db2lin(gain_db);
            ia_logi("element %lu : mix gain %f (%f db) <- 0x%x", sub->conf_s[i].element_id, pi->defaultValue.mixGain, gain_db, sub->conf_s[i].conf_m.gain.mix_gain & U16_MASK);
        }

        iamf_database_element_set_mix_gain_parameter (db, elem->element_id, sub->conf_s[i].conf_m.gain.base.id);
        if (!elem || !cc)
            continue;
        if (elem->obj.flags & cc->obj.flags & IAMF_OBU_FLAG_REDUNDANT) {
            if(iamf_presentation_reuse_stream (pst, old, elem->element_id) != IAMF_OK) {
                iamf_stream_enable (handle, elem);
            }
        } else {
            iamf_stream_enable (handle, elem);
        }
    }

    if (old)
        iamf_presentation_free (old);

    return IAMF_OK;
}

static int iamf_decoder_frame_gain (void *in, void *out, int channels, int frame_size, float gain)
{
    int count = frame_size * channels;
    float  *fin = (float *)in;
    float  *fout = (float *)out;
    ia_logt("gain %f, channels %d, samples of frame %d, all samples %d", gain, channels, frame_size, count);
    for (int i=0; i<count; ++i)
        fout[i] = fin[i] * gain;

    return IAMF_OK;
}


static int
iamf_decoder_internal_decode (IAMF_DecoderHandle handle, const uint8_t *data,
                              int32_t size, uint32_t *rsize, void *pcm)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_DataBase      *db = &ctx->db;
    IAMF_Presentation  *pst = ctx->presentation;
    IAMF_StreamDecoder *decoder;
    IAMF_Stream        *stream;
    IAMF_Mixer         *mixer = &pst->mixer;
    int         ret = 0;
    uint32_t    r = 0;
    int real_frame_size = 0;
    float      *in, *out, gain;


    ia_logd("handle %p, data %p, size %d", handle, data, size);
    r = iamf_decoder_internal_parse_OBUs (handle, data, size);

    *rsize = r;

    if (~handle->ctx.flags & IAMF_FLAGS_MAGIC_CODE) {
        return IAMF_ERR_INVALID_STATE;
    }

    if (!pst->prepared_decoder) {
        return 0;
    }

    decoder = pst->prepared_decoder;
    stream = decoder->stream;
    in = decoder->buffers[0];
    out = decoder->buffers[1];

    iamf_stream_decoder_update_parameters (decoder, db, stream->timestamp);
    ret = iamf_stream_decoder_decode (decoder, in);
    if (ret > 0) {
        stream->timestamp += ret;
        iamf_database_element_time_elapse(db, stream->element_id, ret);
        if (decoder->stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
                ctx->metadata.param) {
            IAMF_Stream            *stream = decoder->stream;
            ChannelLayerContext    *cctx = (ChannelLayerContext *)stream->priv;
            if (cctx->dmx_mode >= 0)
                ctx->metadata.param->dmixp_mode = cctx->dmx_mode;
        }
    }
    iamf_stream_decoder_flush (decoder);
    pst->prepared_decoder = 0;

    ia_logt("decoder ret %d", ret);
    real_frame_size = ret;

#if SR
    if (real_frame_size > 0)
    { //////// SR decoding
        int i;
        void *wf = NULL;
        void *pcm_b;
        int nch = 0;

        for (i = 0; i < N_AUDIO_STREAM; i++)
        {
            if (_rec_stream_log[i].element_id == stream->element_id)
            { // _dec_stream_log[]俊绰 泅犁狼 audio element啊 乐促.
                nch = _rec_stream_log[i].nchannels;
                wf = _rec_stream_log[i].wav;
                break;
            }
        }
        if (i == N_AUDIO_STREAM)
        { // _dec_stream_log[]俊绰 泅犁狼 audio element啊 绝促.
            char ae_fname[256];
            if (_rec_stream_count < N_AUDIO_STREAM)
            {
                sprintf(ae_fname, "rec_%ld.wav", stream->element_id);
                nch = _rec_stream_log[_rec_stream_count].nchannels = stream->nb_channels;
                wf = _rec_stream_log[_rec_stream_count].wav =
                  wav_write_open3(ae_fname, WAVE_FORMAT_FLOAT2, 48000, 32, nch);
                _rec_stream_log[_rec_stream_count].element_id = stream->element_id;
                _rec_stream_count++;
            }
        }

        pcm_b = IAMF_MALLOC(float, real_frame_size * nch);
        ia_decoder_plane2stride_out_float(pcm_b, in, real_frame_size, nch);
        wav_write_data2(wf, pcm_b, real_frame_size * nch * sizeof(float));
        free(pcm_b);
    } ////// SR
#endif

    iamf_stream_render(stream, in, out, real_frame_size);

#if SR
    if (real_frame_size > 0)
    { //////// SR rendering
        int i;
        void *wf = NULL;
        void *pcm_b;
        int nch = 0;

        for (i = 0; i < N_AUDIO_STREAM; i++)
        {
            if (_ren_stream_log[i].element_id == stream->element_id)
            { // _ren_stream_log[]俊绰 泅犁狼 audio element啊 乐促.
                nch = _ren_stream_log[i].nchannels;
                wf = _ren_stream_log[i].wav;
                break;
            }
        }
        if (i == N_AUDIO_STREAM)
        { // _ren_stream_log[]俊绰 泅犁狼 audio element啊 绝促.
            char ae_fname[256];
            if (_ren_stream_count < N_AUDIO_STREAM)
            {
                sprintf(ae_fname, "ren_%ld.wav", stream->element_id);
                nch = _ren_stream_log[_ren_stream_count].nchannels = iamf_layout_channels_count(&stream->final_layout->layout);
                wf = _ren_stream_log[_ren_stream_count].wav =
                    wav_write_open3(ae_fname, WAVE_FORMAT_FLOAT2, 48000, 32, nch);
                _ren_stream_log[_ren_stream_count].element_id = stream->element_id;
                _ren_stream_count++;
            }
        }

        pcm_b = IAMF_MALLOC(float, real_frame_size * nch);
        ia_decoder_plane2stride_out_float(pcm_b, out, real_frame_size, nch);
        wav_write_data2(wf, pcm_b, real_frame_size * nch * sizeof(float));
        free(pcm_b);
    } ////// SR
#endif

    swap((void **)&in, (void **)&out);

    gain = iamf_database_element_get_mix_gain(db, stream->element_id);
    iamf_decoder_frame_gain (in, out,
            iamf_layout_channels_count(&stream->final_layout->layout), real_frame_size, gain);
    swap((void **)&in, (void **)&out);

    iamf_mixer_add_frame(mixer, stream->element_id, in, real_frame_size);
    if (!mixer->enable_mix) {
        return IAMF_OK;
    }
    iamf_mixer_mix(mixer, out);
    swap((void **)&in, (void **)&out);

    audio_effect_peak_limiter_process_block(&ctx->limiter, in, out, real_frame_size);

    iamf_decoder_plane2stride_out_short(pcm, out, real_frame_size,
                                      ctx->output_layout->channels);
#if SR
    if (real_frame_size > 0)
    { //////// SR mixing

        void *wf = NULL;
        void *pcm_b;
        int nch = 0;

        if (_mix_stream_count > 0)
        {
            nch = _mix_stream_log.nchannels;
            wf = _mix_stream_log.wav;
        }
        else
        {
            char ae_fname[256];
            sprintf(ae_fname, "mix.wav");
            nch = _mix_stream_log.nchannels = iamf_layout_channels_count(&stream->final_layout->layout);
            wf = _mix_stream_log.wav =
                wav_write_open3(ae_fname, WAVE_FORMAT_FLOAT2, 48000, 32, nch);
            _mix_stream_count++;
        }

        pcm_b = IAMF_MALLOC(float, real_frame_size * nch);
        ia_decoder_plane2stride_out_float(pcm_b, out, real_frame_size, nch);
        wav_write_data2(wf, pcm_b, real_frame_size * nch * sizeof(float));
        free(pcm_b);
    } ////// SR
#endif

    ctx->duration += real_frame_size;
    ctx->last_frame_size = real_frame_size;

    return real_frame_size;
}

static LayoutInfo* iamf_layout_info_new_sound_system (IAMF_SoundSystem ss)
{
    IAMF_PREDEFINED_SP_LAYOUT *l;
    LayoutInfo *t = 0;

    t = IAMF_MALLOCZ(LayoutInfo, 1);
    if (!t) {
        ia_loge("fail to allocate memory to Layout.");
        return t;
    }

    t->layout.sound_system.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
    t->layout.sound_system.sound_system = ss;
    t->channels = IAMF_layout_sound_system_channels_count(ss);
    t->sp.sp_type = 0;
    l = IAMF_MALLOCZ(IAMF_PREDEFINED_SP_LAYOUT, 1);
    if (!l) {
        ia_loge("fail to allocate memory to Predefined SP Layout.");
        if (t)
            free(t);
        return 0;
    }
    l->system = iamf_sound_system_get_rendering_id(ss);
    l->lfe1 = iamf_sound_system_lfe1(ss);
    l->lfe2 = iamf_sound_system_lfe2(ss);

    t->sp.sp_layout.predefined_sp = l;

    return t;
}

static LayoutInfo* iamf_layout_info_new_binaural ()
{
    IAMF_PREDEFINED_SP_LAYOUT *l;
    LayoutInfo *t = 0;

    t = IAMF_MALLOCZ(LayoutInfo, 1);
    if (!t) {
        ia_loge("fail to allocate memory to Layout.");
        return t;
    }

    t->layout.binaural.type = IAMF_LAYOUT_TYPE_BINAURAL;
    t->channels = IAMF_layout_binaural_channels_count();
    t->sp.sp_type = 0;
    l = IAMF_MALLOCZ(IAMF_PREDEFINED_SP_LAYOUT, 1);
    if (!l) {
        ia_loge("fail to allocate memory to Predefined SP Layout.");
        if (t)
            free(t);
        return 0;
    }
    l->system = IAMF_BINAURAL;
    l->lfe1 = 0;
    l->lfe2 = 0;

    t->sp.sp_layout.predefined_sp = l;

    return t;
}


static void iamf_extra_data_dump(IAMF_extradata *metadata)
{
    ia_logt("metadata: target layout >");
    iamf_layout_dump(&metadata->target_layout);

    ia_logt("metadata: number of samples %u", metadata->number_of_samples);
    ia_logt("metadata: bitdepth %u", metadata->bitdepth);
    ia_logt("metadata: sampling rate %u", metadata->sampling_rate);
    ia_logt("metadata: number loudness layout %d ", metadata->num_loudness_layouts);

    for (int i=0; i<metadata->num_loudness_layouts; ++i) {
        ia_logt("metadata: loudness layout %d >", i);
        iamf_layout_dump(&metadata->loudness_layout[i]);

        ia_logt("metadata: loudness info %d >", i);
        ia_logt("\tinfo type %u", metadata->loudness[i].info_type & U8_MASK);
        ia_logt("\tintegrated loudness 0x%x", metadata->loudness[i].integrated_loudness & U16_MASK);
        ia_logt("\tdigital peak 0x%d", metadata->loudness[i].digital_peak & U16_MASK);
        if (metadata->loudness[i].info_type & 1)
            ia_logt("\ttrue peak %d", metadata->loudness[i].true_peak);
    }
    ia_logt("metadata: number parameters %d ", metadata->num_parameters);

    for (int i=0; i<metadata->num_parameters; ++i) {
        ia_logt("parameter size %d", metadata->param[i].parameter_length);
        ia_logt("parameter type %d", metadata->param[i].parameter_definition_type);
        if (metadata->param[i].parameter_definition_type == IAMF_PARAMETER_TYPE_DEMIXING)
            ia_logt("demix mode %d", metadata->param[i].dmixp_mode);
    }

}

static int iamf_extra_data_init (IAMF_DecoderHandle handle)
{
    IAMF_DecoderContext    *ctx = &handle->ctx;
    IAMF_Presentation      *pst = ctx->presentation;
    IAMF_extradata         *metadata = &ctx->metadata;

    ia_logd ("initialize iamf extra data.");
    iamf_layout_copy(&metadata->target_layout, &ctx->output_layout->layout);
    metadata->bitdepth = 16;
    metadata->sampling_rate = 48000;

    ia_logd("mix presetation %p", ctx->presentation->obj);
    metadata->num_loudness_layouts = ctx->presentation->obj->sub_mixes->num_layouts;
    metadata->loudness_layout = IAMF_MALLOCZ(IAMF_Layout, metadata->num_loudness_layouts);
    metadata->loudness = IAMF_MALLOCZ(IAMF_LoudnessInfo, metadata->num_loudness_layouts);
    if (!metadata->loudness_layout || !metadata->loudness)
        return IAMF_ERR_ALLOC_FAIL;
    for (int i=0; i<metadata->num_loudness_layouts; ++i) {
        iamf_layout_copy2(&metadata->loudness_layout[i],  ctx->presentation->obj->sub_mixes->layouts[i]);
    }
    memcpy(metadata->loudness, pst->obj->sub_mixes->loudness, sizeof(IAMF_LoudnessInfo) * metadata->num_loudness_layouts);

    if (pst) {
        ElementItem *ei;
        for (int i=0; i<pst->obj->sub_mixes->nb_elements; ++i) {
            ei = iamf_database_element_get_item(&ctx->db, pst->obj->sub_mixes->conf_s[i].element_id);
            if (ei && ei->demixing) {
                metadata->num_parameters = 1;
                metadata->param = IAMF_MALLOCZ(IAMF_Param, 1);
                if (!metadata->param)
                    return IAMF_ERR_ALLOC_FAIL;
                metadata->param->parameter_length = 8;
                metadata->param->parameter_definition_type = IAMF_PARAMETER_TYPE_DEMIXING;
                break;
            }
        }
    }

    iamf_extra_data_dump(metadata);
    return IAMF_OK;
}

static int iamf_extra_data_copy (IAMF_extradata *dst, IAMF_extradata *src)
{
    if (!src)
        return IAMF_ERR_BAD_ARG;

    if (!dst)
        return IAMF_ERR_INTERNAL;

    iamf_layout_copy(&dst->target_layout, &src->target_layout);

    dst->number_of_samples = src->number_of_samples;
    dst->bitdepth = src->bitdepth;
    dst->sampling_rate = src->sampling_rate;
    dst->num_loudness_layouts = src->num_loudness_layouts;

    if (dst->num_loudness_layouts) {
        dst->loudness_layout = IAMF_MALLOCZ(IAMF_Layout, dst->num_loudness_layouts);
        dst->loudness = IAMF_MALLOCZ(IAMF_LoudnessInfo, dst->num_loudness_layouts);

        if (!dst->loudness_layout || !dst->loudness)
            return IAMF_ERR_ALLOC_FAIL;
        for (int i=0; i<dst->num_loudness_layouts; ++i) {
            iamf_layout_copy(&dst->loudness_layout[i], &src->loudness_layout[i]);
            memcpy(&dst->loudness[i], &src->loudness[i], sizeof(IAMF_LoudnessInfo));
        }
    } else {
        dst->loudness_layout = 0;
        dst->loudness = 0;
    }

    dst->num_parameters = src->num_parameters;

    if (dst->num_parameters) {
        dst->param = IAMF_MALLOCZ(IAMF_Param, dst->num_parameters);
        if (!dst->param)
            return IAMF_ERR_ALLOC_FAIL;
        for (int i=0; i<src->num_parameters; ++i)
            memcpy(&dst->param[i], &src->param[i], sizeof(IAMF_Param));
    } else {
        dst->param = 0;
    }

    return IAMF_OK;
}

void iamf_extra_data_reset(IAMF_extradata *data)
{
    if (data) {
        iamf_layout_reset(&data->target_layout);

        if (data->loudness_layout) {
            for (int i=0; i<data->num_loudness_layouts; ++i)
                iamf_layout_reset(&data->loudness_layout[i]);

            free (data->loudness_layout);
        }

        if (data->loudness)
            free (data->loudness);
        if (data->param)
            free (data->param);

        memset(data, 0, sizeof(IAMF_extradata));
    }
}
/* ----------------------------- APIs ----------------------------- */

IAMF_DecoderHandle
IAMF_decoder_open (void)
{
    IAMF_DecoderHandle      handle = 0;
    handle = IAMF_MALLOCZ(struct IAMF_Decoder, 1);
    if (handle) {
        IAMF_DataBase          *db = &handle->ctx.db;

        handle->ctx.duration_time_base = 48000;
        if (iamf_database_init (db) != IAMF_OK) {
            IAMF_decoder_close (handle);
            handle = 0;
        }
    }
    return handle;
}

int32_t IAMF_decoder_close (IAMF_DecoderHandle handle)
{
    if (handle) {
        iamf_decoder_internal_reset (handle);
        free (handle);
    }
#if SR
    for (int i = 0; i < _dec_stream_count; i++)
    {
        wav_write_close2(_dec_stream_log[i].wav);
    }
    for (int i = 0; i < _rec_stream_count; i++)
    {
        wav_write_close2(_rec_stream_log[i].wav);
    }
    for (int i = 0; i < _ren_stream_count; i++)
    {
        wav_write_close2(_ren_stream_log[i].wav);
    }
    if (_mix_stream_log.wav)
    {
        wav_write_close2(_mix_stream_log.wav);
    }
#endif

    return 0;
}

int32_t
IAMF_decoder_configure (IAMF_DecoderHandle handle, const uint8_t *data,
                        uint32_t size, uint32_t *rsize)
{
    int32_t     ret = 0;

    ia_logt("handle %p, data %p, size %d", handle, data, size);

    if (!handle || !data || !size) {
        return IAMF_ERR_BAD_ARG;
    }

    if (handle->ctx.flags & IAMF_FLAGS_RECONFIG) {
        IAMF_DecoderContext    *ctx = &handle->ctx;

        ia_logi("reconfigure decoder.");
        iamf_database_reset(&ctx->db);
        iamf_database_init (&ctx->db);
        iamf_extra_data_reset(&ctx->metadata);
        handle->ctx.flags &= ~IAMF_FLAGS_RECONFIG;
    } else {
        ia_logd("initialize limiter.");
        audio_effect_peak_limiter_init(&handle->ctx.limiter, LIMITER_MaximumTruePeak,
                48000, iamf_layout_channels_count(&handle->ctx.output_layout->layout),
                LIMITER_AttackSec, LIMITER_ReleaseSec, LIMITER_LookAhead);
    }
    ret = iamf_decoder_internal_init (handle, data, size, rsize);

    if (ret == IAMF_OK) {
        IAMF_MixPresentation *mixp = iamf_decoder_get_best_mix_presentation (
                handle);
        ia_logi("valid mix presentation %ld", mixp?mixp->mix_presentation_id:-1);
        if (!mixp) {
            ret = iamf_decoder_enable_mix_presentation_by_element (handle, iamf_decoder_get_best_element (handle));
        } else {
            ret = iamf_decoder_enable_mix_presentation (handle, mixp);
        }
        if (ret == IAMF_OK) {
            iamf_mixer_init (handle);
            iamf_extra_data_init (handle);
        }
    }
    return ret;
}


int32_t
IAMF_decoder_get_configuration (IAMF_DecoderHandle handle, IAMF_ConfInfo *info)
{
    IAMF_DecoderContext *ctx = &handle->ctx;

    if (!(ctx->flags & IAMF_FLAGS_CONFIG)) {
        return IAMF_ERR_INVALID_STATE;
    }

    // TODO copy configure information to IADecoderConfInfo.
    return IAMF_OK;
}

int IAMF_decoder_decode (IAMF_DecoderHandle handle, const uint8_t *data, int32_t size, uint32_t *rsize, void *pcm)
{
    IAMF_DecoderContext *ctx = &handle->ctx;
    ia_logd("handle %p, data %p, size %d", handle, data, size);
    if (!(ctx->flags & IAMF_FLAGS_CONFIG)) {
        return IAMF_ERR_INVALID_STATE;
    }
    return iamf_decoder_internal_decode(handle, data, size, rsize, pcm);
}

int IAMF_decoder_read_header (IAMF_DecoderHandle handle, IAMF_IPacket *packet)
{
    IAMF_DecoderContext    *ctx;
    IAMF_DataBase          *db;
    IAMF_MixPresentation   *mixp;

    if (!handle || !packet || !packet->data || !packet->size)
        return IAMF_ERR_BAD_ARG;

    ctx = &handle->ctx;
    db = &ctx->db;
    iamf_decoder_internal_read_descriptors_OBUs(handle, packet->data, packet->size);

    if (!db->codecConf->count || !db->element->count) {
        handle->ctx.flags &= ~IAMF_FLAGS_CONFIG;
        return IAMF_ERR_INVALID_PACKET;
    }

    mixp = iamf_decoder_get_best_mix_presentation ( handle);
    ia_logd("valid mix presentation %ld", mixp ? mixp->mix_presentation_id : -1);
    if (!mixp) {
        IAMF_Element *elem = iamf_decoder_get_best_element (handle);
        iamf_stream_enable (handle, elem);
    } else {
        iamf_decoder_enable_mix_presentation (handle, mixp);
    }
    iamf_mixer_init (handle);

    return IAMF_OK;
}

int IAMF_decoder_decode_packet (IAMF_DecoderHandle handle, IAMF_IPacket *packet)
{
    IAMF_DecoderContext *ctx;

    if (!handle || !packet || !packet->data || !packet->size)
        return IAMF_ERR_BAD_ARG;

    ctx = &handle->ctx;

    if (!(ctx->flags & IAMF_FLAGS_CONFIG)) {
        return IAMF_ERR_INVALID_STATE;
    }

    // TODO
    return IAMF_OK;
}

int  IAMF_decoder_receive_frame (IAMF_DecoderHandle handle, IAMF_OFrame *frame)
{
    // TODO
    return IAMF_OK;
}

IAMF_Labels* IAMF_decoder_get_mix_presentation_labels (IAMF_DecoderHandle handle)
{
    IAMF_DataBase          *db;
    IAMF_MixPresentation   *mp;
    IAMF_Labels            *labels = 0;

    if (!handle) {
        ia_loge ("Invalid input argments.");
        return 0;
    }

    db = &handle->ctx.db;
    labels = IAMF_MALLOCZ (IAMF_Labels, 1);
    if (!labels)
        goto label_fail;

    labels->count = db->mixPresentation->count;
    labels->labels = IAMF_MALLOCZ(char *, labels->count);
    if (!labels->labels)
        goto label_fail;

    for (int i=0; i<labels->count; ++i) {
        mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
        labels->labels[i] = IAMF_MALLOC(char, mp->label_size);
        if (!labels->labels[i])
            goto label_fail;
        memcpy(labels->labels[i], mp->mix_presentation_friendly_label, mp->label_size);
    }

    return labels;

label_fail:
    if (labels) {
        if (labels->labels) {
            for (int i=0; i<labels->count; ++i)
                if (labels->labels[i])
                    free (labels->labels[i]);
            free (labels->labels);
        }
        free (labels);
    }

    return 0;
}

int
IAMF_decoder_output_layout_set_sound_system (IAMF_DecoderHandle handle,
        IAMF_SoundSystem ss)
{
    IAMF_DecoderContext *ctx = &handle->ctx;
    if (!iamf_sound_system_valid(ss)) {
        return IAMF_ERR_BAD_ARG;
    }

    ia_logd("sound system %d, channels %d", ss, IAMF_layout_sound_system_channels_count(ss));

    if (ctx->output_layout)
        iamf_layout_info_free (ctx->output_layout);
    ctx->output_layout = iamf_layout_info_new_sound_system(ss);

    return 0;
}

int IAMF_decoder_output_layout_set_binaural (IAMF_DecoderHandle handle)
{
    IAMF_DecoderContext *ctx = &handle->ctx;

    ia_logd("binaural channels %d", IAMF_layout_binaural_channels_count());

    if (ctx->output_layout)
        iamf_layout_info_free (ctx->output_layout);
    ctx->output_layout = iamf_layout_info_new_binaural();

    return 0;

}

int IAMF_decoder_output_layout_set_mix_presentation_label (IAMF_DecoderHandle handle, const char *label)
{
    IAMF_DecoderContext    *ctx;
    size_t                  slen = 0;

    if (!handle || !label)
        return IAMF_ERR_BAD_ARG;

    ctx = &handle->ctx;

    if (ctx->mix_presentation_label && !strcmp(ctx->mix_presentation_label, label))
        return IAMF_OK;

    if (ctx->mix_presentation_label)
        free (ctx->mix_presentation_label);

    slen = strlen(label);
    ctx->mix_presentation_label = IAMF_MALLOC (char, slen + 1);
    strcpy(ctx->mix_presentation_label, label);
    ctx->mix_presentation_label[slen] = 0;

    return IAMF_OK;
}

int IAMF_layout_sound_system_channels_count (IAMF_SoundSystem ss)
{
    int ret = 0;
    if (!iamf_sound_system_valid(ss)) {
        return IAMF_ERR_BAD_ARG;
    }
    ret = iamf_sound_system_channels_count_without_lfe(ss);
    ret += iamf_sound_system_lfe1(ss);
    ret += iamf_sound_system_lfe2(ss);
    ia_logd("sound system %x, channels %d", ss, ret);
    return ret;
}

int IAMF_layout_binaural_channels_count ()
{
    return 2;
}

int IAMF_decoder_set_pts (IAMF_DecoderHandle handle, uint32_t pts, uint32_t time_base)
{
    IAMF_DecoderContext    *ctx;
    if (!handle)
        return IAMF_ERR_BAD_ARG;

    ctx = &handle->ctx;
    ctx->pts = pts;
    ctx->pts_time_base = time_base;
    ctx->duration = 0;
    ia_logd("set pts %u/%u", pts, time_base);

    return IAMF_OK;
}

int IAMF_decoder_get_last_metadata (IAMF_DecoderHandle handle, uint32_t *pts, IAMF_extradata *metadata)
{
    IAMF_DecoderContext    *ctx;
    uint64_t                d;
    if (!handle || !pts || !metadata)
        return IAMF_ERR_BAD_ARG;

    ctx = &handle->ctx;
    d = (uint64_t) ctx->pts_time_base * (ctx->duration - ctx->last_frame_size) / ctx->duration_time_base;
    *pts = ctx->pts + (uint32_t) d;
    ia_logd("pts %u/%u, last duration %u/%u", *pts, ctx->pts_time_base, ctx->duration - ctx->last_frame_size, ctx->duration_time_base);

    iamf_extra_data_copy(metadata, &ctx->metadata);
    metadata->number_of_samples = ctx->last_frame_size;
    iamf_extra_data_dump(metadata);
    return IAMF_OK;
}


