#ifndef _IMMERSIVE_AUDIO_METADATA_H_
#define _IMMERSIVE_AUDIO_METADATA_H_

#include <stdint.h>

#include "immersive_audio.h"
#include "immersive_audio_types.h"

typedef struct IAStaticMeta {

    uint16_t ambisonics_mode;
    uint16_t channel_audio_layer;

    struct {
        uint8_t output_channel_count;
        uint8_t substream_count;
        uint8_t coupled_substream_count;
        union {
            uint8_t *channel_mapping;
            uint16_t *demixing_matrix;
        };
    } ambix_layer_config;

    struct {
        uint8_t loudspeaker_layout;
        uint8_t output_gain_is_present_flag;
        uint8_t recon_gain_is_present_flag;
        uint8_t substream_count;
        uint8_t coupled_substream_count;
        int16_t loudness;
        uint8_t output_gain_flags;
        int16_t output_gain;
    } ch_audio_layer_config[IA_CH_LAYOUT_TYPE_COUNT];

} IAStaticMeta;

typedef struct IAReconGainInfo {
    uint16_t    flags;
    uint16_t    channels;
    uint8_t     recon_gain[IA_CH_RE_COUNT];
} IAReconGainInfo;

typedef struct IAReconGainInfoList {
    int count;
    IAReconGainInfo recon_gain_info[IA_CH_LAYOUT_TYPE_COUNT];
} IAReconGainInfoList;

IAErrCode ia_static_meta_parse (IAStaticMeta *, uint8_t *, uint32_t );
void ia_static_meta_uninit (IAStaticMeta *);

int ia_recon_gain_info_parse (IAReconGainInfoList *, uint8_t *, uint32_t);
int ia_demixing_info_parse (uint8_t *, uint32_t);

#endif /* _IMMERSIVE_AUDIO_METADATA_H_ */
