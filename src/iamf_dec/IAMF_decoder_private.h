#ifndef IAMF_DECODER_PRIVATE_H
#define IAMF_DECODER_PRIVATE_H

#include <stdint.h>
#include "IAMF_core_decoder.h"
#include "IAMF_defines.h"
#include "IAMF_types.h"
#include "IAMF_OBU.h"
#include "ae_rdr.h"
#include "audio_effect_peak_limiter.h"
#include "demixer.h"

#define IAMF_FLAGS_MAGIC_CODE 0x01
#define IAMF_FLAGS_CONFIG     0x02
#define IAMF_FLAGS_RECONFIG   0x04

#define DEC_BUF_CNT 3


typedef enum {
    IA_CH_GAIN_RTF,
    IA_CH_GAIN_LTF,
    IA_CH_GAIN_RS,
    IA_CH_GAIN_LS,
    IA_CH_GAIN_R,
    IA_CH_GAIN_L,
    IA_CH_GAIN_COUNT
} IAOutputGainChannel;


/* >>>>>>>>>>>>>>>>>> DATABASE >>>>>>>>>>>>>>>>>> */

typedef void (*IAMF_Free)(void *);

typedef struct ObjectSet {
    void  **items;
    int     count;
    int     capacity;
    IAMF_Free    objFree;
} ObjectSet;

typedef struct ParameterItem {
    uint64_t        id;
    uint64_t        type;
    uint64_t        timestamp;

    void           *parent;
    IAMF_Parameter *parameter;

    union {
        float       mixGain;
    } defaultValue;
} ParameterItem;

typedef struct ElementItem {
    uint64_t        timestamp;
    uint64_t        id;

    IAMF_CodecConf *codecConf;
    IAMF_Element   *element;

    ParameterItem  *demixing;
    ParameterItem  *reconGain;
    ParameterItem  *mixGain;

    uint32_t        recon_gain_flags;
} ElementItem;

typedef struct SyncItem {
    uint64_t        id;
    uint64_t        start;
    int             type;
} SyncItem;

typedef struct ElementViewer {
    ElementItem    *items;
    int             count;
} ElementViewer;

typedef struct ParameterViewer {
    ParameterItem **items;
    int             count;
} ParameterViewer;

typedef struct Viewer {
    void           *items;
    int             count;
} Viewer;

typedef struct IAMF_DataBase {
    uint64_t        globalTime;

    IAMF_Object    *version;
    IAMF_Object    *sync;

    ObjectSet      *codecConf;
    ObjectSet      *element;
    ObjectSet      *mixPresentation;
    ObjectSet      *parameters; // composed of ParameterObject.

    ElementViewer       eViewer;
    ParameterViewer     pViewer;
    Viewer             *syncViewer;
} IAMF_DataBase;

/* <<<<<<<<<<<<<<<<<< DATABASE <<<<<<<<<<<<<<<<<< */

typedef struct LayoutInfo {
    IAMF_SP_LAYOUT  sp;
    IAMF_Layout     layout;
    int             channels;
} LayoutInfo;

typedef struct IAMF_OutputGain {
    uint32_t    flags;
    float       gain;
} IAMF_OutputGain;

typedef struct IAMF_ReconGain {
    uint16_t    flags;
    uint16_t    nb_channels;
    float       recon_gain[IA_CH_RE_COUNT];
    IAChannel   order[IA_CH_RE_COUNT];
} IAMF_ReconGain;

typedef struct SubLayerConf {
    uint8_t     layout;
    uint8_t     nb_channels;
    uint8_t     nb_substreams;
    uint8_t     nb_coupled_substreams;
    float       loudness;
    IAMF_OutputGain    *output_gain;
    IAMF_ReconGain     *recon_gain;
} SubLayerConf;

typedef struct ChannelLayerContext {
    int             nb_layers;
    SubLayerConf   *conf_s;

    int             layer;
    int             layout;
    int             channels;
    IAChannel       channels_order[IA_CH_LAYOUT_MAX_CHANNELS];

    int             dmx_mode;
    int             recon_gain_flags;
} ChannelLayerContext;

typedef struct AmbisonicsContext {
    int         mode;
    uint8_t    *mapping;
    int         mapping_size;
} AmbisonicsContext;

typedef struct IAMF_Stream {
    uint64_t    element_id;
    uint64_t    codecConf_id;
    IACodecID   codec_id;

    uint8_t     scheme; // audio element type: 0, CHANNEL_BASED; 1, SCENE_BASED

    int32_t     nb_channels;
    int32_t     nb_substreams;
    int32_t     nb_coupled_substreams;

    LayoutInfo *final_layout;
    void       *priv;

    uint64_t    timestamp;      // sync time
    uint64_t    duration;
    uint64_t    pts;            // external time
    uint64_t    dts;
} IAMF_Stream;

typedef struct ScalableChannelDecoder {
    int             nb_layers;
    IACoreDecoder **sub_decoders;
    Demixer        *demixer;
} ScalableChannelDecoder;

typedef struct AmbisonicsDecoder {
    IACoreDecoder  *decoder;
} AmbisonicsDecoder;

typedef struct IAMF_StreamDecoder {
    union {
        ScalableChannelDecoder  *scale;
        AmbisonicsDecoder       *ambisonics;
    };

    IAMF_Stream    *stream;
    float          *buffers[DEC_BUF_CNT];

    uint8_t   **packets;
    uint32_t   *sizes;
    uint32_t    count;

    uint32_t    frame_size;

} IAMF_StreamDecoder;


typedef struct IAMF_Mixer {
    uint64_t   *element_ids;
    int         nb_elements;
    int         channels;
    int         samples;
    float     **frames;
    int         count;
    int         enable_mix;
} IAMF_Mixer;

typedef struct IAMF_Presentation {
    IAMF_MixPresentation   *obj;

    IAMF_Stream           **streams;
    uint32_t                nb_streams;
    IAMF_StreamDecoder    **decoders;
    IAMF_StreamDecoder     *prepared_decoder;

    IAMF_Mixer              mixer;
} IAMF_Presentation;

typedef struct IAMF_DecoderContext {
    IAMF_DataBase   db;
    uint32_t        flags;

    LayoutInfo         *output_layout;
    int             output_samples;

    IAMF_Presentation  *presentation;
    char               *mix_presentation_label;

    AudioEffectPeakLimiter  limiter;


    // PTS
    uint32_t    duration;
    uint32_t    duration_time_base;
    uint32_t    pts;
    uint32_t    pts_time_base;
    uint32_t    last_frame_size;

    IAMF_extradata  metadata;

} IAMF_DecoderContext;


struct IAMF_Decoder {
    IAMF_DecoderContext     ctx;
};

#endif /* IAMF_DECODER_PRIVATE_H */

