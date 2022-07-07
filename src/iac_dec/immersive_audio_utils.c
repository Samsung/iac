#include "immersive_audio_utils.h"

void* ia_mallocz (uint32_t size)
{
    void *p = 0;
    p = malloc(size);
    if (p)
        memset((char *)p, 0, size);
    return p;
}

void  ia_freep(void** p)
{
    if (p && *p) {
        free (*p);
        *p = 0;
    }
}

int ia_codec_check (IACodecID cid)
{
    return cid >= IA_CODEC_OPUS && cid < IA_CODEC_COUNT;
}


static const char* gIACName[IA_CODEC_COUNT] = {
    "OPUS",
    "AAC-LC"
};

const char* ia_codec_name (IACodecID cid)
{
    if (ia_codec_check(cid))
        return gIACName[cid];
    return "UNKNOWN";
}


static const char* gIAECString[] = {
    "Ok",
    "Bad argments",
    "Unknown",
    "Internal error",
    "Invalid packet",
    "Invalid state",
    "Unimplemented",
    "Memory allocation failure",
    "need more data."
};

const char* ia_error_code_string (IAErrCode ec)
{
    int cnt = sizeof(gIAECString) / sizeof(char *);
    int idx = -ec;
    if (idx >= 0 && idx < cnt)
        return gIAECString[idx];
    return "Unknown";
}


int ia_channel_layout_type_check (IAChannelLayoutType type)
{
    return type > IA_CHANNEL_LAYOUT_INVALID && type < IA_CHANNEL_LAYOUT_COUNT;
}


static const char* gIACLName[] = {
    "1.0.0",
    "2.0.0",
    "5.1.0", "5.1.2", "5.1.4",
    "7.1.0", "7.1.2", "7.1.4",
    "3.1.2"
};

const char* ia_channel_layout_name (IAChannelLayoutType type)
{
    if (ia_channel_layout_type_check(type))
        return gIACLName[type];
    return "Unknown";
}

static const int gIACLChCount[] = {
    1, 2, 6, 8, 10, 8, 10, 12, 6
};

int ia_channel_layout_get_channels_count (IAChannelLayoutType type)
{
    return ia_channel_layout_type_check(type) ? gIACLChCount[type] : 0;
}

static const IAChannel gIACLChannels[][IA_CH_LAYOUT_MAX_CHANNELS] = {
    {IA_CH_MONO},
    {IA_CH_L2, IA_CH_R2},
    {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_LFE},
    {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5,
        IA_CH_HL, IA_CH_HR,
        IA_CH_LFE},
    {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5,
        IA_CH_HFL, IA_CH_HFR, IA_CH_HBL, IA_CH_HBR,
        IA_CH_LFE},
    {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7,
        IA_CH_LFE},
    {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7,
        IA_CH_HL, IA_CH_HR,
        IA_CH_LFE},
    {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7,
        IA_CH_HFL, IA_CH_HFR, IA_CH_HBL, IA_CH_HBR,
        IA_CH_LFE},
    {IA_CH_L3, IA_CH_C, IA_CH_R3, IA_CH_TL, IA_CH_TR, IA_CH_LFE}
};

int ia_channel_layout_get_channels (IAChannelLayoutType type,
                                    IAChannel *channels, uint32_t count)
{
    int ret = 0;
    if (!ia_channel_layout_type_check(type))
        return 0;

    ret = ia_channel_layout_get_channels_count(type);
    if (count < ret)
        return IA_ERR_BUFFER_TOO_SMALL;

    for (int c=0; c<ret; ++c)
        channels[c] = gIACLChannels[type][c];

    return ret;
}

static const struct {
    int s;
    int w;
    int t;
} gIACLC2Count[IA_CHANNEL_LAYOUT_COUNT] = {
    {1, 0, 0},
    {2, 0, 0},
    {5, 1, 0},
    {5, 1, 2},
    {5, 1, 4},
    {7, 1, 0},
    {7, 1, 2},
    {7, 1, 4},
    {3, 1, 2},
};

int ia_channel_layout_get_category_channels_count (IAChannelLayoutType type,
                                                   uint32_t categorys)
{
    int chs = 0;
    if (!ia_channel_layout_type_check(type))
        return 0;

    if (categorys & IA_CH_CATE_TOP)
        chs += gIACLC2Count[type].t;
    if (categorys & IA_CH_CATE_WEIGHT)
        chs += gIACLC2Count[type].w;
    if (categorys & IA_CH_CATE_SURROUND)
        chs += gIACLC2Count[type].s;
    return chs;
}

static const IAChannel gIAALChannels[][IA_CH_LAYOUT_MAX_CHANNELS] = {
    {IA_CH_MONO},
    {IA_CH_L2, IA_CH_R2},
    {IA_CH_L5, IA_CH_R5, IA_CH_SL5, IA_CH_SR5,
        IA_CH_C, IA_CH_LFE},
    {IA_CH_L5, IA_CH_R5, IA_CH_SL5, IA_CH_SR5,
        IA_CH_HL, IA_CH_HR,
        IA_CH_C, IA_CH_LFE},
    {IA_CH_L5, IA_CH_R5, IA_CH_SL5, IA_CH_SR5,
        IA_CH_HFL, IA_CH_HFR, IA_CH_HBL, IA_CH_HBR,
        IA_CH_C, IA_CH_LFE},
    {IA_CH_L7, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7,
        IA_CH_C, IA_CH_LFE},
    {IA_CH_L7, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7,
        IA_CH_HL, IA_CH_HR,
        IA_CH_C, IA_CH_LFE},
    {IA_CH_L7, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7,
        IA_CH_HFL, IA_CH_HFR, IA_CH_HBL, IA_CH_HBR,
        IA_CH_C, IA_CH_LFE},
    {IA_CH_L3, IA_CH_R3,
        IA_CH_TL, IA_CH_TR,
        IA_CH_C, IA_CH_LFE}
};

int ia_audio_layer_get_channels (IAChannelLayoutType type, IAChannel *channels,
                                 uint32_t count)
{
    int ret = 0;
    if (!ia_channel_layout_type_check(type))
        return 0;

    ret = ia_channel_layout_get_channels_count(type);
    if (count < ret)
        return IA_ERR_BUFFER_TOO_SMALL;

    for (int c=0; c<ret; ++c)
        channels[c] = gIAALChannels[type][c];

    return ret;
}


static const char* gIAChName[] = {
    "unknown",
    "l7/l5", "r7/r5", "c", "lfe", "sl7", "sr7", "bl7", "br7",
    "hfl", "hfr", "hbl", "hbr",
    "mono",
    "l2", "r2",
    "tl", "tr",
    "l3", "r3",
    "sl5", "sr5",
    "hl", "hr"
};

const char* ia_channel_name (IAChannel ch)
{
    return ch < IA_CH_COUNT ? gIAChName[ch] : "unknown";
}


int leb128_read (uint8_t *data, int32_t len, uint64_t* size)
{
    uint64_t value = 0;
    int Leb128Bytes = 0, i;
    uint8_t leb128_byte;
    for ( i = 0; i < 8; i++ ) {
        leb128_byte = data[i];
        value |= ( ((uint64_t)leb128_byte & 0x7f) << (i*7) );
        Leb128Bytes += 1;
        if ( !(leb128_byte & 0x80) ) {
            break;
        }
    }
    *size = value;
    return i + 1;
}

int bit1_count (uint32_t value)
{
    int n = 0;
    for (; value ; ++n)
        value &= (value - 1);
    return n;
}

