#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bitstreamrw.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_metadata.h"
#include "immersive_audio_utils.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMETA"


int ia_codec_specific_info_parse (IACodecSpecInfo *info, uint8_t *spec, uint32_t size)
{
    if (size < 4 || !info)
        return IA_ERR_BAD_ARG;

    info->cid = *((uint32_t *)spec);
    info->config = spec + 4;
    info->size = size - 4;

    return IA_OK;
}

/**
 *
 *  ia_static_meta()
 *  {
 *      version                         f(8)
 *      ambisonics_mode                 f(2)
 *      channel_audio_layer             f(3)
 *      reserved                        f(3)
 *      if (ambisonics_mode == 1 or 2)
 *          ambix_layer_config (ambisonics_mode)
 *      for (i=0; i < ch_audio_layer; i++)
 *          ch_audio_layer_config(i)
 *  }
 *
 ******************************************************************************
 *
 *  ambix_layer_config (mode)
 *  {
 *      output_channel_count (C)        f(8)
 *      substream_count(0) (N)          f(8)
 *      coupled_substream_count(0) (M)  f(8)
 *      if (mode == 1) channel_mapping  f(8*C)
 *      if (mode == 2) demixing_matrix  f(16*(N+M)*C)
 *  }
 *
 ******************************************************************************
 *
 *  ch_audio_layer_config (i)
 *  {
 *      loudspeaker_layout(i)           f(4)
 *      output_gain_is_present_flag(i)  f(1)
 *      recon_gain_is_present_flag(i)   f(1)
 *      reserved                        f(2)
 *      substream_count(i)              f(8)
 *      coupled_substream_count(i)      f(8)
 *      loudness(i)                     su(16)
 *      if (output_gain_is_present_flag(i) == 1)
 *          output_gain_flags(i)        f(6)
 *          reserved                    f(2)
 *          output_gain(i)              su(16)
 *  }
 *
 * */
IAErrCode ia_static_meta_parse (IAStaticMeta *meta,
                                uint8_t *data, uint32_t len)
{
    IAErrCode ec = IA_OK;
    bitstream_t bs;
    uint32_t pos = 0;

    bs_init (&bs, data, len);

    ia_logt("static meta parsing ...");
    bs_getbits(&bs, 8);
    meta->ambisonics_mode = bs_getbits(&bs, 2);
    meta->channel_audio_layer = bs_getbits(&bs, 3);
    bs_getbits(&bs, 3);
    ia_logd ("ambisonics_mode : %d", meta->ambisonics_mode);
    ia_logd ("channel_audio_layer : %d", meta->channel_audio_layer);

    if (meta->ambisonics_mode > 0) {
        uint32_t bytes = 0;
        meta->ambix_layer_config.output_channel_count = bs_getbits(&bs, 8);
        meta->ambix_layer_config.substream_count = bs_getbits(&bs, 8);
        meta->ambix_layer_config.coupled_substream_count = bs_getbits(&bs, 8);
        if (meta->ambisonics_mode == 1) {
            bytes = sizeof(uint8_t) * meta->ambix_layer_config.output_channel_count;
            meta->ambix_layer_config.channel_mapping =
                (uint8_t *) malloc (bytes);
            if (!meta->ambix_layer_config.channel_mapping) {
                ec = IA_ERR_ALLOC_FAIL;
                ia_static_meta_uninit (meta);
                return ec;
            }

            pos = len - bs_available(&bs);
            memcpy(meta->ambix_layer_config.channel_mapping, data + pos, bytes);
            bs_init (&bs, data + pos + bytes, len - pos - bytes);

        } else if (meta->ambisonics_mode == 2) {
            bytes = sizeof(uint16_t) *
                (meta->ambix_layer_config.substream_count + meta->ambix_layer_config.coupled_substream_count) *
                meta->ambix_layer_config.output_channel_count;
            meta->ambix_layer_config.demixing_matrix =
                (uint16_t *) malloc (bytes);
            if (!meta->ambix_layer_config.demixing_matrix) {
                ec = IA_ERR_ALLOC_FAIL;
                ia_static_meta_uninit (meta);
                return ec;
            }

            pos = len - bs_available(&bs);
            memcpy(meta->ambix_layer_config.demixing_matrix, data + pos, bytes);
            bs_init (&bs, data + pos + bytes, len - pos - bytes);
        }
    }

    for (int i=0; i<meta->channel_audio_layer; ++i) {
        ia_logd ("layer : %d", i);
        meta->ch_audio_layer_config[i].loudspeaker_layout = bs_getbits(&bs, 4);
        meta->ch_audio_layer_config[i].output_gain_is_present_flag = bs_getbits(&bs, 1);
        meta->ch_audio_layer_config[i].recon_gain_is_present_flag = bs_getbits(&bs, 1);
        bs_getbits(&bs, 2);
        meta->ch_audio_layer_config[i].substream_count = bs_getbits(&bs, 8);
        meta->ch_audio_layer_config[i].coupled_substream_count = bs_getbits(&bs, 8);
        meta->ch_audio_layer_config[i].loudness = bs_getbits(&bs, 16);
        ia_logd("loudspeaker_layout : %d", meta->ch_audio_layer_config[i].loudspeaker_layout);
        ia_logd("output_gain_is_present_flag : %d", meta->ch_audio_layer_config[i].output_gain_is_present_flag);
        ia_logd("recon_gain_is_present_flag : %d", meta->ch_audio_layer_config[i].recon_gain_is_present_flag);
        ia_logd("substream_count : %d", meta->ch_audio_layer_config[i].substream_count);
        ia_logd("coupled_substream_count : %d", meta->ch_audio_layer_config[i].coupled_substream_count);
        ia_logd("loudness : 0x%04x", meta->ch_audio_layer_config[i].loudness & U16_MASK);
        if (meta->ch_audio_layer_config[i].output_gain_is_present_flag) {
            meta->ch_audio_layer_config[i].output_gain_flags = bs_getbits(&bs, 6);
            bs_getbits(&bs, 2);
            meta->ch_audio_layer_config[i].output_gain = bs_getbits(&bs, 16);
        }
    }

    ia_logt("static meta parsed.");
    return ec;
}


void ia_static_meta_uninit(IAStaticMeta *meta)
{
    if (meta->ambisonics_mode == 1 &&
            meta->ambix_layer_config.channel_mapping)
        free (meta->ambix_layer_config.channel_mapping);
    else if (meta->ambisonics_mode == 2 &&
            meta->ambix_layer_config.demixing_matrix)
        free (meta->ambix_layer_config.demixing_matrix);
}

/**
 *  recon_gain_info_obu() {
 *      for (i=0; i< channel_audio_layer; i++) {
 *          if (recon_gain_is_present_flag(i) == 1) {
 *              recon_gain_flags(i)                     leb128()
 *              for (j=0; j<n(i); j++) {
 *                  if (recon_gain_flag(i)(j) == 1)
 *                      recon_gain                      f(8)
 *              }
 *          }
 *      }
 *  }
 * */
int ia_recon_gain_info_parse (IAReconGainInfoList *list,
                              uint8_t *data, uint32_t len)
{
    uint64_t value = 0;
    int ret = 0;
    int bytes = 0;
    uint32_t pos = 0;

    while (pos < len) {
        bytes = leb128_read (data + pos, len - pos, &value);
        ia_logt("ia_recon_gain_info_parse : flags %lx", value);
        pos += bytes;
        if (bytes) {
            list->recon_gain_info[ret].flags = (uint16_t)value;
            list->recon_gain_info[ret].channels = bit1_count(value);
            memcpy(list->recon_gain_info[ret].recon_gain, data + pos,
                    list->recon_gain_info[ret].channels);
            pos += list->recon_gain_info[ret].channels;
            ++ret;
        }
    }
    list->count = ret;

    return ret;
}

/**
 *  demixing_info_obu() {
 *      dmixp_mode      f(3)
 *      reserved        f(5)
 *  }
 * */
int ia_demixing_info_parse (uint8_t *data, uint32_t len)
{
    ia_logd ("demixing mode raw : 0x%02x", *data);
    return *data >> 5 & 0x7;
}

