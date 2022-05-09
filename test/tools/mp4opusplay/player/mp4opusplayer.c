#include<stdio.h>
#include<math.h>

#include "mp4opuspar.h"
#include "opus_header.h"
#include "dmemory.h"
#include "string.h"

#include "immersive_audio_decoder.h"
#include "wavwriter.h"

#define PKT_SIZE 2048*12
static void dump_opus_header(OpusHeader *, int);
static void dump_meta(OpusHeader *);
static void dump_audio(OpusHeader *);
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
    fprintf(stderr, "-o4          : -o4(decode CMF4 opus bitstream, audio processing and output wave file).\n");
    fprintf(stderr, "-l[1-8]      : layout(1:2.0, 2:5.1, 3:5.1.2, 4:5.1.4, 5:7.1, 6:7.1.2, 7:7.1.4, 8:3.1.2.\n");
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
    } else if(output_mode == 4) {
        mp4_input_wav_output(argv[args], layout);
    } else {
        print_usage(argv);
        fprintf(stderr, "invalid output mode %d\n", output_mode);
    }

    return 0;
}

void mp4_input_data_dump (char *opusmeta_mp4_fn)
{
    MP4OpusParser mp4par;
    OpusHeader opusheader[2];
    int num_a_trak;
    int done = 0;
    char pkt_buf[PKT_SIZE];

    FILE *dump_f;
    char dump_fn[256] = {0};
    char prefix[256] = {0};

    int pkt_len;
    int64_t sample_offs;
    int entno;

    OpusHeader *header = opusheader;

    char *ptr;
    ptr = strrchr(opusmeta_mp4_fn, '.');
    if (ptr == 0) {
        strncpy(prefix, opusmeta_mp4_fn, strlen(opusmeta_mp4_fn));
    } else {
        strncpy(prefix, opusmeta_mp4_fn, ptr - opusmeta_mp4_fn);
    }

    snprintf(dump_fn, 256, "%s.log", prefix);

    dump_f = fopen(dump_fn, "wb+");
    if (!dump_f) {
        fprintf(stderr, "log file %s can not open.\n", dump_fn);
        goto err_done;
    }

    mp4_opus_parser_init(&mp4par);
    mp4_opus_parser_set_logger(&mp4par, dump_f);
    num_a_trak = mp4_opus_parser_open_trak(&mp4par, opusmeta_mp4_fn,
            opusheader);

    if (num_a_trak <= 0) {
        fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", opusmeta_mp4_fn);
        goto err_done;
    }

    if (opusheader[0].media_type == TRAK_TYPE_METADATA) {
        header = &opusheader[1];
    }

    dump_opus_header(opusheader, num_a_trak);

    while (!done) {
        for (int i=0; i<num_a_trak; ++i) {
            if (mp4_opus_parser_read_packet(&mp4par, i, (void *)pkt_buf,
                        PKT_SIZE, &pkt_len, &sample_offs, &entno) < 0) {
                done = 1;
                break;
            }

            if (header->dents > 0) {
                /* fprintf(stderr, "track id %d: len %4d, offset %8ld, entno %4d, demix mode %d\n", */
                    /* i, pkt_len, sample_offs, entno, header->demix_modes[entno-1]); */
                fprintf(dump_f, "track id %d: len %4d, offset %8ld, entno %4d, demix mode %d\n",
                    i, pkt_len, sample_offs, entno, header->demix_modes[entno-1]);
            } else {
                /* fprintf(dump_f, "track id %d: len %4d, offset %8ld, entno %4d\n", */
                        /* i, pkt_len, sample_offs, entno); */
                /* fprintf(stderr, "track id %d: len %4d, offset %8ld, entno %4d", */
                        /* i, pkt_len, sample_offs, entno); */
            }
        }
    }

err_done:
    mp4_opus_parser_close(&mp4par);
    if (dump_f) {
        fclose(dump_f);
    }
}

static uint8_t wav_layout_map[channel_layout_type_count][12] = {
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


#define MAX_CG 4
void mp4_input_wav_output (char *opusmeta_mp4_fn, int flags)
{
    MP4OpusParser mp4par;
    OpusHeader opusheader[2];
    int num_a_trak;
    FILE *dump_f;
    char dump_fn[256] = {0};
    int done = 0;
    mp4r_t *mp4r;
    FILE *wav_f = 0;
    char wav_fn[256] = {0};
    char *ptr;
    char prefix[256] = {0};
    unsigned char pkt_buf[PKT_SIZE];
    int ret = 0;

    IADecoder *dec = 0;

    int pkt_len;
    int64_t sample_offs;
    int entno;

    int channels = 0;
    int streams[MAX_CG] = {0};
    int coupled_streams[MAX_CG] = {0};
    int layout[MAX_CG] = {0};
    int gain[MAX_CG] = {0};
    int loudness[MAX_CG] = {0};
    int nb_frames;
    int tlayout = flags;
    int layouts[channel_layout_type_count];

    OpusHeader *header = opusheader;

    ptr = strrchr(opusmeta_mp4_fn, '.');
    if (ptr == 0) {
        strncpy(prefix, opusmeta_mp4_fn, strlen(opusmeta_mp4_fn));
    } else {
        strncpy(prefix, opusmeta_mp4_fn, ptr - opusmeta_mp4_fn);
    }

    snprintf(dump_fn, 256, "%s.log", prefix);

    dump_f = fopen(dump_fn, "wb+");
    if (!dump_f) {
        fprintf(stderr, "log file %s can not open.\n", dump_fn);
        goto err_done;
    }

    mp4_opus_parser_init(&mp4par);
    mp4_opus_parser_set_logger(&mp4par, dump_f);
    num_a_trak = mp4_opus_parser_open_trak(&mp4par, opusmeta_mp4_fn,
            opusheader);

    if (num_a_trak <= 0) {
        fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", opusmeta_mp4_fn);
        goto err_done;
    }


    if (opusheader[0].media_type == TRAK_TYPE_METADATA) {
        header = &opusheader[1];
    }


    channels = header->channels;

    nb_frames = channels * 960;
    int16_t *pcm = 0;
    int16_t *wavpcm = 0;
    int16_t *out = 0;

    dec = immersive_audio_decoder_create(header->input_sample_rate,
            channels, header->metadata, header->len, 1, &ret);
    if (!dec) {
        fprintf(stderr, "fail to make OpusMSDecoder.errno %d \n", ret);
        goto err_done;
    }

    pcm = (int16_t *) malloc (sizeof (int16_t) * nb_frames);
    wavpcm = (int16_t *) malloc (sizeof (int16_t) * nb_frames);
    if (!pcm || !wavpcm) {
        fprintf(stderr, "fail to alloc memory.\n");
        goto err_done;
    }

    ret = immersive_audio_get_valid_layouts(dec, layouts);
    if (ret > 0) {
        if (tlayout != -1) {
            int i = 0;
            for (; i<ret; ++i)
                if (tlayout == layouts[i])
                    break;
            if (ret == i) {
                fprintf(stderr, "the layout %d is not included in the stream.\n", tlayout);
                fprintf(stderr, "%s supports", opusmeta_mp4_fn);
                for (i=0; i<ret; ++i) {
                    fprintf(stderr, " %s(%d)", layout_str[layouts[i]], layouts[i]);
                }
                fprintf (stderr, ".\n");
                goto err_done;
            } else {
                ret = immersive_audio_set_layout(dec, tlayout);
                channels = channel_layout_get_channel_count(tlayout);
            }
        } else {
            tlayout = layouts[ret - 1];
        }
    } else {
        fprintf(stderr, "can not get valid layout in the stream.");
        goto err_done;
    }

    snprintf(wav_fn, 256, "%s_%s.wav", prefix, layout_str[tlayout]);
    fprintf(stderr, "Sample rate: %u, channles: %d, layout %d.\n",
            header->input_sample_rate, channels, tlayout);
    wav_f = (FILE *)wav_write_open(wav_fn, header->input_sample_rate, 16, channels);
    fprintf(stderr, "Decoding ...\n");
    while (!done) {
        for (int i=0; i<num_a_trak; ++i) {
            if (mp4_opus_parser_read_packet(&mp4par, i, (void *)pkt_buf,
                        PKT_SIZE, &pkt_len, &sample_offs, &entno) < 0) {
                done = 1;
                break;
            } else {
                /* fprintf(stderr, */
                        /* "track id %d: len %4d, offset %8ld, entno %4d, demix mode %d\n", */
                        /* i, pkt_len, sample_offs, entno, */
                        /* header->demix_modes[entno-1]); */
                /* memset(pcm, 0, sizeof (int16_t) * nb_frames); */
                ret = immersive_audio_decode (dec, pkt_buf, pkt_len, pcm, 960, 0, header->demix_modes[entno-1]);
                if (ret > 0 && wav_f) {
                    out = pcm;
                    if (tlayout > channel_layout_type_2_0_0) {
                        wav_layout_out(pcm, wavpcm, ret, channels, wav_layout_map[tlayout]);
                        out = wavpcm;
                    }
                    wav_write_data(wav_f, (unsigned char *)out, sizeof (int16_t) * 960 * channels);
                }
            }
        }
    }


err_done:

    if (pcm) {
        free(pcm);
    }
    if (wavpcm) {
        free(wavpcm);
    }
    if (dec) {
        immersive_audio_decoder_destroy(dec);
    }
    if (wav_f) {
        wav_write_close(wav_f);
    }
    mp4_opus_parser_close(&mp4par);
    if (dump_f) {
        fclose(dump_f);
    }
}

void dump_meta (OpusHeader *header)
{
    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "META track\n");
    fprintf(stderr, "Codec ID   :\t\t%u(%s)\n", header->codec_id,
            header->codec_id == 0 ? "opus":"unknow");
    fprintf(stderr, "Profile    :\t\t%d\n", header->profile);
    fprintf(stderr, "Ambisonics mode:\t%d\n", header->meta.ambisonics_mode);
    fprintf(stderr, "Channel audio  :\t%d\n", header->meta.channel_audio);
    fprintf(stderr, "Sub-sample count   :\t%d\n", header->meta.sub_sample_count);
}

inline static float u82f(uint8_t uc, int frac)
{
    return ((float)uc / (pow(2.0f, (float)frac) - 1.0));
}

inline static float i162f(short s, int frac)
{
    return ((float)s) * powf(2.0f, (float)-frac);
}

void dump_audio (OpusHeader *header)
{
    int layout;
    int loudness;
    uint8_t gain;
    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "AUDIO track\n");
    fprintf(stderr, "Channels   :\t\t%u\n", header->channels);
    fprintf(stderr, "Preskip    :\t\t%d\n", header->preskip);
    fprintf(stderr, "Input sample rate  :\t%u\n", header->input_sample_rate);
    fprintf(stderr, "Gain       :\t\t%d\n", header->gain);
    fprintf(stderr, "Channel mapping:\t%d\n", header->channel_mapping);
    if (header->channel_mapping != 4 &&
            header->channel_mapping != 1) {
        fprintf(stderr, "streams    :\t\t%d\n", header->nb_streams);
        fprintf(stderr, "coupled    :\t\t%d\n", header->nb_coupled);
        fprintf(stderr, "stream map :\t\t[");
        for (int i=0; i<header->channels; ++i) {
            fprintf(stderr, " %d", header->stream_map[i]);
        }
        fprintf(stderr, " ]\n");
    }
    fprintf(stderr, "Ambisonics mode:\t%d\n", header->meta.ambisonics_mode);
    fprintf(stderr, "Channel audio  :\t%d\n", header->meta.channel_audio);
    fprintf(stderr, "Sub-sample count   :\t%d\n", header->meta.sub_sample_count);
    fprintf(stderr, "ALC number     :\t\t%d\n", header->meta.alc_cnt);
    for (int i=0; i<header->meta.alc_cnt; ++i) {
        layout = ((AudioLayerConfig *)header->meta.alc)[i].loudspeaker_layout;
        loudness = ((AudioLayerConfig *)header->meta.alc)[i].loudness;
        fprintf(stderr, "\nAudio Layer %d  :\n", i);
        fprintf(stderr, "Layout         :\t\t%s(%d)\n", layout_str[layout],
                layout);
        /* fprintf(stderr, "Dmix gain flag :\t\t%d\n", */
                /* ((AudioLayerConfig *)header->meta.alc)[i].dmix_gain_flag); */
        fprintf(stderr, "streams        :\t\t%d\n",
                ((AudioLayerConfig *)header->meta.alc)[i].stream_count);
        fprintf(stderr, "coupled        :\t\t%d\n",
                ((AudioLayerConfig *)header->meta.alc)[i].coupled_stream_count);

        fprintf(stderr, "Loundness      :\t\t%d(%.2fdB)\n", loudness,
                i162f(loudness, 8));
        if (((AudioLayerConfig *)header->meta.alc)->dmix_gain_flag) {
            gain = ((AudioLayerConfig *)header->meta.alc)[i].dmix_gain;
            fprintf(stderr, "Dmix gain      :\t\t%d(%.2f)\n", gain,
                    u82f(gain, 8));
        }
    }
}


void dump_opus_header(OpusHeader *header, int num)
{

    fprintf(stderr, "\n\nStream Info:\n");
    for (int i=0; i<num; ++i) {
        if (header[i].media_type == TRAK_TYPE_METADATA) {
            dump_meta(header);
        } else if (header[i].media_type == TRAK_TYPE_AUDIO) {
            dump_audio(&header[i]);
        } else {
            fprintf(stderr, "\n\nmedia type : %d \n", header[i].media_type);
        }
    }
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
