#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "opus_defines.h"
#include "opus_multistream.h"

#include "bitstreamrw.h"
#include "fixedp11_5.h"

#include "immersive_audio_debug.h"
#include "immersive_audio_decoder.h"
#include "channel.h"
#include "demixer.h"
#include "drc_processor.h"
#include "audio_effect_peak_limiter.h"


#define OPUS_MAX_SUB_PACKET_SIZE            1277

/**
 * Util interfaces
 * */

#define OPUS_FREE(ptr) if (ptr) free(ptr);

#define RSHIFT(a) (1 << (a))
#define LAYOUT_FLAG(type) RSHIFT(type)
#define CHANNEL_FLAG(type) RSHIFT(type)


typedef struct ChannelLayout {
  int nb_channels;
  int nb_streams;
  int nb_coupled_streams;
  unsigned char mapping[256];
} ChannelLayout;


#define CELT_SIG_SCALE 32768.f

#define MIN32(a,b) ((a) < (b) ? (a) : (b))   /**< Minimum 32-bit value.   */
#define MAX32(a,b) ((a) > (b) ? (a) : (b))   /**< Maximum 32-bit value.   */

#include <math.h>
#define float2int(x) lrintf(x)

static  int16_t FLOAT2INT16(float x)
{
  x = x*CELT_SIG_SCALE;
  x = MAX32(x, -32768);
  x = MIN32(x, 32767);
  return (int16_t)float2int(x);
}

static void swap(void **p1, void **p2) {
    void *p = *p2;
    *p2 = *p1;
    *p1 = p;
}

/*
 * Util interfaces
 * */


inline static int check_layout_type(uint32_t type)
{
    return type < channel_layout_type_count;
}

/**
 * Opus channel group private interfaces.
 * */

static const int gs_layout_channel_count[] = {
    1, 2, 6, 8, 10, 8, 10, 12, 6
};

inline int get_layout_channel_count(int type)
{
    return check_layout_type(type) ? gs_layout_channel_count[type] : 0;
}

/* #define WAV_LAYOUT */
#ifdef WAV_LAYOUT
static const uint8_t gs_layout_channels[][12] = {
    {channel_mono},
    {channel_l2, channel_r2},
    {channel_l5, channel_r5, channel_c, channel_lfe, channel_sl5, channel_sr5},
    {channel_l5, channel_r5, channel_c, channel_lfe, channel_sl5, channel_sr5,
        channel_hl, channel_hr},
    {channel_l5, channel_r5, channel_c, channel_lfe, channel_sl5, channel_sr5,
        channel_hfl, channel_hfr, channel_hbl, channel_hbr},
    {channel_l7, channel_r7, channel_c, channel_lfe, channel_sl7, channel_sr7,
        channel_bl7, channel_br7},
    {channel_l7, channel_r7, channel_c, channel_lfe, channel_sl7, channel_sr7,
        channel_bl7, channel_br7, channel_hl, channel_hr},
    {channel_l7, channel_r7, channel_c, channel_lfe, channel_sl7, channel_sr7,
        channel_bl7, channel_br7,
        channel_hfl, channel_hfr, channel_hbl, channel_hbr},
    {channel_l3, channel_r3, channel_c, channel_lfe, channel_tl, channel_tr}
};
#else
static const uint8_t gs_layout_channels[][12] = {
    {channel_mono},
    {channel_l2, channel_r2},
    {channel_l5, channel_c, channel_r5, channel_sl5, channel_sr5, channel_lfe},
    {channel_l5, channel_c, channel_r5, channel_sl5, channel_sr5,
        channel_hl, channel_hr, channel_lfe},
    {channel_l5, channel_c, channel_r5, channel_sl5, channel_sr5,
        channel_hfl, channel_hfr, channel_hbl, channel_hbr, channel_lfe},
    {channel_l7, channel_c, channel_r7, channel_sl7, channel_sr7,
        channel_bl7, channel_br7, channel_lfe},
    {channel_l7, channel_c, channel_r7, channel_sl7, channel_sr7,
        channel_bl7, channel_br7, channel_hl, channel_hr, channel_lfe},
    {channel_l7, channel_c, channel_r7, channel_sl7, channel_sr7,
        channel_bl7, channel_br7, channel_hfl, channel_hfr,
        channel_hbl, channel_hbr, channel_lfe},
    {channel_l3, channel_c, channel_r3, channel_tl, channel_tr, channel_lfe}
};
#endif

inline const uint8_t* get_layout_channels (int type)
{
    return check_layout_type(type) ? gs_layout_channels[type] : 0;
}

static const char* gs_str_layout[] = {
    "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4",
    "3.1.2"
};

const char *get_layout_name(int type)
{
    return check_layout_type(type) ? gs_str_layout[type] : "unknown";
}

static const char* gs_channel_name[] = {
    "l7/l5", "r7/r5", "c", "lfe", "sl7", "sr7", "bl7", "br7", "hfl", "hfr",
    "hbl", "hbr", "mono", "l2", "r2", "tl", "tr", "l3", "r3", "sl5", "sr5",
    "hl", "hr"
};

inline const char* get_channel_name(uint32_t ch)
{
    return ch < channel_cnt ? gs_channel_name[ch] : "unknown";
}

static const char* gs_recon_gain_channel_name[] = {
    "l", "c", "r", "ls(lss)", "rs(rss)", "ltf", "rtf", "lb(lrs)", "rb(rrs)",
    "ltb(ltr)", "rtb(rtr)", "lfe"
};

inline const char* get_recon_gain_channel_name (uint32_t ch)
{
    return ch < rg_channel_cnt ? gs_recon_gain_channel_name[ch] : "unknown";
}

static const unsigned char mapping_default[] =
                    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint32_t mono_channels_mask =
                    CHANNEL_FLAG(channel_c) | CHANNEL_FLAG(channel_lfe);

static int leb128(uint8_t *bs, int32_t len, uint64_t* size)
{
    uint64_t value = 0;
    int Leb128Bytes = 0, i;
    uint8_t leb128_byte;
    for ( i = 0; i < 8; i++ ) {
        leb128_byte = bs[i];
        ia_logt("leb128: %d-byte : value %u", i + 1, bs[i]);
        value |= ( (leb128_byte & 0x7f) << (i*7) );
        Leb128Bytes += 1;
        if ( !(leb128_byte & 0x80) ) {
            break;
        }
    }
    *size = value;
    ia_logt("leb128: %d-bytes : value %lu", i + 1, value);
    return i + 1;
}


/*
 * Opus channel group private interfaces.
 * */


/**
 * channel group decoder internal interfaces.
 * */

static const int gs_max_frame_size = 960 * 6;
static const int gs_frame_size = 960;


typedef struct ChannelGroupContext {
    unsigned char   *data;
    int             len;

    int             layout;
    int             loudness;
    int             gain;
    int             recon_gain_flag;

    uint8_t         channels[256];
    ChannelLayout   clayout;
    OpusMSDecoder   *sdec;
} ChannelGroupContext;


typedef struct PacketContext {
    int         count;

    /* static meta */
    int         recon_gain_flag[channel_layout_type_count];
    float       gain[channel_layout_type_count];
    int         loudness[channel_layout_type_count];
    int         layout[channel_layout_type_count];

    /* timed meta */
    uint8_t*    data;
    int32_t     len;

    int         demixing_mode;

    int         sub_packet_len[channel_layout_type_count];
    uint8_t     recon_gain[rg_channel_cnt];
} PacketContext;


#define DEC_BUF_CNT 3
struct IADecoder {
    int32_t             fs;
    int                 channels;
    int                 groups;
    int                 amb;
    uint32_t            layout_flags;
    int                 target_layout;

    ChannelGroupContext ctx[channel_layout_type_count];
    PacketContext       packet;
    int                 target_group;

    void*                   decoder;
    Demixer*                demixer;
    AudioEffectPeakLimiter  limiter;

    uint8_t*        buffer[DEC_BUF_CNT];
    uint8_t         packet_channel_order[256];
    uint8_t         sub_packet_order[channel_layout_type_count];
};


static int timed_metadata_parse(uint8_t *bs, int32_t len,
        PacketContext *packet)
{
    int flag = 0, rg_flags;

    uint8_t *pd = bs;
    uint64_t size;
    int ss;

    if (packet->demixing_mode < 0 && packet->count > 1) {

/**
 *  Demixing_Info
 *  {
 *      Info_Extension_Flag     f(1)
 *      Dmixp_Mode              f(3)
 *      reserved                f(4)
 *      if (Info_Extension_Flag == 1)
 *          Extension_Field();
 * }
 *
 * */
        flag = *pd >> 7 & 0x01;
        packet->demixing_mode = *pd >> 4 & 0x07;
        ia_logi("demixing mode %d", packet->demixing_mode);
        ++pd;
        if (flag) {
            ss = leb128(pd, len - (pd - bs), &size);
            pd += (ss + size);
        }
    }

    ia_logi("timed metadata packet count %d.", packet->count);
    /* ChannelGroupSpecificationInformation */
    for (int g=0; g<packet->count; ++g) {

        // flag
        flag = *pd >> 7 & 0x01;
        ++pd;

        // channel group size.
        ss = leb128(pd, len - (pd - bs), &size);
        packet->sub_packet_len[g] = size;
        pd += ss;

        ia_logi("timed metadata: CG#%d, flag %d, size %lu, offset %ld",
                g, flag, size, pd - bs);

        // option: sub-stream size.
        // option: recon gain.
        if (packet->recon_gain_flag[g]) {
            int chcnt = 0;

            ss = leb128(pd, len - (pd - bs), &size);
            pd += ss;

            rg_flags = size & 0xFFFF;
            ss == 1 ? chcnt = 7 : ss > 1 ? chcnt = 12 : 0;

            ia_logi("recon gain flays (size %d) : 0x%x(0x%lx)",
                    ss, rg_flags, size);
            for (int i = 0; i<chcnt; ++i) {
                if (rg_flags & CHANNEL_FLAG(i)) {
                    packet->recon_gain[i] = *pd++;
                    ia_logi("Channel %s(%d) recon gain %u(0x%x)",
                            get_recon_gain_channel_name(i), i,
                            packet->recon_gain[i],
                            packet->recon_gain[i] & 0xFF);
                }
            }
        }

        if (flag) {
            ss = leb128(pd, len - (pd - bs), &size);
            pd += (ss + size);
        }
    }

    ia_logt("parsed %ld", pd - bs);

    return OPUS_OK;
}



enum OBUType {
    OBU_CODEC_SEPCIFIC_INFO = 1,
    OBU_IA_STATIC_META,
    OBU_TIMED_META,
    OBU_IA_CODED_DATA
};


static int obu_parse_unit(uint8_t *bs, int32_t len,
        PacketContext *packet)
{
    int idx;
    int obu_f, obu_type, obu_ef, obu_hsf;
    uint64_t obu_size = 0;

/**
 *  obu header
 *  {
 *      obu_forbidden_bit       f(1)
 *      obu_type                f(4)
 *      obu_extension_flag      f(1)
 *      obu_has_size_field      f(1)
 *      obu_reserved_1bit       f(1)
 *      if (obu_extension_flag == 1)
 *          obu_extension_header()
 *  }
 * */

    obu_f = bs[0] >> 7 & 0x01;
    obu_type = bs[0] >> 3 & 0x0F;
    obu_ef = bs[0] >> 2 & 0x01;
    obu_hsf = bs[0] >> 1 & 0x01;


    idx = 1;
    if (obu_ef)
        idx += 1;

    if (obu_hsf)
        idx += leb128(bs + idx, len - idx, &obu_size);
    else
        obu_size = len - idx;

    ia_logd("obu header: forbidden %d, type %d, extension %d,"
            " has size field %d, size %ld",
            obu_f, obu_type, obu_ef, obu_hsf, obu_size);

    switch (obu_type) {
        case OBU_TIMED_META:
            timed_metadata_parse(bs + idx, obu_size, packet);
            break;
        case OBU_IA_CODED_DATA: // extract audio data packet.
            packet->data = bs + idx;
            packet->len = obu_size;
            break;
    }

    return obu_size + idx;
}



static int gs_714channel[channel_cnt] = {
    channel_l7, // channel_l5,
    channel_r7, // channel_r5,
    channel_c,
    channel_lfe,
    channel_sl7,
    channel_sr7,
    channel_bl7,
    channel_br7,
    channel_hfl,
    channel_hfr,
    channel_hbl,
    channel_hbr,
    channel_bl7, // channel_mono
    channel_bl7, // channel_l2,
    channel_br7, // channel_r2,
    channel_hbl, // channel_tl,
    channel_hbr, // channel_tr,
    channel_bl7, // channel_l3,
    channel_br7, // channel_r3,
    channel_bl7, // channel_sl5,
    channel_br7, // channel_sr5,
    channel_hbl, // channel_hl,
    channel_hbr, // channel_hr,
};

inline static int map_714channel(int ch)
{
    return ch < channel_cnt ? gs_714channel[ch] : -1;
}

static uint32_t gs_layout_channels_masks[] = {
    0x00000040, 0x000000C0, 0x000000CF, 0x00000CCF, 0x00000FCF, 0x000000FF,
    0x00000CFF, 0x00000FFF, 0x00000CCC
};

inline static uint32_t get_layout_channels_714channel_mask (int type)
{
    return check_layout_type(type) ? gs_layout_channels_masks[type] : 0;
}

static int get_new_channels(uint32_t base, uint32_t target, uint8_t* channels)
{
    int tcnt = get_layout_channel_count(target);
    const uint8_t *tchs = get_layout_channels(target);
    uint32_t mask = get_layout_channels_714channel_mask(base);
    int idx = 0;

    ia_logd("layout %s(%d) channels mask %08X", get_layout_name(base),
            base, mask);

    if (mask) {
        int bcnt = get_layout_channel_count(base);
        uint32_t chflag;
        for (int ti=0; ti<tcnt; ++ti) {
            ia_logt("check target channel: %s(map_714channel name %s)",
                    get_channel_name(tchs[ti]),
                    get_channel_name(map_714channel(tchs[ti])));
            chflag = CHANNEL_FLAG (map_714channel(tchs[ti]));
            if (chflag & mono_channels_mask) {
                ia_logt("target channels: skip channel %s.",
                        get_channel_name(tchs[ti]));
                continue;
            }
            if (!(chflag & mask)) {
                channels[idx++] = tchs[ti];
                ia_logd("add new channel: %s(%d)",
                        get_channel_name(tchs[ti]), tchs[ti]);
            }
        }

        if (idx != (tcnt - bcnt)) {
            ia_logd("current new channes %d vs %d", idx, tcnt - bcnt);
            channels[idx++] = channel_c;
            channels[idx++] = channel_lfe;
        }
    } else if (base == (uint32_t)channel_layout_type_invalid) {
        int ti = 0;
        for (; ti<tcnt; ++ti) {
            if (CHANNEL_FLAG(tchs[ti]) & mono_channels_mask) {
                ia_logt("target channels: skip channel %s.",
                        get_channel_name(tchs[ti]));
                continue;
            }
            channels[idx++] = tchs[ti];
        }
        if (idx != ti) {
            channels[idx++] = channel_c;
            channels[idx++] = channel_lfe;
        }
    }

    for (int i=0; i<idx; ++i)
        ia_logt("new channel %s", get_channel_name(channels[i]));

    return idx;
}


static void immersive_audio_sub_decoder_destroy (IADecoder *st)
{
    if (st) {
        for (int i=0; i<=st->target_group; ++i)
            OPUS_FREE(st->ctx[i].sdec);
    }
}

static void immersive_audio_packet_update(IADecoder *st)
{
    PacketContext *pkt = &st->packet;
    pkt->count = st->groups;
    ia_logi("packet update: count %d.", pkt->count);
    for (int g=0; g<pkt->count; ++g) {
        pkt->recon_gain_flag[g] = st->ctx[g].recon_gain_flag;
        pkt->gain[g] =
            st->ctx[g].gain ? qf_to_float((qf_t)st->ctx[g].gain, 8) : 0;
        ia_logi("CG#%d: gain %d -> %f", g, st->ctx[g].gain, pkt->gain[g]);
        pkt->loudness[g] = st->ctx[g].loudness;
        pkt->layout[g] = st->ctx[g].layout;
    }
    pkt->demixing_mode = -1;
}


static void copy_channel_out (void *dst, const float *src, int frame_size,
        int channels)
{
    float *out = (float *)dst;
    int idx, i,j;
    ia_logd("dst %p, src %p, frame_size %d, channels %d", dst, src,
            frame_size, channels);
    if (src) {
        for (j=0;j<channels;j++) {
            idx = j*frame_size;
            for (i=0;i<frame_size;i++) {
                out[idx + i] = src[i*channels + j];
            }
        }
    } else {
        memset(dst, 0, sizeof(float)*frame_size*channels);
    }
}


static int immersive_audio_static_metadata_parse (IADecoder *st,
    const unsigned char *data, int32_t len, int32_t codec_info)
{
    bitstream_t bs, *p;
    uint8_t *pd;
    ChannelGroupContext *c;
    int flag, cmf;

    p = &bs;
    pd = (uint8_t*) data;

    if (codec_info) {

/**
 * codec specification info {
 *     codec id (32)
 *     4cc (32)
 *     version (8)
 *     output channel count (8)
 *     pre-skip (16)
 *     input sample rate (32)
 *     output gain (16)
 *     channel mapping faily (8)
 * }
 * */

#define CODEC_ID_OPUS ((('o') << 24) | (('p') << 16) | (('u') << 8) | ('s'))

#if 0
        uint32_t ccid = get_uint32be(pd, 0);
        if (ccid != CODEC_ID_OPUS) {
            ia_loge("codec id : %.4s", pd);
            return OPUS_BAD_ARG;
        }
#endif

        st->channels =  get_uint8(pd, 9);
        st->fs = get_uint32be(pd, 12);
        cmf = get_uint8(pd, 18);

        pd += 19;
        ia_logi("codec info: %.4s,"
                " channels %d, input sample rate %u, channel mapping family %d,"
                " size %ld",
                pd, st->channels, st->fs, cmf, pd - data);
    }

    bs_init (p, pd, len - (pd - data));

/**
 *  IA static metadata {
 *      version (8)
 *      ambisonics mode (2)
 *      channel audio layer (3)
 *      reserved (2)
 *      sub-stream size is present flag (1)
 *      if (ambisonics_mode == 1 or 2)
 *          ambisonics layer configuration (ambisonics)
 *      for ( i in channel_audio_layer )
 *          channel audio layer configuration (i)
 *  }
 * */
    bs_getbits(p, 8);
    st->amb = bs_getbits(p, 2);
    st->groups = bs_getbits(p, 3);
    bs_getbits(p, 3);

    ia_logi("static metadata : ambisonics mode %d, channel audio layer %d",
            st->amb, st->groups);
    if (st->amb == 1 || st->amb == 2) {
        ia_logw ("Do not support ambisonisc mode.");
        return OPUS_INTERNAL_ERROR;
    }

    /**
     *  channel audio layer configuration {
     *      loudspeaker layout (4)
     *      output gain is present flag (1)
     *      recon gain is present flag (1)
     *      reserved (2)
     *      sub-stream count (8)
     *      coupled sub-stream count (8)
     *      loudness (s16)
     *      if (output_gain_is_present_flag)
     *          output gain (s16)
     *  }
     * */

    for (int g=0; g<st->groups; ++g) {
        c = &st->ctx[g];
        c->layout = bs_getbits(p, 4);
        st->layout_flags |= LAYOUT_FLAG(c->layout);
        flag = bs_getbits(p, 1);
        c->recon_gain_flag = bs_getbits(p, 1);
        bs_getbits(p, 2);
        c->clayout.nb_streams = bs_getbits(p, 8);
        c->clayout.nb_coupled_streams = bs_getbits(p, 8);
        c->loudness = bs_getbits(p, 16);
        if (flag)
            c->gain = bs_getbits(p, 16);
        ia_logi("CG#%d: layout %d, output gain flag %d, recon gain flag %d,"
                " streams %d, coupled streams %d, loudness %d (0x%04x),"
                " output gain %d (0x%04x) ",
                g, c->layout, flag, c->recon_gain_flag, c->clayout.nb_streams,
                c->clayout.nb_coupled_streams, c->loudness, c->loudness,
                flag ? c->gain : 0, flag ? c->gain : 0);
    }

    return OPUS_OK;
}

static int immersive_audio_context_update(IADecoder *st)
{
    OpusMSDecoder *mst;
    ChannelGroupContext *ctx;
    int channels, last;
    int s,cs,layout;
    uint8_t *mapping;
    int ret = OPUS_OK;

    last = channel_layout_type_invalid;

    for (int i=0; i<st->groups; ++i) {
        ctx = &st->ctx[i];
        layout = st->ctx[i].layout;
        s = ctx->clayout.nb_streams;
        cs = ctx->clayout.nb_coupled_streams;
        channels = ctx->clayout.nb_channels = s + cs;
        mapping = (uint8_t *)mapping_default;

        ret = get_new_channels(last, layout, ctx->channels);
        if (ret != s+cs) {
            ia_loge("ERROR: s%d, c%d, ret %d", s, cs, ret);
            return ret;
        }

        mst = opus_multistream_decoder_create (st->fs, channels, s, cs,
                (const unsigned char*)mapping, &ret);

        if (ret != OPUS_OK) {
            ia_loge("ERROR: s%d, c%d, msd(%d) init failed. ret %d",
                    s, cs, i, ret);
            goto termination;
        }
        ctx->sdec = mst;

        ia_logi("sub-decoder (%d): channels %d,"
                " streams %d, coupled_streams %d, mapping :%s",
                i, ctx->clayout.nb_channels, ctx->clayout.nb_streams,
                ctx->clayout.nb_coupled_streams, mapping);

        for (int i=0; i<channels; ++i)
            ia_logi(" %d", mapping[i]);
        last = layout;
    }

    return OPUS_OK;

termination:
    immersive_audio_sub_decoder_destroy(st);
    return ret;
}


static int immersive_audio_find_group_index(IADecoder *st, int layout)
{
    int g = 0;
    for (; g<st->groups; ++g)
        if (st->ctx[g].layout == layout)
            break;
    return g == st->groups ? OPUS_BAD_ARG : g;
}


static int immersive_audio_packet_parse (IADecoder *st,
    const unsigned char *data, int32_t len, int demixing_mode)
{
    PacketContext *packet = &st->packet;
    uint8_t *last;
    int ret = 0;

    ia_logi("packet %p, size %d, demixing mode %d",
            data, len, demixing_mode);
    packet->demixing_mode = demixing_mode;
    memset(packet->recon_gain, 0, sizeof(packet->recon_gain));

    int idx = 0;
    uint8_t *p = (uint8_t *)data;
    while (idx < len) {
        ret = obu_parse_unit(p + idx, len - idx, packet);
        ia_logd ("obu unit size %d", ret);
        idx += ret;
    }

    last = packet->data;
    for(int g=0; g<packet->count; ++g) {
        st->ctx[g].data = last;
        st->ctx[g].len = packet->sub_packet_len[g];
        last += packet->sub_packet_len[g];
    }

    return OPUS_OK;
}


static int immersive_audio_internal_decode (IADecoder *st,
    float *pcm, int frame_size, int decode_fec)
{
    ChannelGroupContext *ctx;
    int ret = gs_frame_size;
    float *out = pcm;
    float *dout = (float *)st->buffer[2];
    OpusMSDecoder *mst;

    for (int g=0; g<=st->target_group; ++g) {
        ctx = &st->ctx[g];
        mst = ctx->sdec;
        ia_logd("CG#%d: start code %X(%p), length %d, channels %d,"
                " out %p, offset %lX, size %lu",
                g, ctx->data[0], ctx->data, ctx->len, ctx->clayout.nb_channels,
                out, (void *)out - (void *)pcm,
                sizeof(float) * gs_frame_size * ctx->clayout.nb_channels);
        ret = opus_multistream_decode_float(mst, ctx->data, ctx->len, dout,
                frame_size, 0);
        if(ret < 0) {
            ia_loge("sub packet %d decode fail.", g);
            break;
        } else if (ret != gs_frame_size) {
            ia_loge("decoded frame size is not %d (%d).", gs_frame_size, ret);
            break;
        }
        copy_channel_out(out, dout, ret, ctx->clayout.nb_channels);
        out += (ret * ctx->clayout.nb_channels);
    }

    return ret;
}

static int immersive_audio_demixer_update_param(IADecoder *st,
    DemixingParam *param)
{
    param->demixing_mode = st->packet.demixing_mode;
    param->steps = st->target_group + 1;
    param->gain = st->packet.gain;
    param->recon_gain_flag = st->packet.recon_gain_flag[st->target_group];
    param->recon_gain = st->packet.recon_gain;
    param->layout = st->packet.layout;
    param->channel_order = st->packet_channel_order;

    return OPUS_OK;
}


static int immersive_audio_packet_demix (IADecoder *st,
    float *in, int frame_size, float *out)
{
    DemixingParam param;

    ia_logd("Update demixer parameters.");
    immersive_audio_demixer_update_param(st, &param);
    return demixer_demix(st->demixer, in, frame_size, out, &param);
}


static void immersive_audio_packet_drc (IADecoder *st,
    float *pcm, int frame_size, float *out)
{
    short LKFSnch = st->packet.loudness[st->target_group];
    float input_loudness = q_to_float((q16_t)LKFSnch, 8);

    ia_logd("LKFS(%d->%f)", LKFSnch, input_loudness);
    drc_process_block(0, input_loudness, (float *)pcm, (float *)out,
            frame_size, get_layout_channel_count(st->target_layout),
            &st->limiter);
}


static void immersive_audio_copy_channel_out_short (IADecoder *st,
    const float *src, int frame_size, void *out)
{
   int16_t *short_dst;
   int32_t i, ch;
   short_dst = (int16_t*)out;
   unsigned char *mapping = (unsigned char *)mapping_default;

   ia_logd("channels %d", st->channels);
   for (ch = 0; ch<st->channels;++ch) {
       if (src != NULL) {
          for (i=0;i<frame_size;i++)
             short_dst[i * st->channels + mapping[ch]] =
                 FLOAT2INT16(src[frame_size * ch + i]);
       } else {
          for (i=0;i<frame_size;i++)
             short_dst[i * st->channels + mapping[ch]] = 0;
       }
   }
}


/**
 * Opus channel group decoder APIs.
 * */


int32_t channel_layout_get_channel_count(int32_t type)
{
    return get_layout_channel_count(type);
}


int32_t immersive_audio_decoder_get_size()
{
    return sizeof(IADecoder);
}


int immersive_audio_decoder_init(IADecoder *st, int32_t Fs,
    int channels, const unsigned char *meta, int32_t len,
    int32_t codec_info)
{
    int ret = OPUS_OK;

    ia_logi("sample rate %d, channels %d, meta %p, length %d, codec info %d",
            Fs, channels, meta, len, codec_info);

    if (!meta || len < 1)
        return OPUS_BAD_ARG;

    memset(st, 0x00, immersive_audio_decoder_get_size());

    if (!codec_info) {
        st->fs = Fs;
        st->channels = channels;
    }

    ret = immersive_audio_static_metadata_parse(st, meta, len, codec_info);
    if (ret != OPUS_OK) {
        ia_loge("failed to parse static meta data.");
        goto termination;
    }

    ret = immersive_audio_context_update(st);
    if (ret != OPUS_OK) {
        goto termination;
    }

    immersive_audio_packet_update(st);

    ret = immersive_audio_set_layout(st, st->ctx[st->groups - 1].layout);
    if (ret != OPUS_OK) {
        ia_loge("failed to set layout %d.", st->target_layout);
        goto termination;
    }

    st->demixer = demixer_create();
    if (!st->demixer) {
        ia_loge("demixer is allocated failed.");
        ret = OPUS_ALLOC_FAIL;
        goto termination;
    }
    demixer_init(st->demixer);

    ia_logi("buffer size %u (%ld*%d*%d)",
            sizeof(float) * gs_max_frame_size * st->channels,
            sizeof(float), gs_max_frame_size, st->channels);

    for (int i=0; i<DEC_BUF_CNT; ++i) {
        st->buffer[i] =
            (uint8_t *)malloc(sizeof(float) * gs_max_frame_size * st->channels);
        if (!st->buffer[i]) {
            ia_loge("buffer is allocated failed.");
            ret = OPUS_ALLOC_FAIL;
            goto termination;
        }
    }

    return OPUS_OK;

termination:
    immersive_audio_decoder_uninit(st);
    return ret;
}


IADecoder *immersive_audio_decoder_create(int32_t Fs, int channels,
    const unsigned char *meta, int32_t len, int32_t codec_info,
    int *error)
{
    int size, ret;
    IADecoder *st;

    size = immersive_audio_decoder_get_size();
    ia_logi("IADecoder size %d", size);
    if (!size) {
        if (error)
            *error = OPUS_ALLOC_FAIL;
        return NULL;
    }
    st = (IADecoder *)malloc(size);
    if (!st) {
        if (error)
            *error = OPUS_ALLOC_FAIL;
        return NULL;
    }

    /* Initialize channel group decoder with provided settings. */
    ia_logd("Initialize channel group decoder with provided settings.");
    ret = immersive_audio_decoder_init(st, Fs, channels, meta, len,
            codec_info);
    if (ret != OPUS_OK) {
        free(st);
        st = NULL;
    }
    if (error)
        *error = ret;
    return st;
}


int immersive_audio_get_valid_layouts (IADecoder *st,
    int32_t types[channel_layout_type_count])
{
    if (!st || !st->groups)
        return OPUS_INVALID_STATE;

    for (int i=0; i<st->groups; ++i)
        types[i] = st->ctx[i].layout;

    return st->groups;
}


int immersive_audio_set_layout (IADecoder *st, int32_t type)
{
    int idx = 0, ret = OPUS_OK;
    int channels;

    ia_logi("set layout %s(%d)", get_layout_name(type), type);
    if (!st || !st->groups)
        return OPUS_INVALID_STATE;

    if (!check_layout_type(type) || !(st->layout_flags & LAYOUT_FLAG(type))) {
        ia_loge("this layout %d is invalid, please get valid layouts firstly",
                type);
        int32_t types[channel_layout_type_count];
        ret = immersive_audio_get_valid_layouts(st, types);
        if (ret > 0) {
            ia_logw("there are %d layouts:", ret);
            for (int i=0; i<ret; ++i)
                ia_logw("\t%d:%s", i, get_layout_name(types[i]));
        }
        return OPUS_BAD_ARG;
    }

    if (type == st->target_layout)
        return OPUS_OK;

    st->target_group = immersive_audio_find_group_index(st, type);
    st->target_layout = type;
    st->channels = channel_layout_get_channel_count(type);

    ia_logi("target group %d, layout %s(%d)", st->target_group,
            get_layout_name(type), type);
    for (int g=0; g<=st->target_group; ++g) {
        channels = st->ctx[g].clayout.nb_channels;

        for (int i=idx, j=0; j<channels; ++i, ++j) {
            st->packet_channel_order[i] = st->ctx[g].channels[j];
        }
        idx += channels;
    }

    ia_logd("Packet channel order:");
    channels = get_layout_channel_count(st->target_layout);
    for (int ch=0; ch < channels; ++ch)
        ia_logd("%2d: %2d (%s)", ch, st->packet_channel_order[ch],
                get_channel_name(st->packet_channel_order[ch]));

    drc_init_limiter(&st->limiter, st->fs, channels);
    ret = OPUS_OK;

    return ret;
}


int immersive_audio_decode(IADecoder *st,
    const unsigned char *data, int32_t len, int16_t *pcm, int frame_size,
    int decode_fec, int demixing_mode)
{
    int ret;
    int real_frame_size = 0;
    float *in, *out;

    if (st->groups < 1 || st->target_group < 0) {
        ia_loge("IADecoder is not initializated."
                " please call immersive_audio_decoder_init() and"
                " immersive_audio_decoder_audio_layer_init() firstly.");
        return OPUS_INVALID_STATE;
    }

    in = (float *)st->buffer[0];
    out = (float *)st->buffer[1];

    /**
     * 1. parse audio complex packet.
     * */
    ia_logd("Parse audio packet.");
    ret = immersive_audio_packet_parse(st, data, len, demixing_mode);
    if (ret != OPUS_OK)
        goto end;


    /**
     * 2. Decode audio data packet by OpusMSDecoder.
     * */
    ia_logd("Decode audio data packet by OpusMSDecoder.");
    ret = immersive_audio_internal_decode(st, out, frame_size, decode_fec);
    if (ret <= 0)
        goto end;


    /* D2F(1, out, sizeof(float) * ret * get_layout_channel_count(st->target_layout), "DEC"); */
    real_frame_size = ret;

    swap((void **)&in, (void **)&out);

    /**
     * 3. Gain-up, demixing and smoothing.
     * */
    ia_logd("Gain-up, demixing, smoothing.");
    ret = immersive_audio_packet_demix(st, in, real_frame_size, out);
    if (ret != OPUS_OK)
        goto end;

    swap((void **)&in, (void **)&out);

    /**
     * 4. Loudness normalization, drc and limit.
     * */
    ia_logd("Loudness normalization, drc and limit.");

    immersive_audio_packet_drc(st, in, real_frame_size, out);

    /**
     * 5. re-order and pack
     * */
    immersive_audio_copy_channel_out_short(st, out, real_frame_size, pcm);

end:
    return ret == OPUS_OK ? real_frame_size : ret;
}

void immersive_audio_decoder_uninit (IADecoder *st)
{
    if (st) {
        OPUS_FREE(st->decoder);

        for (int i=0; i<DEC_BUF_CNT; ++i)
            OPUS_FREE(st->buffer[i]);

        immersive_audio_sub_decoder_destroy(st);
        demixer_destroy(st->demixer);
        audio_effect_peak_limiter_uninit(&st->limiter);

        memset(st, 0x00, sizeof(IADecoder));
    }
}

void immersive_audio_decoder_destroy(IADecoder *st)
{
    if (st) {
        immersive_audio_decoder_uninit(st);
        free(st);
    }
}
