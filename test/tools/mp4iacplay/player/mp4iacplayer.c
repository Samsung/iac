#include<stdio.h>
#include<math.h>

#include "mp4iacpar.h"
#include "iac_header.h"
#include "dmemory.h"
#include "string.h"
#include "atom.h"

#include "immersive_audio_decoder.h"
#include "wavwriter.h"

#define PKT_SIZE 2048*12


static void dump_iac_header(IACHeader *);
static void mp4_input_data_dump (char *);
static void mp4_input_wav_output (char *, int flags);
static void wav_layout_out(int16_t *in, int16_t *out, int size,
        int channels, uint8_t* mapping);

static void print_usage (char *argv[])
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s <options> <input/output file>\n", argv[0]);
    fprintf(stderr, "options:\n");
    fprintf(stderr, "-o1          : -o1(mp4 dump output)\n");
    fprintf(stderr, "-o2          : -o2(decode IAC bitstream, audio processing and output wave file).\n");
    fprintf(stderr, "-l[0-8]      : layout(0:mono, 1:stereo, 2:5.1, 3:5.1.2, 4:5.1.4, 5:7.1, 6:7.1.2, 7:7.1.4, 8:3.1.2.\n");
}

int main(int argc, char *argv[])
{
    int output_mode = -1;
    int layout = -1;
    int args;

    if (argc < 2) {
        print_usage(argv);
        return -1;
    }

    args = 1;
    while (args < argc-1) {
        if (argv[args][0] == '-') {
            if (argv[args][1] == 'o') {
                output_mode = atoi(argv[args] + 2);
            } else if (argv[args][1] == 'l') {
                layout = atoi(argv[args] + 2);
            }
        }
        args++;
    }

    if (!strncmp(argv[args], "-h", 2)) {
        print_usage(argv);
        return 0;
    }

    if (output_mode == 1) {
        mp4_input_data_dump(argv[args]);
    } else if(output_mode == 2) {
        mp4_input_wav_output(argv[args], layout);
    } else {
        print_usage(argv);
        fprintf(stderr, "invalid output mode %d\n", output_mode);
    }

    return 0;
}

void mp4_input_data_dump (char *fn)
{
    MP4IACParser mp4par;
    int ret;
    char pkt_buf[PKT_SIZE];

    FILE *dump_f;
    char dump_fn[256] = {0};

    int pkt_len;
    int64_t sample_offs;
    int entno;

    IACHeader *header;

    char *ptr;
    ptr = strrchr(fn, '.');
    if (ptr == 0) {
        strncpy(dump_fn, fn, strlen(fn));
    } else {
        strncpy(dump_fn, fn, ptr - fn);
    }
    strcpy(dump_fn+strlen(dump_fn), ".log");

    dump_f = fopen(dump_fn, "wb+");
    if (!dump_f) {
        fprintf(stderr, "log file %s can not open.\n", dump_fn);
        goto err_done;
    }

    mp4_iac_parser_init(&mp4par);
    mp4_iac_parser_set_logger(&mp4par, dump_f);
    ret = mp4_iac_parser_open_trak(&mp4par, fn, &header);

    if (ret <= 0) {
        fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", fn);
        goto err_done;
    }

    dump_iac_header(header);

    while (1) {
        if (mp4_iac_parser_read_packet(&mp4par, 0, (void *)pkt_buf,
                    PKT_SIZE, &pkt_len, &sample_offs, &entno) < 0) {
            break;
        }

        if (header->dents > 0) {
            /* fprintf(stderr, "track id %d: len %4d, offset %8ld, entno %4d\n", */
                /* 0, pkt_len, sample_offs, entno); */
            /* fprintf(dump_f, "track id %d: len %4d, offset %8ld, entno %4d\n", */
                /* 0, pkt_len, sample_offs, entno); */
        } else {
            /* fprintf(dump_f, "track id %d: len %4d, offset %8ld, entno %4d\n", */
                    /* i, pkt_len, sample_offs, entno); */
            /* fprintf(stderr, "track id %d: len %4d, offset %8ld, entno %4d", */
                    /* i, pkt_len, sample_offs, entno); */
        }
    }

err_done:
    mp4_iac_parser_close(&mp4par);
    if (dump_f) {
        fclose(dump_f);
    }
}

static uint8_t wav_layout_map[IA_CHANNEL_LAYOUT_COUNT][12] = {
    {0},
    {0, 1},
    {0, 2, 1, 5, 3, 4},
    {0, 2, 1, 7, 3, 4, 5, 6},
    {0, 2, 1, 9, 3, 4, 5, 6, 7, 8},
    {0, 2, 1, 7, 3, 4, 5, 6},
    {0, 2, 1, 9, 3, 4, 5, 6, 7, 8},
    {0, 2, 1, 11, 3, 4, 5, 6, 7, 8, 9, 10},
    {0, 2, 1, 5, 3, 4}
};

static const char* layout_str[] = {
    "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4",
    "3.1.2"
};


void mp4_input_wav_output (char *fn, int flags)
{
    MP4IACParser mp4par;
    FILE *dump_f;
    char dump_fn[256] = {0};
    mp4r_t *mp4r;
    FILE *wav_f = 0;
    char wav_fn[256] = {0};
    char *ptr;
    char prefix[256] = {0};
    unsigned char pkt_buf[PKT_SIZE];
    int ret = 0;

    IADecoder *dec = 0;
    IACodecID cid = IA_CODEC_OPUS;
    int layout = -1;

    int pkt_len;
    int64_t sample_offs;
    int entno;

    int channels = 0;
    IACHeader *header = 0;
    int idx = 0;

    int16_t *pcm = 0;
    int16_t *wavpcm = 0;
    uint32_t nb_frames;

    ptr = strrchr(fn, '.');
    if (ptr == 0) {
        strncpy(prefix, fn, strlen(fn));
    } else {
        strncpy(prefix, fn, ptr - fn);
    }

    snprintf(dump_fn, 256, "%s.log", prefix);

    dump_f = fopen(dump_fn, "wb+");
    if (!dump_f) {
        fprintf(stderr, "log file %s can not open.\n", dump_fn);
        goto err_done;
    }

    mp4_iac_parser_init(&mp4par);
    mp4_iac_parser_set_logger(&mp4par, dump_f);
    ret = mp4_iac_parser_open_trak(&mp4par, fn, &header);

    if (ret <= 0) {
        fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", fn);
        goto err_done;
    }

    if (header->codec_id == ATOM_TYPE_MP4A)
        cid = IA_CODEC_AAC;


    dec = immersive_audio_decoder_create(cid, header->codec_config, header->clen,
                                       header->metadata, header->mlen,
                                       IA_FLAG_CODEC_CONFIG_ISOBMFF);
    if (!dec) {
        fprintf(stderr, "fail to make IADecoder.\n");
        goto err_done;
    }

    if (flags < 0) {
        layout = header->layout[header->layers-1];
    } else {
        for (int i=0; i<header->layers; ++i) {
            if (header->layout[i] == flags) {
                layout = flags;
                break;
            }
        }

        if (layout < 0)
            goto err_done;
    }

    channels = immersive_audio_channel_layout_get_channels_count(layout);
    nb_frames = channels * 1024;

    pcm = (int16_t *) malloc (sizeof (int16_t) * nb_frames);
    wavpcm = (int16_t *) malloc (sizeof (int16_t) * nb_frames);
    if (!pcm || !wavpcm) {
        fprintf(stderr, "fail to alloc memory.\n");
        goto err_done;
    }

    immersive_audio_decoder_set_channel_layout(dec, layout);

    snprintf(wav_fn, 256, "%s_%s.wav", prefix, layout_str[layout]);
    fprintf(stderr, "Sample rate: %u, channles: %d, layout %d.\n",
            48000, channels, layout);
    wav_f = (FILE *)wav_write_open(wav_fn, 48000, 16, channels);
    fprintf(stderr, "Decoding ...\n");
    while (1) {
        if (mp4_iac_parser_read_packet(&mp4par, 0, (void *)pkt_buf,
                    PKT_SIZE, &pkt_len, &sample_offs, &entno) < 0) {
            break;
        } else {
            /* fprintf(stderr, "track id %d: len %4d, offset %8ld, entno %4d\n", */
                    /* 0, pkt_len, sample_offs, entno); */
            IAParam *param = 0;
            idx = header->demix_modes[entno-1];
            param = immersive_audio_param_raw_data_new (IA_PARAM_DEMIXING_INFO,
                    header->demix_entries[idx], header->entry_len[idx]);
            ret = immersive_audio_decoder_decode (dec, pkt_buf, pkt_len, pcm, 1024, &param, 1);
            if (ret > 0 && wav_f) {
                wav_layout_out(pcm, wavpcm, ret, channels, wav_layout_map[layout]);
                wav_write_data(wav_f, (unsigned char *)wavpcm, sizeof (int16_t) * ret * channels);
            }
            immersive_audio_param_free (param);
        }
    }

    fprintf(stderr, "Decoded.\n");

err_done:

    if (pcm) {
        free(pcm);
    }
    if (wavpcm) {
        free(wavpcm);
    }
    if (dec) {
        immersive_audio_decoder_destory(dec);
    }
    if (wav_f) {
        wav_write_close(wav_f);
    }
    mp4_iac_parser_close(&mp4par);
    if (dump_f) {
        fclose(dump_f);
    }
}


static void dump_iac_codec_spec (IACHeader *header)
{
    if (header->codec_id == ATOM_TYPE_OPUS) {
        OpusHeader *oh = &header->opus;
        fprintf(stderr, " > Codec                    :\topus\n");
        fprintf(stderr, " > version                  :\t%d\n", oh->version);
        fprintf(stderr, " > output channel count     :\t%d\n", oh->channels);
        fprintf(stderr, " > preskip                  :\t%d\n", oh->preskip);
        fprintf(stderr, " > input sample rate        :\t%d\n", oh->input_sample_rate);
        fprintf(stderr, " > output gain              :\t%d\n", oh->gain);
        fprintf(stderr, " > channel mapping family   :\t%d\n", oh->channel_mapping);
    } else if (header->codec_id == ATOM_TYPE_MP4A) {
        AACHeader *ah = &header->aac;
        fprintf(stderr, " > object type indication   :\t0x%x\n", ah->object_type_indication);
        fprintf(stderr, " > stream type              :\t0x%x\n", ah->stream_type);
        fprintf(stderr, " > upstream                 :\t%d\n", ah->upstream);

        fprintf(stderr, " > audio object type        :\t%d\n", ah->audio_object_type);
        fprintf(stderr, " > input sample rate        :\t%d\n", ah->sample_rate);
        fprintf(stderr, " > output channel count     :\t%d\n", ah->channels);

        fprintf(stderr, " > frame length flag        :\t%d\n", ah->frame_length_flag);
        fprintf(stderr, " > depends on core coder    :\t%d\n", ah->depends_on_core_coder);
        fprintf(stderr, " > extension flag           :\t%d\n", ah->extension_flag);
    }
}

void dump_iac_header(IACHeader *header)
{
    fprintf(stderr, "\n\nStream Info:\n");
    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "Ambisonics mode:\t%d\n", header->ambix);
    fprintf(stderr, "Channel audio  :\t%d\n", header->layers);
    fprintf(stderr, "Ambisonics channel count:\t%d\n", header->ambix_chs);
    fprintf(stderr, "loudspeaker layout:\t[ ");
    for (int i=0; i<header->layers; ++i) {
        fprintf(stderr, "%d(%s) ", header->layout[i], layout_str[header->layout[i]]);
    }
    fprintf(stderr, "]\n");
    fprintf(stderr, "sub-stream codec specific :\t\n");
    dump_iac_codec_spec (header);
    fprintf(stderr, "\n");
}

void wav_layout_out(int16_t *in, int16_t *out, int size, int channels,
        uint8_t* mapping)
{
    /* fprintf(stderr, "wav layout out: channels %d, frame size %d\n", */
            /* channels, size); */

    for (int i=0; i < size; ++i) {
        for (int s=0; s<channels; ++s)
            out[channels * i + s] = in[channels * i + mapping[s]];
    }
}
