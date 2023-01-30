/**
 * @file IAMF_decoder.h
 * @brief Immersive audio decoder reference API
 */

#ifndef IAMF_DECODER_H
#define IAMF_DECODER_H

#include <stdint.h>
#include "IAMF_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@}*/
/**\name Immersive audio decoder functions */
/**@{*/

typedef enum IAMF_SP_Label {
    SP_LABEL_Mp000 = 0,
    SP_LABEL_Mp022, SP_LABEL_Mn022,
    SP_LABEL_MpSC, SP_LABEL_MnSC,
    SP_LABEL_Mp030, SP_LABEL_Mn030,
    SP_LABEL_Mp045, SP_LABEL_Mn045,
    SP_LABEL_Mp060, SP_LABEL_Mn060,
    SP_LABEL_Mp090, SP_LABEL_Mn090,
    SP_LABEL_Mp110, SP_LABEL_Mn110,
    SP_LABEL_Mp135, SP_LABEL_Mn135,
    SP_LABEL_Mp180,

    SP_LABEL_Up000 = 18,
    SP_LABEL_Up022, SP_LABEL_Un022,
    SP_LABEL_Up030, SP_LABEL_Un030,
    SP_LABEL_Up045, SP_LABEL_Un045,
    SP_LABEL_Up060, SP_LABEL_Un060,
    SP_LABEL_Up090, SP_LABEL_Un090,
    SP_LABEL_Up110, SP_LABEL_Un110,
    SP_LABEL_Up135, SP_LABEL_Un135,
    SP_LABEL_Up180,
    SP_LABEL_UHp180,

    SP_LABEL_Tp000 = 35,

    SP_LABEL_Bp000 = 36,
    SP_LABEL_Bp022, SP_LABEL_Bn022,
    SP_LABEL_Bp030, SP_LABEL_Bn030,
    SP_LABEL_Bp045, SP_LABEL_Bn045,
    SP_LABEL_Bp060, SP_LABEL_Bn060,
    SP_LABEL_Bp090, SP_LABEL_Bn090,
    SP_LABEL_Bp110, SP_LABEL_Bn110,
    SP_LABEL_Bp135, SP_LABEL_Bn135,
    SP_LABEL_Bp180,

    SP_LABEL_LFE1 = 52,
    SP_LABEL_LFE2 = 53,
} IAMF_SP_Label;


typedef struct IAMF_Decoder *IAMF_DecoderHandle;
typedef struct {
    int channles;
    int sample_rate;
} IAMF_ConfInfo;

typedef struct {
    int     count;
    char**  labels;
} IAMF_Labels;

typedef struct {
    uint8_t    *data;
    uint32_t    size;
    uint64_t    pts;
    uint64_t    dts;
} IAMF_IPacket;

typedef struct {
    void       *pcm;
    uint32_t    frame_size;
    int         channels;
    uint64_t    pts;
} IAMF_OFrame;

typedef struct IAMF_Param {
    int         parameter_length;
    uint32_t    parameter_definition_type;
    union {
        uint32_t    dmixp_mode;
    };
} IAMF_Param;

typedef struct IAMF_extradata {
    IAMF_Layout     target_layout;
    uint32_t        number_of_samples;
    uint32_t        bitdepth;
    uint32_t        sampling_rate;

    int                 num_loudness_layouts;
    IAMF_Layout        *loudness_layout;
    IAMF_LoudnessInfo  *loudness;

    uint32_t        num_parameters;
    IAMF_Param     *param;
} IAMF_extradata;

IAMF_DecoderHandle  IAMF_decoder_open (void);
int     IAMF_decoder_close (IAMF_DecoderHandle handle);

int     IAMF_decoder_configure (IAMF_DecoderHandle handle, const uint8_t *data, uint32_t size, uint32_t *rsize);
int     IAMF_decoder_get_configuration (IAMF_DecoderHandle handle, IAMF_ConfInfo *info);
int     IAMF_decoder_decode (IAMF_DecoderHandle handle, const uint8_t *data, int32_t size, uint32_t *rsize, void *pcm);

// TODO
// int     IAMF_decoder_read_header (IAMF_DecoderHandle handle, IAMF_IPacket *packet);
// int     IAMF_decoder_decode_packet (IAMF_DecoderHandle handle, IAMF_IPacket *packet);
// int     IAMF_decoder_receive_frame (IAMF_DecoderHandle handle, IAMF_OFrame *frame);

IAMF_Labels*    IAMF_decoder_get_mix_presentation_labels (IAMF_DecoderHandle handle);
int     IAMF_decoder_output_layout_set_sound_system (IAMF_DecoderHandle handle, IAMF_SoundSystem ss);
// int     IAMF_decoder_output_layout_set_sp_labels (IAMF_DecoderHandle handle, int count, IAMF_SP_Label *labels);
int     IAMF_decoder_output_layout_set_binaural (IAMF_DecoderHandle handle);
int     IAMF_decoder_output_layout_set_mix_presentation_label (IAMF_DecoderHandle handle, const char *label);
int     IAMF_layout_sound_system_channels_count (IAMF_SoundSystem ss);
int     IAMF_layout_binaural_channels_count ();

int     IAMF_decoder_set_pts (IAMF_DecoderHandle handle, uint32_t pts, uint32_t time_base);
int     IAMF_decoder_get_last_metadata (IAMF_DecoderHandle handle, uint32_t *pts, IAMF_extradata *metadata);
#ifdef __cplusplus
}
#endif



#endif /* IAMF_DECODER_H */
