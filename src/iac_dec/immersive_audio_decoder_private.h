#ifndef IMMERSIVE_AUDIO_DECODER_PRIVATE_H
#define IMMERSIVE_AUDIO_DECODER_PRIVATE_H

#include <stdint.h>

#include "audio_effect_peak_limiter.h"
#include "demixer.h"
#include "immersive_audio_types.h"
#include "immersive_audio_metadata.h"

#define DEC_BUF_CNT 2

struct IAParam {
    IAParamID   id;
    union {
        struct {
            uint8_t    *data;
            uint32_t    size;
        } raw;
    };
};


typedef struct IAOutputGainInfo {
    uint8_t gain_flags;
    float   gain;
} IAOutputGainInfo;


typedef struct IAReconGainInfo2 {
    uint16_t    flags;
    uint16_t    channels;
    float       recon_gain[IA_CH_RE_COUNT];
    IAChannel   order[IA_CH_RE_COUNT];
} IAReconGainInfo2;


typedef struct IABuffer {
    uint8_t     *data;
    uint32_t    len;
} IABuffer;


typedef struct IAAmbisonicsLayerInfo {
    uint8_t     channels;
    uint8_t     streams;
    uint8_t     coupled_streams;
    void       *matrix;
} IAAmbisonicsLayerInfo;


typedef struct IAChannelAudioLayerInfo {

    uint8_t             layout;
    uint8_t             streams;
    uint8_t             coupled_streams;
    float               loudness;
    IAOutputGainInfo   *output_gain_info;

    IABuffer           *buffers;
    IAReconGainInfo2   *recon_gain_info;

} IAChannelAudioLayerInfo;


typedef struct IADecoderContext {

    int                     layer;
    IAChannelLayoutType     layout;
    int                     layout_channels;
    uint32_t                layout_flags;

    uint32_t                frame_size;
    uint32_t                delay;

    int                     layers;
    int                     ambix;

    int                     channels;
    int                     streams;
    int                     coupled_streams;
    IAChannel               channels_order[IA_CH_LAYOUT_MAX_CHANNELS];

    IAChannelAudioLayerInfo    *layer_info;
    IAAmbisonicsLayerInfo      *ambix_info;

    uint32_t                flags;
    int                     dmx_mode;

    Queue                   *q_recon;
    Queue                   *q_dmx_mode;

} IADecoderContext;


struct IADecoder {
    IADecoderContext   *dctx;

    /* decoder for channel audio layer. */
    IACoreDecoder      *ldec[IA_CH_LAYOUT_TYPE_COUNT];

    /* decoder for ambisonics */
    IACoreDecoder      *adec;

    uint8_t*        buffer[DEC_BUF_CNT];

    Demixer*                demixer;
    AudioEffectPeakLimiter  limiter;
};

#endif /* IMMERSIVE_AUDIO_DECODER_PRIVATE_H */
