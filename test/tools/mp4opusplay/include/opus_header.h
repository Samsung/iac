#ifndef OPUS_HEADER_H
#define OPUS_HEADER_H

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

    uint32_t codec_id;
    uint32_t profile;
    uint32_t media_type;

    struct {
        int ambisonics_mode;
        int channel_audio;
        int sub_sample_count;
        void *alc; // audio layer configuration
        int alc_cnt;
    } meta;

    uint8_t *demix_entries;
    uint32_t entry_count;
    uint8_t *demix_modes;
    int dents;
    uint8_t *metadata;
    int len;
} OpusHeader;

#ifdef __cplusplus
extern "C" {
#endif
int opus_header_parse(const unsigned char *header, int len, OpusHeader *h);
#ifdef __cplusplus
}
#endif
extern const int wav_permute_matrix[8][8];

#endif
