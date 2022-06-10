#ifndef IAC_HEADER_H
#define IAC_HEADER_H

#include <stdint.h>

#define OPUS_DEMIXING_MATRIX_SIZE_MAX (18 * 18 * 2)

enum {
    fL = 0x1,
    fR = 0x2,
    fC = 0x4,
    LFE = 0x8,
    bL = 0x10,
    bR = 0x20,
    bC = 0x100,
    sL = 0x200,
    sR = 0x400,
    // SRopus
    tfL = 0x1000,
    tfR = 0x2000,
    tbL = 0x4000,
    tbR = 0x8000
    // SRopus
};

typedef struct {
    int loudspeaker_layout;
    int dmix_gain_flag;
    int recon_gain_flag;
    int stream_count;
    int coupled_stream_count;
    int loudness;
    int dmix_gain;
} AudioLayerConfig;


typedef struct {
    int magic_id;
    int version;
    int channels; /* Number of channels: 1..255 */
    int preskip;
    uint32_t input_sample_rate;
    int gain; /* in dB S7.8 should be zero whenever possible */
    int channel_mapping;
    /* The rest is only used if channel_mapping != 0 */
    int nb_streams;
    int nb_coupled;
    int cmf;
    int nb_output_channels;
    unsigned char stream_map[255];
    unsigned char
    dmatrix[OPUS_DEMIXING_MATRIX_SIZE_MAX]; // 2 channel demixing matrix

} OpusHeader;

typedef struct {
    // MP4DecoderConfigDescriptor
    int         object_type_indication;
    int         stream_type;
    int         upstream;

    // MP4DecSpecificInfoDescriptor
    int         audio_object_type;
    int         sample_rate;
    int         channels;

    // GASpecificConfig
    int         frame_length_flag;
    int         depends_on_core_coder;
    int         extension_flag;
} AACHeader;

typedef struct {
    // aiac box
    int         version;
    int         ambix;
    int         layers;
    int         ambix_chs;
    int         layout[9];
    uint8_t    *codec_config;
    int         clen;

    // demixing mode
    int         dents;
    uint8_t    *demix_modes;

    uint8_t   **demix_entries;
    int        *entry_len;
    int         entry_count;

    // static meta data
    uint8_t    *metadata;
    int         mlen;

    // codce info

    int codec_id;
    union {
       OpusHeader opus;
       AACHeader aac;
    };
} IACHeader;

#ifdef __cplusplus
extern "C" {
#endif
int iac_header_parse_codec_spec(IACHeader *h);
#ifdef __cplusplus
}
#endif
extern const int wav_permute_matrix[8][8];

#endif
