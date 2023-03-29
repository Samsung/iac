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
#include <stdio.h>
#include <stdlib.h>

#include "IAMF_decoder.h"
#include "mp4iamfpar.h"
#include "string.h"
#include "wavwriter.h"

typedef struct Layout {
  int type;
  union {
    IAMF_SoundSystem ss;
    struct {
      int nb_labels;
      uint8_t *labels;
    } label;
  };
} Layout;

typedef struct TestCase {
  const char *path;
  Layout *layout;
} TestCase;

static int bs_input_wav_output(const char *path, Layout *layout, float db);

static Layout ss[] = {
    {2, SOUND_SYSTEM_A}, {2, SOUND_SYSTEM_B},       {2, SOUND_SYSTEM_C},
    {2, SOUND_SYSTEM_D}, {2, SOUND_SYSTEM_E},       {2, SOUND_SYSTEM_F},
    {2, SOUND_SYSTEM_G}, {2, SOUND_SYSTEM_H},       {2, SOUND_SYSTEM_I},
    {2, SOUND_SYSTEM_J}, {2, SOUND_SYSTEM_EXT_712}, {2, SOUND_SYSTEM_EXT_312},
};

static const char *bs_test_files[] = {
    "./testcases/channel-based/simple_profile.iamf",
    "./testcases/channel-based/base_profile.iamf",
    "./testcases/pcm/simple_profile.iamf",
    "./testcases/scene-based/simple_profile.iamf",
    "./testcases/scene-based/base_profile.iamf",
    "./testcases/scene-based/projection_simple_profile.iamf",
};

static void print_usage(char *argv[]) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s <options> <input file>\n", argv[0]);
  fprintf(stderr, "options:\n");
  /* fprintf(stderr, */
  /* "-t2          : -t2(test internal testcases, IAMF bitstream input and pcm
   * output).\n"); */
  fprintf(stderr, "-i[0-1]    0 : IAMF bitstream input.(default)\n");
  fprintf(stderr, "           1 : mp4 input.\n");
  /* fprintf(stderr, "-o1          : -o1(mp4 dump output)\n"); */
  fprintf(stderr, "-o2        2 : pcm output.\n");
  fprintf(stderr,
          "-s[0~11,b]   : output layout, the sound system A~J and extensions "
          "(Upper + Middle + Bottom).\n");
  fprintf(stderr, "           0 : Sound system A (0+2+0)\n");
  fprintf(stderr, "           1 : Sound system B (0+5+0)\n");
  fprintf(stderr, "           2 : Sound system C (2+5+0)\n");
  fprintf(stderr, "           3 : Sound system D (4+5+0)\n");
  fprintf(stderr, "           4 : Sound system E (4+5+1)\n");
  fprintf(stderr, "           5 : Sound system F (3+7+0)\n");
  fprintf(stderr, "           6 : Sound system G (4+9+0)\n");
  fprintf(stderr, "           7 : Sound system H (9+10+3)\n");
  fprintf(stderr, "           8 : Sound system I (0+7+0)\n");
  fprintf(stderr, "           9 : Sound system J (4+7+0)\n");
  fprintf(stderr, "          10 : Sound system extension 712 (2+7+0)\n");
  fprintf(stderr, "          11 : Sound system extension 312 (2+3+0)\n");
  fprintf(stderr, "           b : Binaural.\n");
  fprintf(stderr, "-p [dB]      : Peak threshold in dB.\n");
}

static uint32_t valid_sound_system_layout(uint32_t ss) {
  return ss <= SOUND_SYSTEM_EXT_312 ? 1 : 0;
}

static void bs_ss_testcases_run(int sound_system) {
  int cnt = sizeof(ss) / sizeof(Layout);
  int fcnt = sizeof(bs_test_files) / sizeof(char *);
  const char *path = 0;
  Layout *layout = 0;

  for (int i = 0; i < cnt; ++i) {
    if (sound_system != -1 && ss[i].ss != sound_system) continue;

    for (int j = 0; j < fcnt; ++j) {
      path = bs_test_files[j];
      layout = &ss[i];

      bs_input_wav_output(path, layout, -1.0f);
    }
  }
}

typedef struct extradata_header {
  uint32_t nSize;
  uint32_t nVersion;
  uint32_t nPortIndex;
  uint32_t nType;
  uint32_t nDataSize;
} extradata_header;

static int extradata_layout2stream(uint8_t *buf, IAMF_Layout *layout) {
  uint32_t offset = 0;

  memcpy(buf + offset, layout, 1);
  ++offset;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
    memcpy(buf + offset, &layout->sp_labels.sp_label,
           layout->sp_labels.num_loudspeakers);
    offset += layout->sp_labels.num_loudspeakers;
  }
  return offset;
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

  return offset;
}

static int extradata_iamf_layout_size(IAMF_Layout *layout) {
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL)
    return 1 + layout->sp_labels.num_loudspeakers;
  return 1;
}

static int extradata_iamf_loudness_size(IAMF_LoudnessInfo *loudness) {
  if (loudness->info_type & 1) return 7;
  return 5;
}

static int extradata_iamf_size(IAMF_extradata *meta) {
  int ret = 24;
  for (int i = 0; i < meta->num_loudness_layouts; ++i) {
    ret += extradata_iamf_layout_size(&meta->loudness_layout[i]);
    ret += extradata_iamf_loudness_size(&meta->loudness[i]);
  }
  ret += 4;
  if (meta->num_parameters) {
    ret += sizeof(IAMF_Param);
  }
  /* printf("iamf extradata size %d\n", ret); */
  return ret;
}

/**
 *  [0..3] PTS #4 bytes  // ex) PTS = 90000 * [sample start clock] / 48000
 *  [4..n]
 *  struct extradata_type {
 *     u32 nSize;
 *     u32 nVersion;   // 1
 *     u32 nPortIndex; // 0
 *     u32 nType;       // Extra Data type,  0x7f000001 : raw data, 0x7f000005 :
 * info data u32 nDataSize;   // Size of the supporting data to follow u8
 * data[1];     // Supporting data hint  ===>iamf_extradata } extradata_type;
 *
 *  struct iamf_extradata {
 *    IAMF_SoundSystem output_sound_system; // sound system (5.1.2 -> -s2): 0~11 in 4.2.5. Layout Syntax and Semantics <--- renamed target layout to output_sound_system
 *    uint32_t number_of_samples;
 *    uint32_t bitdepth; // 16 bits per sample
 *    uint32_t sampling_rate; // 48000
 *    int output_sound_mode; // n/a(-1), stereo(0), multichannel(1) and binaural(2).
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
static int extradata_write(FILE *f, uint32_t pts, IAMF_extradata *meta) {
  extradata_header h;
  uint8_t *buf;
  uint32_t offset = 0;
  uint32_t size = 0;

  h.nVersion = 1;
  h.nPortIndex = 0;
  h.nType = 0x7f000005;

  h.nDataSize = extradata_iamf_size(meta);
  h.nSize = sizeof(extradata_header) + h.nDataSize;

  size = h.nSize + 4 + 3 & ~3;
  /* printf ("the extradata size is %d\n", size); */

  buf = (uint8_t *)malloc(size);
  if (!buf) return -1;
  memset(buf, 0, size);

  memcpy(buf + offset, &pts, 4);
  offset += 4;

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
  memcpy(buf + offset, &meta->num_loudness_layouts, sizeof(uint32_t));
  offset += 4;
  memcpy(buf + offset, &meta->output_sound_mode, sizeof(int));
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
}

static void extradata_iamf_layout_clean(IAMF_Layout *layout) {
  if (layout && layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL &&
      layout->sp_labels.sp_label) {
    free(layout->sp_labels.sp_label);
  }
}

static void extradata_iamf_clean(IAMF_extradata *data) {
  if (data) {
    if (data->loudness_layout) {
      for (int i = 0; i < data->num_loudness_layouts; ++i)
        extradata_iamf_layout_clean(&data->loudness_layout[i]);
      free(data->loudness_layout);
    }

    if (data->loudness) free(data->loudness);
    if (data->param) free(data->param);
  }
}

#define BLOCK_SIZE 8192
#define NAME_LENGTH 128
#define FCLOSE(f) \
  if (f) {        \
    fclose(f);    \
    f = 0;        \
  }

int bs_input_wav_output(const char *path, Layout *layout, float db) {
  FILE *f;
  FILE *wav_f, *meta_f;
  uint8_t block[BLOCK_SIZE];
  char out[NAME_LENGTH] = {0};
  char meta_n[NAME_LENGTH] = {0};
  int used = 0;
  int ret = 0;
  int state = 0;
  int rsize = 0;
  void *pcm = NULL;
  IAMF_DecoderHandle dec;
  int channels;
  int count = 0;
  uint32_t frsize = 0;
  uint32_t size;
  const char *s = 0, *d;
  static const char *type_prefix[] = {"channel_", "pcm_", "scene_"};

  if (!path) return -1;

  if (layout->type == 2) {
    fprintf(stdout, "input file %s, and output layout 0x%x\n", path,
            layout->type == 2 ? layout->ss : (uint32_t)-1);

    snprintf(out, NAME_LENGTH, "ss%d_", layout->ss);
    ret = strlen(out);
  } else if (layout->type == 3) {
    fprintf(stdout, "input file %s, and output layout binaural.\n", path);
    snprintf(out, NAME_LENGTH, "binaural_");
    ret = strlen(out);
  } else {
    fprintf(stdout, "Invalid output layout type %d.\n", layout->type);
    return -1;
  }

  if (strstr(path, "channel-based")) {
    s = type_prefix[0];
  } else if (strstr(path, "pcm")) {
    s = type_prefix[1];
  } else if (strstr(path, "scene-based")) {
    s = type_prefix[2];
  }
  if (s) {
    snprintf(out + ret, NAME_LENGTH - ret, "%s", s);
    ret = strlen(out);
  }

  s = strrchr(path, '/');
  if (!s) {
    s = path;
  } else {
    ++s;
  }
  d = strrchr(path, '.');
  if (d) {
    strncpy(out + ret, s,
            d - s < NAME_LENGTH - 5 - ret ? d - s : NAME_LENGTH - 5 - ret);
    ret = strlen(out);
  }
  strcpy(meta_n, out);
  snprintf(out + ret, NAME_LENGTH - ret, "%s", ".wav");
  snprintf(meta_n + ret, NAME_LENGTH - ret, "%s", ".met");

  dec = IAMF_decoder_open();
  if (!dec) {
    fprintf(stderr, "IAMF decoder can't created.\n");
    return -1;
  }

  IAMF_decoder_peak_limiter_set_threshold(dec, db);
  if (layout->type == 2) {
    IAMF_decoder_output_layout_set_sound_system(dec, layout->ss);
    channels = IAMF_layout_sound_system_channels_count(layout->ss);

    fprintf(stdout, "layout (0x%x) has %d channels\n",
            layout->type == 0 ? layout->ss : (uint32_t)-1, channels);
  } else if (layout->type == 3) {
    IAMF_decoder_output_layout_set_binaural(dec);
    channels = IAMF_layout_binaural_channels_count();
    fprintf(stdout, "binaural has %d channels\n", channels);
  } else {
    fprintf(stderr, "Invalid layout");
    return -1;
  }

  f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "%s can't opened.\n", path);
    return -1;
  }

  wav_f = (FILE *)wav_write_open(out, 48000, 16, channels);
  if (!wav_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    return -1;
  }
  meta_f = fopen(meta_n, "w+");
  if (!meta_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    return -1;
  }

  do {
    ret = fread(block + used, 1, BLOCK_SIZE - used, f);
    if (ret < 0) {
      fprintf(stderr, "file read error : %d (%s).\n", ret, strerror(ret));
      break;
    }
    if (!ret) {
      break;
    }

    frsize += ret;
    fprintf(stdout, "Read FILE ========== read %d and count %u\n", ret, frsize);
    size = used + ret;
    used = 0;
    if (state <= 0) {
      rsize = 0;
      if (!state) IAMF_decoder_set_pts(dec, 0, 90000);
      ret = IAMF_decoder_configure(dec, block + used, size - used, &rsize);
      if (ret == IAMF_OK) {
        state = 1;
        if (!pcm) pcm = (void *)malloc(sizeof(int16_t) * 3840 * channels);
      }
      fprintf(stdout, "header length %d, ret %d\n", rsize, ret);
      used += rsize;
    }
    if (state > 0) {
      IAMF_extradata meta;
      uint32_t pts;
      while (1) {
        rsize = 0;
        ret = IAMF_decoder_decode(dec, block + used, size - used, &rsize, pcm);
        fprintf(stdout, "read packet size %d\n", rsize);
        if (ret > 0) {
          ++count;
          /* fprintf(stderr, "===================== Get %d frame and size %d\n",
           * count, ret); */
          wav_write_data(wav_f, (unsigned char *)pcm,
                         sizeof(int16_t) * ret * channels);

          IAMF_decoder_get_last_metadata(dec, &pts, &meta);
          extradata_write(meta_f, pts, &meta);
          extradata_iamf_clean(&meta);
        }
        used += rsize;

        if (ret == IAMF_ERR_INVALID_STATE) {
          state = ret;
          printf("state change to invalid, need reconfigure.\n");
        }

        if (ret <= 0) {
          break;
        }
      }
    }
    memmove(block, block + used, size - used);
    used = size - used;
  } while (1);
  fprintf(stderr, "===================== Get %d frames\n", count);

  if (pcm) {
    free(pcm);
  }

  FCLOSE(f)
  FCLOSE(meta_f)

  if (wav_f) {
    wav_write_close(wav_f);
  }
  if (dec) {
    IAMF_decoder_close(dec);
  }
  return ret;
}

int mp4_input_wav_output(const char *path, Layout *layout, float db) {
  MP4IAMFParser mp4par;
  IAMFHeader *header = 0;
  FILE *f = 0;
  FILE *wav_f = 0, *meta_f = 0;
  FILE *dump_f = 0;
  uint8_t block[BLOCK_SIZE];
  char out[NAME_LENGTH] = {0};
  char meta_n[NAME_LENGTH] = {0};
  char dump_fn[NAME_LENGTH] = {0};
  int used = 0;
  int ret = 0;
  int state = 0;
  int rsize = 0;
  void *pcm = NULL;
  IAMF_DecoderHandle dec;
  int channels;
  int count = 0;
  uint32_t frsize = 0;
  uint32_t size;
  const char *s = 0, *d;
  int entno = 0;
  int64_t sample_offs;
  static const char *type_prefix[] = {"channel_", "pcm_", "scene_"};

  if (!path) return -1;

  if (layout->type == 2) {
    fprintf(stdout, "input file %s, and output layout 0x%x\n", path,
            layout->type == 2 ? layout->ss : (uint32_t)-1);

    snprintf(out, NAME_LENGTH, "ss%d_", layout->ss);
    ret = strlen(out);
  } else if (layout->type == 3) {
    fprintf(stdout, "input file %s, and output layout binaural.\n", path);
    snprintf(out, NAME_LENGTH, "binaural_");
    ret = strlen(out);
  } else {
    fprintf(stdout, "Invalid output layout type %d.\n", layout->type);
    return -1;
  }

  if (strstr(path, "channel-based")) {
    s = type_prefix[0];
  } else if (strstr(path, "pcm")) {
    s = type_prefix[1];
  } else if (strstr(path, "scene-based")) {
    s = type_prefix[2];
  }
  if (s) {
    snprintf(out + ret, NAME_LENGTH - ret, "%s", s);
    ret = strlen(out);
  }

  s = strrchr(path, '/');
  if (!s) {
    s = path;
  } else {
    ++s;
  }
  d = strrchr(path, '.');
  if (d) {
    strncpy(out + ret, s,
            d - s < NAME_LENGTH - 5 - ret ? d - s : NAME_LENGTH - 5 - ret);
    ret = strlen(out);
  }
  strcpy(meta_n, out);
  strcpy(dump_fn, out);
  snprintf(out + ret, NAME_LENGTH - ret, "%s", ".wav");
  snprintf(meta_n + ret, NAME_LENGTH - ret, "%s", ".met");
  snprintf(dump_fn + ret, NAME_LENGTH - ret, "%s", ".log");

  dec = IAMF_decoder_open();
  if (!dec) {
    fprintf(stderr, "IAMF decoder can't created.\n");
    return -1;
  }

  IAMF_decoder_peak_limiter_set_threshold(dec, db);

  if (layout->type == 2) {
    IAMF_decoder_output_layout_set_sound_system(dec, layout->ss);
    channels = IAMF_layout_sound_system_channels_count(layout->ss);

    fprintf(stdout, "layout (0x%x) has %d channels\n",
            layout->type == 0 ? layout->ss : (uint32_t)-1, channels);
  } else if (layout->type == 3) {
    IAMF_decoder_output_layout_set_binaural(dec);
    channels = IAMF_layout_binaural_channels_count();
    fprintf(stdout, "binaural has %d channels\n", channels);
  } else {
    fprintf(stderr, "Invalid layout");
    ret = -1;
    goto end;
  }

  f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "%s can't opened.\n", path);
    ret = errno;
    goto end;
  }

  wav_f = (FILE *)wav_write_open(out, 48000, 16, channels);
  if (!wav_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    ret = errno;
    goto end;
  }
  meta_f = fopen(meta_n, "w+");
  if (!meta_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    ret = errno;
    goto end;
  }

  dump_f = fopen(dump_fn, "w+");
  if (!dump_f) {
    fprintf(stderr, "log file %s can not open.\n", dump_fn);
    ret = errno;
    goto end;
  }

  pcm = (void *)malloc(sizeof(int16_t) * 3840 * channels);
  if (!pcm) {
    ret = errno;
    fprintf(stderr, "error no(%d):fail to malloc memory for pcm.", ret);
    goto end;
  }

  mp4_iamf_parser_init(&mp4par);
  mp4_iamf_parser_set_logger(&mp4par, dump_f);
  ret = mp4_iamf_parser_open_trak(&mp4par, path, &header);

  if (ret <= 0) {
    fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", path);
    goto end;
  }

  IAMF_decoder_set_pts(dec, 0, 90000);

  ret = iamf_header_read_description_OBUs(header, block, BLOCK_SIZE);
  if (!ret) {
    fprintf(stderr, "fail to copy description obu.");
    goto end;
  }
  used += ret;

  do {
    IAMF_extradata meta;
    uint32_t pts;
    if (mp4_iamf_parser_read_packet(&mp4par, 0, (void *)(block + used),
                                    BLOCK_SIZE - used, &ret, &sample_offs,
                                    &entno) < 0)
      break;

    used += ret;

    /* fprintf(stdout, "packet size %d, add %d\n", used, ret); */

  configure:
    if (state <= 0) {
      rsize = 0;
      ret = IAMF_decoder_configure(dec, block, used, &rsize);
      /* fprintf(stdout, "header length %d\n", rsize); */
      if (rsize < used) memmove(block, block + rsize, used - rsize);
      used -= rsize;

      if (ret == IAMF_OK) {
        state = 1;
      } else
        continue;
    }
  decode:
    rsize = 0;
    ret = IAMF_decoder_decode(dec, block, used, &rsize, pcm);
    /* fprintf(stdout, "packet size %d, read %d, ret %d\n", used, rsize, ret);
     */
    if (ret > 0) {
      ++count;
      /* fprintf(stderr, "===================== Get %d frame and size %d\n",
       * count, ret); */
      wav_write_data(wav_f, (unsigned char *)pcm,
                     sizeof(int16_t) * ret * channels);

      IAMF_decoder_get_last_metadata(dec, &pts, &meta);
      extradata_write(meta_f, pts, &meta);
      extradata_iamf_clean(&meta);
    }
    if (rsize < used) memmove(block, block + rsize, used - rsize);
    used -= rsize;
    if (ret == IAMF_ERR_INVALID_STATE) {
      state = ret;
      goto configure;
    }
    if (used) goto decode;

  } while (1);

end:
  fprintf(stderr, "===================== Get %d frames\n", count);
  if (pcm) {
    free(pcm);
  }

  FCLOSE(f)
  FCLOSE(dump_f)
  FCLOSE(meta_f)

  if (wav_f) {
    wav_write_close(wav_f);
  }

  if (dec) {
    IAMF_decoder_close(dec);
  }

  mp4_iamf_parser_close(&mp4par);

  return ret;
}

int main(int argc, char *argv[]) {
  int args;
  int output_mode = 0;
  int input_mode = 0;
  int tc = 0;
  int sound_system = -1;
  char *f = 0;
  float db = -1.f;
  Layout target;

  if (argc < 2) {
    print_usage(argv);
    return -1;
  }

  memset(&target, 0, sizeof(Layout));
  args = 1;
  while (args < argc) {
    if (argv[args][0] == '-') {
      if (argv[args][1] == 'o') {
        output_mode = atoi(argv[args] + 2);
        fprintf(stdout, "output mode %d\n", output_mode);
      } else if (argv[args][1] == 'i') {
        input_mode = atoi(argv[args] + 2);
        fprintf(stdout, "input mode %d\n", input_mode);
      } else if (argv[args][1] == 't') {
        tc = atoi(argv[args] + 2);
        fprintf(stdout, "tc %d\n", tc);
      } else if (argv[args][1] == 's') {
        if (argv[args][2] == 'b') {
          target.type = 3;
          fprintf(stdout, "Binaural\n");
        } else {
          sound_system = atoi(argv[args] + 2);
          fprintf(stdout, "sound system %d\n", sound_system);
          if (valid_sound_system_layout(sound_system)) {
            target.type = 2;
            target.ss = sound_system;
          } else {
            fprintf(stderr, "invalid layout of sound system %d\n", sound_system);
          }
        }
      } else if (argv[args][1] == 'p') {
        db = strtof(argv[args] + 3, 0);
        fprintf(stderr, "peak threshold value : %g db\n", db);
      } else if (argv[args][1] == 'h') {
        print_usage(argv);
        return 0;
      }
    } else {
      f = argv[args];
    }
    args++;
  }

  if (!f) {
    fprintf(stderr, "Input file path.\n");
    print_usage(argv);
    return 0;
  }

  if (tc == 2) {
    bs_ss_testcases_run(sound_system);
  } else if (target.type) {
    if (!input_mode && output_mode == 2) {
      bs_input_wav_output(f, &target, db);
    } else if (input_mode == 1 && output_mode == 2) {
      mp4_input_wav_output(f, &target, db);
    } else {
      fprintf(stderr, "invalid output mode %d\n", output_mode);
    }
  } else {
    print_usage(argv);
    fprintf(stderr, "invalid output sound system %d\n", sound_system);
  }

  return 0;
}
