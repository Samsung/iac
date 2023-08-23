/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file iamfplayer.c
 * @brief IAMF player.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "IAMF_decoder.h"
#include "dep_wavwriter.h"
#include "mp4iamfpar.h"
#include "string.h"

#ifndef SUPPORT_VERIFIER
#define SUPPORT_VERIFIER 0
#endif
#if SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#define FLAG_METADATA 0x1
#if SUPPORT_VERIFIER
#define FLAG_VLOG 0x2
#endif
#define FLAG_DISABLE_LIMITER 0x4
#define FLAG_TEST_SOUND_SYSTEM 0x100
#define SAMPLING_RATE 48000

typedef struct Layout {
  int type;
  union {
    IAMF_SoundSystem ss;
  };
} Layout;

typedef struct PlayerArgs {
  const char *path;
  Layout *layout;
  float peak;
  float loudness;
  uint32_t flags;
  uint32_t st;
  uint32_t rate;
  uint32_t bit_depth;
  uint64_t mix_presentation_id;
} PlayerArgs;

typedef struct Player {
  FILE *f;
  FILE *wav_f;
  FILE *meta_f;

  IAMF_DecoderHandle dec;

  int channels;
  uint32_t rate;

} Player;

static void print_usage(char *argv[]) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s <options> <input file>\n", argv[0]);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "-i[0-1]    0 : IAMF bitstream input.(default)\n");
  fprintf(stderr, "           1 : mp4 input.\n");
  /* fprintf(stderr, "-o1          : -o1(mp4 dump output)\n"); */
  fprintf(stderr, "-o2        2 : pcm output.\n");
  fprintf(
      stderr,
      "-r [rate]    : audio signal sampling rate, 48000 is the default. \n");
  fprintf(stderr,
          "-ts pos      : seek to a given position in seconds, which is valid "
          "when mp4 file is used as input.\n");
#if SUPPORT_VERIFIER
  fprintf(stderr, "-v <file>    : verification log generation.\n");
#endif
  fprintf(stderr,
          "-s[0~11,b]   : output layout, the sound system A~J and extensions "
          "(Upper + Middle + Bottom).\n");
  fprintf(stderr, "           0 : Sound system A (0+2+0)\n");
  fprintf(stderr, "           1 : Sound system B (0+5+0)\n");
  fprintf(stderr, "           2 : Sound system C (2+5+0)\n");
  fprintf(stderr, "           3 : Sound system D (4+5+0)\n");
#ifndef SAMSUNG_TV
  fprintf(stderr, "           4 : Sound system E (4+5+1)\n");
  fprintf(stderr, "           5 : Sound system F (3+7+0)\n");
  fprintf(stderr, "           6 : Sound system G (4+9+0)\n");
  fprintf(stderr, "           7 : Sound system H (9+10+3)\n");
#endif
  fprintf(stderr, "           8 : Sound system I (0+7+0)\n");
  fprintf(stderr, "           9 : Sound system J (4+7+0)\n");
  fprintf(stderr, "          10 : Sound system extension 712 (2+7+0)\n");
  fprintf(stderr, "          11 : Sound system extension 312 (2+3+0)\n");
  fprintf(stderr, "          12 : Sound system mono (0+1+0)\n");
  fprintf(stderr, "           b : Binaural.\n");
  fprintf(stderr, "-p [dB]      : Peak threshold in dB.\n");
  fprintf(stderr, "-l [LKFS]    : Normalization loudness in LKFS.\n");
  fprintf(stderr, "-d           : Bit depth of pcm output.\n");
  fprintf(stderr, "-mp [id]     : Set mix presentation id.\n");
  fprintf(stderr,
          "-m           : Generate a metadata file with the suffix .met .\n");
  fprintf(stderr, "-disable_limiter\n             : Disable peak limiter.\n");
}

static uint32_t valid_sound_system_layout(uint32_t ss) {
#ifdef SAMSUNG_TV
  return (ss <= SOUND_SYSTEM_D) ||
         (ss >= SOUND_SYSTEM_I && ss < SOUND_SYSTEM_END);
#else
  return ss < SOUND_SYSTEM_END;
#endif
}

typedef struct extradata_header {
  uint32_t nSize;
  uint32_t nVersion;
  uint32_t nPortIndex;
  uint32_t nType;
  uint32_t nDataSize;
} extradata_header;

static int extradata_layout2stream(uint8_t *buf, IAMF_Layout *layout) {
  int s = sizeof(IAMF_Layout);
  memcpy(buf, layout, s);
  return s;
}

static int extradata_loudness2stream(uint8_t *buf,
                                     IAMF_LoudnessInfo *loudness) {
  uint32_t offset = 0;
  memcpy(buf + offset, &loudness->info_type, 1);
  offset += 1;
  memcpy(buf + offset, &loudness->integrated_loudness, 2);
  offset += 2;
  memcpy(buf + offset, &loudness->digital_peak, 2);
  offset += 2;

  if (loudness->info_type & 1) {
    memcpy(buf + offset, &loudness->true_peak, 2);
    offset += 2;
  }

  if (loudness->info_type & 2) {
    memcpy(buf + offset, &loudness->num_anchor_loudness, 1);
    offset += 1;

    for (int i = 0; i < loudness->num_anchor_loudness; ++i) {
      memcpy(buf + offset, &loudness->anchor_loudness[i].anchor_element, 1);
      offset += 1;
      memcpy(buf + offset, &loudness->anchor_loudness[i].anchored_loudness, 2);
      offset += 2;
    }
  }

  return offset;
}

static int extradata_iamf_loudness_size(IAMF_LoudnessInfo *loudness) {
  int ret = 5;
  if (loudness->info_type & 1) ret += 2;
  if (loudness->info_type & 2) {
    ret += (1 + loudness->num_anchor_loudness * sizeof(anchor_loudness_t));
  }
  return ret;
}

static int extradata_iamf_size(IAMF_extradata *meta) {
  int ret = 24;
  for (int i = 0; i < meta->num_loudness_layouts; ++i) {
    ret += sizeof(IAMF_Layout);
    ret += extradata_iamf_loudness_size(&meta->loudness[i]);
  }
  ret += 4;
  if (meta->num_parameters) {
    ret += sizeof(IAMF_Param) * meta->num_parameters;
  }
  /* printf("iamf extradata size %d\n", ret); */
  return ret;
}

/**
 *  [0..7] PTS #8 bytes  // ex) PTS = 90000 * [sample start clock] / 48000
 *  [8..n]
 *  struct extradata_type {
 *     u32 nSize;
 *     u32 nVersion;   // 1
 *     u32 nPortIndex; // 0
 *     u32 nType;       // Extra Data type,  0x7f000001 : raw data,
 *                      // 0x7f000005 : info data
 *     u32 nDataSize;   // Size of the supporting data to follow
 *     u8  data[1];     // Supporting data hint  ===> iamf_extradata
 *   } extradata_type;
 *
 *  struct iamf_extradata {
 *    IAMF_SoundSystem output_sound_system; // sound system (5.1.2 -> -s2): 0~11
 *                                          // in 4.2.5. Layout Syntax and
 *                                          // Semantics <--- renamed target
 *                                          // layout to output_sound_system
 *    uint32_t number_of_samples;
 *    uint32_t bitdepth; // 16 bits per sample
 *    uint32_t sampling_rate; // 48000
 *    int output_sound_mode; // n/a(-1), stereo(0), multichannel(1) and
 *                           // binaural(2).
 *    int num_loudness_layouts;
 *    for (i = 0; i < num_loudness_layouts; i++) {
 *      layout loudness_layout; // stereo, 5.1., 7.1.4, etc
 *      loudness_info loudness;
 *    }
 *    uint32_t num_parameters; // 1
 *    for (i = 0; i < num_parameters; i++) {
 *      int parameter_length; // 8 bytes
 *      uint32_t parameter_definition_type; // PARAMETER_DEFINITION_DEMIXING(1)
 *      uint32_t dmixp_mode;
 *    }
 *  }
 *
 * */
static int extradata_write(FILE *f, int64_t pts, IAMF_extradata *meta) {
  extradata_header h;
  uint8_t *buf;
  uint32_t offset = 0;
  uint32_t size = 0;

  h.nVersion = 1;
  h.nPortIndex = 0;
  h.nType = 0x7f000005;

  h.nDataSize = extradata_iamf_size(meta);
  h.nSize = sizeof(extradata_header) + h.nDataSize;

  size = h.nSize + 8 + 3 & ~3;
  /* printf ("the extradata size is %d\n", size); */

  buf = (uint8_t *)malloc(size);
  if (!buf) return -1;
  memset(buf, 0, size);

  memcpy(buf + offset, &pts, 8);
  offset += 8;

  memcpy(buf + offset, &h, sizeof(extradata_header));
  offset += sizeof(extradata_header);

  memcpy(buf + offset, &meta->output_sound_system, sizeof(uint32_t));
  offset += 4;
  memcpy(buf + offset, &meta->number_of_samples, sizeof(uint32_t));
  offset += 4;
  memcpy(buf + offset, &meta->bitdepth, sizeof(uint32_t));
  offset += 4;
  memcpy(buf + offset, &meta->sampling_rate, sizeof(uint32_t));
  offset += 4;
  memcpy(buf + offset, &meta->output_sound_mode, sizeof(int));
  offset += 4;
  memcpy(buf + offset, &meta->num_loudness_layouts, sizeof(uint32_t));
  offset += 4;

  for (int i = 0; i < meta->num_loudness_layouts; ++i) {
    offset += extradata_layout2stream(buf + offset, &meta->loudness_layout[i]);
    offset += extradata_loudness2stream(buf + offset, &meta->loudness[i]);
  }

  memcpy(buf + offset, &meta->num_parameters, sizeof(uint32_t));
  offset += 4;
  if (meta->num_parameters) {
    for (int i = 0; i < meta->num_parameters; ++i) {
      memcpy(buf + offset, &meta->param[i], sizeof(IAMF_Param));
      offset += sizeof(IAMF_Param);
    }
  }

  fwrite(buf, 1, size, f);

  if (buf) free(buf);
  return size;
}

static void extradata_iamf_clean(IAMF_extradata *data) {
  if (data) {
    if (data->loudness_layout) free(data->loudness_layout);

    if (data->loudness) {
      if (data->loudness->anchor_loudness)
        free(data->loudness->anchor_loudness);
      free(data->loudness);
    }
    if (data->param) free(data->param);
    memset(data, 0, sizeof(IAMF_extradata));
  }
}

static int build_file_name(const char *path, Layout *layout, char *n,
                           uint32_t size) {
  int ret = 0;
  const char *s = 0, *d;

  if (layout->type == 2) {
    snprintf(n, size, "ss%d_", layout->ss);
    ret = strlen(n);
  } else if (layout->type == 3) {
    snprintf(n, size, "binaural_");
    ret = strlen(n);
  } else {
    fprintf(stdout, "Invalid output layout type %d.\n", layout->type);
    return -1;
  }

#if defined(__linux__)
  s = strrchr(path, '/');
#else
  s = strrchr(path, '\\');
#endif
  if (!s)
    s = path;
  else
    ++s;

  d = strrchr(path, '.');
  if (d) {
    int nn = d - s < size - ret - 1 ? d - s : size - ret - 1;
    strncpy(n + ret, s, nn);
    ret += nn;
  }
  n[ret] = 0;

  return ret;
}

static const char *sound_system_string(IAMF_SoundSystem ss) {
  static const char *sss[] = {
      "sound system A",   "sound system B",       "sound system C",
      "sound system D",   "sound system E",       "sound system F",
      "sound system G",   "sound system H",       "sound system I",
      "sound system J",   "sound system EXT 712", "sound system EXT 312",
      "sound system MONO"};

  if (ss < SOUND_SYSTEM_END && ss > SOUND_SYSTEM_INVALID) return sss[ss];
  return "Invalid sound system.";
}

#define BLOCK_SIZE 960 * 6 * 2 * 16
#define NAME_LENGTH 128
#define FCLOSE(f) \
  if (f) {        \
    fclose(f);    \
    f = 0;        \
  }

static int player_init(Player *pr, PlayerArgs *pas) {
  int ret = 0;

  char out[NAME_LENGTH];
  char meta_n[NAME_LENGTH];

  memset(pr, 0, sizeof(Player));

  pr->rate = pas->rate;
  ret = build_file_name(pas->path, pas->layout, out, NAME_LENGTH - 4);
  if (ret < 0) return ret;
  snprintf(out + ret, NAME_LENGTH - ret, "%s", ".wav");

  if (pas->flags & FLAG_METADATA) {
    strcpy(meta_n, out);
    snprintf(meta_n + ret, NAME_LENGTH - ret, "%s", ".met");
    pr->meta_f = fopen(meta_n, "w+");
    if (!pr->meta_f) {
      fprintf(stderr, "%s can't opened.\n", out);
    }
  }

  pr->dec = IAMF_decoder_open();
  if (!pr->dec) {
    fprintf(stderr, "IAMF decoder can't created.\n");
    return -1;
  }

  if (pas->flags & FLAG_DISABLE_LIMITER)
    IAMF_decoder_peak_limiter_enable(pr->dec, 0);
  else
    IAMF_decoder_peak_limiter_set_threshold(pr->dec, pas->peak);
  IAMF_decoder_set_normalization_loudness(pr->dec, pas->loudness);
  IAMF_decoder_set_bit_depth(pr->dec, pas->bit_depth);

  if (pr->rate > 0 &&
      IAMF_decoder_set_sampling_rate(pr->dec, pr->rate) != IAMF_OK) {
    fprintf(stderr, "Invalid sampling rate %u\n", pr->rate);
    return -1;
  }

  if (pas->layout->type == 2) {
    IAMF_decoder_output_layout_set_sound_system(pr->dec, pas->layout->ss);
    pr->channels = IAMF_layout_sound_system_channels_count(pas->layout->ss);
    fprintf(stdout, "%s has %d channels\n",
            sound_system_string(pas->layout->ss), pr->channels);
  } else {
    IAMF_decoder_output_layout_set_binaural(pr->dec);
    pr->channels = IAMF_layout_binaural_channels_count();
    fprintf(stdout, "Binaural has %d channels\n", pr->channels);
  }

  pr->f = fopen(pas->path, "rb");
  if (!pr->f) {
    fprintf(stderr, "%s can't opened.\n", pas->path);
    return -1;
  }

  if (!pr->rate) pr->rate = SAMPLING_RATE;
#ifdef SAMSUNG_TV
  pr->channels = SAMSUNG_SPECIFIC_CHANNELS;
#endif
  pr->wav_f =
      (FILE *)dep_wav_write_open(out, pr->rate, pas->bit_depth, pr->channels);
  if (!pr->wav_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    return -1;
  }

  return 0;
}

#ifdef SAMSUNG_TV
static int player_test_sound_system(Player *pr, PlayerArgs *pas) {
  Layout target;
  int a, sret;

  char out[NAME_LENGTH];
  char meta_n[NAME_LENGTH];

  if (pr->wav_f) dep_wav_write_close(pr->wav_f);
  pr->wav_f = 0;
  FCLOSE(pr->meta_f);

  target.type = 0;

  do {
    srand((unsigned)time(NULL));
    a = rand() % 14;
    if (!valid_sound_system_layout(a) && a != 13) continue;
    if (a < 13) {
      target.type = 2;
      target.ss = a;
    } else {
      target.type = 3;
    }
  } while (!target.type ||
           (target.type == pas->layout->type &&
            (target.type == 3 ||
             (target.type == 2 && target.ss == pas->layout->ss))));

  sret = build_file_name(pas->path, &target, out, NAME_LENGTH - 4);
  if (sret < 0) return -1;
  snprintf(out + sret, NAME_LENGTH - sret, "%s", ".wav");

  if (pas->flags & FLAG_METADATA) {
    strcpy(meta_n, out);
    snprintf(meta_n + sret, NAME_LENGTH - sret, "%s", ".met");
    pr->meta_f = fopen(meta_n, "w+");
    if (!pr->meta_f) {
      fprintf(stderr, "%s can't opened.\n", out);
    }
  }

  if (target.type == 2) {
    IAMF_decoder_output_layout_set_sound_system(pr->dec, target.ss);
    fprintf(stdout, "Change to %s (%d) and it has %d channels\n",
            sound_system_string(target.ss), target.ss,
            IAMF_layout_sound_system_channels_count(target.ss));
  } else {
    IAMF_decoder_output_layout_set_binaural(pr->dec);
    fprintf(stdout, "Change to binaural and its has %d channels\n",
            IAMF_layout_binaural_channels_count());
  }

  pr->wav_f = (FILE *)dep_wav_write_open(out, pr->rate, pas->bit_depth,
                                         SAMSUNG_SPECIFIC_CHANNELS);
  if (!pr->wav_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    return -1;
  }

  if (IAMF_decoder_configure(pr->dec, 0, 0, 0) != IAMF_OK) {
    fprintf(stderr, "fail to reconfigure for sound system.");
    return -1;
  }

  return 0;
}
#endif

static void player_clean(Player *pr) {
  FCLOSE(pr->f);
  FCLOSE(pr->meta_f);

  if (pr->wav_f) dep_wav_write_close(pr->wav_f);
  if (pr->dec) IAMF_decoder_close(pr->dec);
}

static int bs_input_wav_output(PlayerArgs *pas) {
  Player pr;
  uint8_t block[BLOCK_SIZE];
  int used = 0, end = 0;
  int ret = 0;
  int state = 0;
  uint32_t rsize = 0;
  void *pcm = NULL;
  int count = 0, samples = 0;
  uint64_t frsize = 0, fsize = 0;
  uint32_t size;
  uint32_t flags = pas->flags;

  if (!pas->path) return -1;

  ret = player_init(&pr, pas);
  if (ret < 0) goto end;

  fseek(pr.f, 0L, SEEK_END);
  fsize = ftell(pr.f);
  fseek(pr.f, 0L, SEEK_SET);

  do {
    ret = 0;
    if (BLOCK_SIZE != used) {
      ret = fread(block + used, 1, BLOCK_SIZE - used, pr.f);
      if (ret < 0) {
        fprintf(stderr, "file read error : %d (%s).\n", ret, strerror(ret));
        break;
      }
      if (!ret) end = 1;
    }

    frsize += ret;
    /* fprintf(stdout, "Read FILE ========== read %d and count %" PRIu64"\n",
     * ret, */
    /* frsize); */
    size = used + ret;
    used = 0;
    if (state <= 0) {
      if (end) break;
      rsize = 0;
      if (!state) IAMF_decoder_set_pts(pr.dec, 0, 90000);
      if (pas->mix_presentation_id != UINT64_MAX)
        IAMF_decoder_set_mix_presentation_id(pr.dec, pas->mix_presentation_id);
      ret = IAMF_decoder_configure(pr.dec, block + used, size - used, &rsize);
      if (ret == IAMF_OK) {
        state = 1;
        IAMF_StreamInfo *info = IAMF_decoder_get_stream_info(pr.dec);
        if (!pcm)
          pcm = (void *)malloc(pas->bit_depth / 8 * info->max_frame_size *
                               pr.channels);
      } else if (ret != IAMF_ERR_BUFFER_TOO_SMALL) {
        fprintf(stderr, "errno: %d, fail to configure decoder.\n", ret);
        break;
      } else if (!rsize) {
        fprintf(stderr, "errno: %d, buffer is too small.\n", ret);
        break;
      }
      /* fprintf(stdout, "header length %d, ret %d\n", rsize, ret); */
      used += rsize;
    }
    if (state > 0) {
      IAMF_extradata meta;
      int64_t pts;
      while (1) {
        rsize = 0;
        if (!end)
          ret = IAMF_decoder_decode(pr.dec, block + used, size - used, &rsize,
                                    pcm);
        else
          ret = IAMF_decoder_decode(pr.dec, (const uint8_t *)NULL, 0, &rsize,
                                    pcm);

        /* fprintf(stdout, "read packet size %d\n", rsize); */
        if (ret > 0) {
          ++count;
          samples += ret;
          /* fprintf(stderr, "===================== Get %d frame and size %d\n",
           * count, ret); */
          dep_wav_write_data(pr.wav_f, (unsigned char *)pcm,
                             (pas->bit_depth / 8) * ret * pr.channels);

          if (pas->flags & FLAG_METADATA) {
            IAMF_decoder_get_last_metadata(pr.dec, &pts, &meta);
            if (pr.meta_f) extradata_write(pr.meta_f, pts, &meta);
            extradata_iamf_clean(&meta);
          }
        }

        if (end) break;

        used += rsize;

        if (ret == IAMF_ERR_INVALID_STATE) {
          state = ret;
          printf("state change to invalid, need reconfigure.\n");
        }

        if (ret < 0 || used >= size || !rsize) {
          break;
        }
      }
    }

    if (end) break;

    memmove(block, block + used, size - used);
    used = size - used;

#ifdef SAMSUNG_TV
    if (ret >= 0 && flags & FLAG_TEST_SOUND_SYSTEM && frsize * 2 > fsize) {
      ret = player_test_sound_system(&pr, pas);
      if (ret < 0) goto end;
      flags &= ~FLAG_TEST_SOUND_SYSTEM;
    }
#endif
  } while (1);

end:
  fprintf(stderr, "===================== Get %d frames\n", count);
  fprintf(stderr, "===================== Get %d samples\n", samples);

  if (fsize != frsize)
    fprintf(stderr,
            "file is read %" PRIu64 " (vs %" PRIu64
            "), not completely. return value %d\n",
            frsize, fsize, ret);

  if (pcm) free(pcm);
  player_clean(&pr);

  return ret;
}

static int mp4_input_wav_output2(PlayerArgs *pas) {
  MP4IAMFParser mp4par;
  IAMFHeader *header = 0;
  Player pr;
  uint8_t *block = 0;
  uint32_t size = 0;
  int used = 0, end = 0;
  int ret = 0;
  uint32_t rsize = 0;
  void *pcm = NULL;
  int count = 0, samples = 0;
  uint64_t frsize = 0;
  const char *s = 0, *d;
  int entno = 0;
  int64_t sample_offs;
  int64_t st = 0;
  uint32_t flags = pas->flags;

  if (!pas->path) return -1;

  ret = player_init(&pr, pas);
  if (ret < 0) goto end;

  memset(&mp4par, 0, sizeof(mp4par));
  mp4_iamf_parser_init(&mp4par);
  mp4_iamf_parser_set_logger(&mp4par, 0);
  ret = mp4_iamf_parser_open_audio_track(&mp4par, pas->path, &header);

  if (ret <= 0) {
    fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", pas->path);
    goto end;
  }

  if (pas->st)
    st = pas->st * 90000;
  else {
    double r = header->skip * 90000;
    st = r / header->timescale + 0.5f;
    printf("skip %d/%d pts is %" PRId64 "/90000\n", header->skip,
           header->timescale, st);
  }
  IAMF_decoder_set_pts(pr.dec, st * -1, 90000);

  if (pas->st > 0) {
    ret = mp4_iamf_parser_set_starting_time(&mp4par, 0, pas->st);
    if (ret < 0) {
      fprintf(stderr, "invalid starting time for %s\n", pas->path);
      goto end;
    }

    mp4_iamf_parser_get_audio_track_header(&mp4par, &header);
  }

  ret = iamf_header_read_description_OBUs(header, &block, &size);
  if (!ret) {
    fprintf(stderr, "fail to copy description obu.\n");
    goto end;
  }

  if (pas->mix_presentation_id != UINT64_MAX)
    IAMF_decoder_set_mix_presentation_id(pr.dec, pas->mix_presentation_id);
  ret = IAMF_decoder_configure(pr.dec, block, ret, 0);
  IAMF_StreamInfo *info = IAMF_decoder_get_stream_info(pr.dec);
  pcm = (void *)malloc(pas->bit_depth / 8 * info->max_frame_size * pr.channels);
  if (!pcm) {
    ret = errno;
    fprintf(stderr, "error no(%d):fail to malloc memory for pcm.", ret);
    goto end;
  }
  if (block) free(block);
  if (ret != IAMF_OK) {
    fprintf(stderr, "errno: %d, fail to configure decoder.\n", ret);
    goto end;
  }

  do {
    IAMF_extradata meta;
    int64_t pts;
    if (mp4_iamf_parser_read_packet(&mp4par, 0, &block, &size, &sample_offs,
                                    &entno) < 0) {
      end = 1;
    }

    if (!end)
      ret = IAMF_decoder_decode(pr.dec, block, size, 0, pcm);
    else
      ret = IAMF_decoder_decode(pr.dec, (const uint8_t *)NULL, 0, 0, pcm);

    if (block) free(block);
    block = 0;
    if (ret > 0) {
      ++count;
      samples += ret;
      /* fprintf(stderr, */
      /* "===================== Get %d frame and size %d, offset %" PRId64"\n",
       */
      /* count, ret, sample_offs); */
      dep_wav_write_data(pr.wav_f, (unsigned char *)pcm,
                         (pas->bit_depth / 8) * ret * pr.channels);

      if (pas->flags & FLAG_METADATA) {
        IAMF_decoder_get_last_metadata(pr.dec, &pts, &meta);
        if (pr.meta_f) extradata_write(pr.meta_f, pts, &meta);
        extradata_iamf_clean(&meta);
      }

#ifdef SAMSUNG_TV
      if (flags & FLAG_TEST_SOUND_SYSTEM && count > 5) {
        ret = player_test_sound_system(&pr, pas);
        if (ret < 0) goto end;
        flags &= ~FLAG_TEST_SOUND_SYSTEM;
      }
#endif
    }
    if (end) break;
  } while (1);
end:
  fprintf(stderr, "===================== Get %d frames\n", count);
  fprintf(stderr, "===================== Get %d samples\n", samples);

  if (pcm) free(pcm);
  mp4_iamf_parser_close(&mp4par);
  player_clean(&pr);

  return ret;
}

int main(int argc, char *argv[]) {
  int args;
  int output_mode = 0;
  int input_mode = 0;
  int tc = 0;
  int sound_system = -1;
  char *f = 0;
  Layout target;
  PlayerArgs pas;

  memset(&pas, 0, sizeof(pas));
  pas.layout = &target;
  pas.peak = -1.f;
  pas.loudness = .0f;
  pas.bit_depth = 16;
  pas.mix_presentation_id = UINT64_MAX;

  if (argc < 2) {
    print_usage(argv);
    return -1;
  }

#if SUPPORT_VERIFIER
  char *vlog_file = 0;
#endif

  memset(&target, 0, sizeof(Layout));
  args = 1;
  fprintf(stdout, "IN ARGS:\n");
  while (args < argc) {
    if (argv[args][0] == '-') {
      if (argv[args][1] == 'o') {
        output_mode = atoi(argv[args] + 2);
        fprintf(stdout, "Output mode %d\n", output_mode);
      } else if (argv[args][1] == 'i') {
        input_mode = atoi(argv[args] + 2);
        fprintf(stdout, "Input mode %d\n", input_mode);
      } else if (argv[args][1] == 's') {
        if (argv[args][2] == 'b') {
          target.type = 3;
          fprintf(stdout, "Output binaural\n");
        } else {
          sound_system = atoi(argv[args] + 2);
          fprintf(stdout, "Output sound system %d\n", sound_system);
          if (valid_sound_system_layout(sound_system)) {
            target.type = 2;
            target.ss = sound_system;
          } else {
            fprintf(stderr, "Invalid output layout of sound system %d\n",
                    sound_system);
          }
        }
      } else if (!strcmp(argv[args], "-p")) {
        pas.peak = strtof(argv[++args], 0);
        fprintf(stdout, "Peak threshold value : %g db\n", pas.peak);
      } else if (!strcmp(argv[args], "-l")) {
        pas.loudness = strtof(argv[++args], 0);
        fprintf(stdout, "Normalization loudness value : %g LKFS\n",
                pas.loudness);
      } else if (!strcmp(argv[args], "-d")) {
        pas.bit_depth = strtof(argv[++args], 0);
        fprintf(stdout, "Bit depth of pcm output : %u bit\n", pas.bit_depth);
      } else if (!strcmp(argv[args], "-m")) {
        pas.flags |= FLAG_METADATA;
        fprintf(stdout, "Generate metadata file");
      } else if (argv[args][1] == 'v') {
#if SUPPORT_VERIFIER
        pas.flags |= FLAG_VLOG;
        vlog_file = argv[++args];
        fprintf(stdout, "Verification log file : %s\n", vlog_file);
#endif
      } else if (argv[args][1] == 'h') {
        print_usage(argv);
        return 0;
      } else if (argv[args][1] == 't') {
        if (!strcmp(argv[args], "-ts")) {
          pas.st = strtoul(argv[++args], NULL, 10);
          fprintf(stdout, "Start time : %us\n", pas.st);
#ifdef SAMSUNG_TV
        } else if (!strcmp(argv[args], "-test_soundsystem")) {
          pas.flags |= FLAG_TEST_SOUND_SYSTEM;
          fprintf(stdout, "Flag: Test sound system.\n");
#endif
        }
      } else if (!strcmp(argv[args], "-r")) {
        pas.rate = strtoul(argv[++args], NULL, 10);
        fprintf(stdout, "sampling rate : %u\n", pas.rate);
      } else if (!strcmp(argv[args], "-mp")) {
        pas.mix_presentation_id = strtoull(argv[++args], NULL, 10);
        fprintf(stdout, "select mix presentation id %" PRId64,
                pas.mix_presentation_id);
      } else if (!strcmp(argv[args], "-disable_limiter")) {
        pas.flags |= FLAG_DISABLE_LIMITER;
        fprintf(stdout, "Disable peak limiter\n");
      }
    } else {
      f = argv[args];
      if (!f) {
        fprintf(stderr, "Input file path.\n");
        return 0;
      }
      fprintf(stdout, "Input file : %s\n", f);
      pas.path = f;
    }
    args++;
  }

  if (input_mode != 1 && pas.st) {
    fprintf(stderr, "ERROR: -st is valid when mp4 file is used as input.\n");
    print_usage(argv);
    return -1;
  }

  if (target.type) {
#if SUPPORT_VERIFIER
    if (pas.flags & FLAG_VLOG) vlog_file_open(vlog_file);
#endif
    if (!input_mode && output_mode == 2) {
      bs_input_wav_output(&pas);
    } else if (input_mode == 1 && output_mode == 2) {
      // mp4_input_wav_output(&pas);
      mp4_input_wav_output2(&pas);
    }
#if SUPPORT_VERIFIER
    if (pas.flags & FLAG_VLOG) vlog_file_close();
#endif
    else {
      fprintf(stderr, "invalid output mode %d\n", output_mode);
    }
  } else {
    print_usage(argv);
    fprintf(stderr, "invalid output sound system %d\n", sound_system);
  }

  return 0;
}
