#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "bitstreamrw.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_obu.h"
#include "immersive_audio_utils.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAOBU"

static const char* gIAOBUTName[] = {
    "ia stream indicator",
    "codec specific info",
    "ia static meta",
    "temporal delimitor",
    "demixing info",
    "recon gain info",
    "substream"
};

static const char* ia_obu_type_name(IAOBUType t)
{
    return t <= IA_OBU_SUBSTREAM && t >= IA_OBU_IA_STREAM_INDICATOR ?
        gIAOBUTName[t] : "unknown";
}

static IAErrCode ia_obu_find (IAOBU *u, IAOBUType type,
                              uint8_t *data, uint32_t len)
{
    uint32_t left = len;
    uint32_t last_obu_len = 0;
    uint8_t *p = data;
    IAErrCode ec = IA_ERR_INVALID_PACKET;
    int ret = 0;

    memset ((char*)u, 0, sizeof(IAOBU));
    while (left > last_obu_len) {
        left -= last_obu_len;
        p += last_obu_len;
        ret = ia_obu_stream_parse (u, p, left);
        if (ret < 0) break;

        if (u->type == type) {
            ec = IA_OK;
            break;
        }

        last_obu_len = ret;

        if (last_obu_len > left) break;
    }

    return ec;
}


IAErrCode ia_obu_find_codec_specific_info (IAOBU *u, uint8_t *data, uint32_t len)
{
    return ia_obu_find (u, IA_OBU_CODEC_SPECIFIC_INFO, data, len);
}


IAErrCode ia_obu_find_static_meta (IAOBU *u, uint8_t *data, uint32_t len)
{
    return ia_obu_find (u, IA_OBU_IA_STATIC_META, data, len);
}


IAErrCode ia_obu_find_demixing_info (IAOBU *u, uint8_t *data, uint32_t len)
{
    return ia_obu_find (u, IA_OBU_DEMIXING_INFO, data, len);
}


/**
 *  obu_header
 *  {
 *      obu_forbidden_bit       f(1)
 *      obu_type                f(4)
 *      obu_reserved_1bit       f(1)
 *      obu_has_size_field      f(1)
 *      obu_reserved_1bit       f(1)
 *  }
 *
 * */


int ia_obu_stream_parse (IAOBU *u, uint8_t *data, uint32_t len)
{
    int idx;
    int obu_hsf;
    uint64_t obu_size = 0;
    bitstream_t bs;

    bs_init (&bs, data, len);
    bs_getbits(&bs, 1);  // forbidden bit, shall be 0.
    u->type = bs_getbits(&bs, 4);

    bs_getbits(&bs, 1); // reserved bit
    obu_hsf = bs_getbits(&bs, 1);

    idx = 1;

    if (obu_hsf)
        idx += leb128_read(data + idx, len - idx, &obu_size);
    else
        obu_size = len - idx;

    ia_logd("obu header: type %u(%s), has size field %d, size %ld",
            u->type, ia_obu_type_name(u->type), obu_hsf, obu_size);

    if (obu_size) {
        u->payload = data + idx;
        u->psize = obu_size;
    } else {
        u->payload = 0;
        u->psize = 0;
    }

    return obu_size + idx;
}
