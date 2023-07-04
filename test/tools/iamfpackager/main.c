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
 * @file main.c
 * @brief unit test of iamfpackager
 * @version 0.1
 * @date Created 3/3/2023
 **/

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "IAMF_encoder.h"
#include "cJSON.h"
#include "dep_wavreader.h"
#include "dep_wavwriter.h"
#include "mp4mux.h"
#include "progressbar.h"
#ifndef MAX_OUTPUT_CHANNELS
#define MAX_OUTPUT_CHANNELS 24
#endif
#ifndef MAX_BITS_PER_SAMPLE
#define MAX_BITS_PER_SAMPLE 4
#endif

typedef struct {
  FILE *wav;
  uint32_t data_length;

  int format;
  int sample_rate;
  int bits_per_sample;
  int channels;
  int byte_rate;
  int block_align;
  int endianness;

  int streamed;
} wav_reader_s;

void print_usage(char *argv[]) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "-profile   : <0/1(simpe/base)>\n");
  fprintf(stderr,
          "-codec     : <codec name/frame size(opus,aac,flac,pcm/1024)>\n");
  fprintf(stderr, "-i         : <input wav file>\n");
  fprintf(stderr,
          "-config    : <iamf config file(simple profile: iamf_config_s.json, "
          "base profile: iamf_config_b.json)>\n");
  fprintf(stderr, "-o         : <0/1/2(bitstream/mp4/fmp4)> <output file>\n");
  fprintf(stderr,
          "Example:\niamfpackager -profile 0 -codec opus -i input.wav -config "
          "iamf_config_s.json -o 0 "
          "simple_profile.iamf\n");
  fprintf(stderr,
          "or\niamfpackager -profile 1 -codec opus -i input1.wav -i input2.wav "
          "-config iamf_config_b.json "
          "-o 1 base_profile.mp4\n");
}

static void mix_presentation_free(MixPresentation *mixp) {
  for (int i = 0; i < mixp->num_audio_elements; i++) {
    if (mixp->element_mix_config[i].num_parameter_blks > 0) {
      for (int j = 0; j < mixp->element_mix_config[i].num_parameter_blks; j++) {
        if (mixp->element_mix_config[i].subblock_duration[j])
          free(mixp->element_mix_config[i].subblock_duration[j]);
        if (mixp->element_mix_config[i].animated_parameter_data[j])
          free(mixp->element_mix_config[i].animated_parameter_data[j]);
      }
      if (mixp->element_mix_config[i].duration)
        free(mixp->element_mix_config[i].duration);
      if (mixp->element_mix_config[i].num_subblocks)
        free(mixp->element_mix_config[i].num_subblocks);
      if (mixp->element_mix_config[i].constant_subblock_duration)
        free(mixp->element_mix_config[i].constant_subblock_duration);
      if (mixp->element_mix_config[i].subblock_duration)
        free(mixp->element_mix_config[i].subblock_duration);
      if (mixp->element_mix_config[i].animated_parameter_data)
        free(mixp->element_mix_config[i].animated_parameter_data);
    }
  }

  if (mixp->output_mix_config.num_parameter_blks > 0) {
    for (int j = 0; j < mixp->output_mix_config.num_parameter_blks; j++) {
      if (mixp->output_mix_config.subblock_duration[j])
        free(mixp->output_mix_config.subblock_duration[j]);
      if (mixp->output_mix_config.animated_parameter_data[j])
        free(mixp->output_mix_config.animated_parameter_data[j]);
    }
    if (mixp->output_mix_config.duration)
      free(mixp->output_mix_config.duration);
    if (mixp->output_mix_config.num_subblocks)
      free(mixp->output_mix_config.num_subblocks);
    if (mixp->output_mix_config.constant_subblock_duration)
      free(mixp->output_mix_config.constant_subblock_duration);
    if (mixp->output_mix_config.subblock_duration)
      free(mixp->output_mix_config.subblock_duration);
    if (mixp->output_mix_config.animated_parameter_data)
      free(mixp->output_mix_config.animated_parameter_data);
  }
}

static void mix_gain_config_free(MixGainConfig *mix_gain) {
  if (mix_gain->num_parameter_blks > 0) {
    for (int j = 0; j < mix_gain->num_parameter_blks; j++) {
      if (mix_gain->subblock_duration[j]) free(mix_gain->subblock_duration[j]);
      if (mix_gain->animated_parameter_data[j])
        free(mix_gain->animated_parameter_data[j]);
    }
    if (mix_gain->duration) free(mix_gain->duration);
    if (mix_gain->num_subblocks) free(mix_gain->num_subblocks);
    if (mix_gain->constant_subblock_duration)
      free(mix_gain->constant_subblock_duration);
    if (mix_gain->subblock_duration) free(mix_gain->subblock_duration);
    if (mix_gain->animated_parameter_data)
      free(mix_gain->animated_parameter_data);
  }
}

static void parameter_block_split2(MixGainConfig *mix_gain,
                                   MixGainConfig *mix_gain_src,
                                   uint64_t frame_size, int input_sample_r) {
  mix_gain->default_mix_gain = mix_gain_src->default_mix_gain;
  mix_gain->parameter_rate = mix_gain_src->parameter_rate;
  if (mix_gain_src->num_parameter_blks > 0) {
    int blk_index = 0;
    frame_size = mix_gain_src->parameter_rate * frame_size / input_sample_r;
    uint64_t total_duration = 0;
    for (int i = 0; i < mix_gain_src->num_subblocks[blk_index]; i++) {
      total_duration += mix_gain_src->subblock_duration[blk_index][i];
    }
    mix_gain->num_parameter_blks = total_duration / frame_size;
    if (total_duration % frame_size > 0) mix_gain->num_parameter_blks++;

    mix_gain->duration =
        (uint64_t *)malloc(mix_gain->num_parameter_blks * sizeof(uint64_t));
    mix_gain->num_subblocks =
        (uint64_t *)malloc(mix_gain->num_parameter_blks * sizeof(uint64_t));
    mix_gain->constant_subblock_duration =
        (uint64_t *)malloc(mix_gain->num_parameter_blks * sizeof(uint64_t));
    mix_gain->subblock_duration =
        (uint64_t **)malloc(mix_gain->num_parameter_blks * sizeof(uint64_t *));
    mix_gain->animated_parameter_data = (AnimatedParameterData **)malloc(
        mix_gain->num_parameter_blks * sizeof(AnimatedParameterData *));

    if (mix_gain_src->animated_parameter_data[blk_index][0].animation_type ==
        ANIMATION_TYPE_STEP) {
      for (int i = 0; i < mix_gain->num_parameter_blks; i++) {
        mix_gain->num_subblocks[i] = 1;
        mix_gain->subblock_duration[i] =
            (uint64_t *)malloc(mix_gain->num_subblocks[i] * sizeof(uint64_t));
        mix_gain->animated_parameter_data[i] = (AnimatedParameterData *)malloc(
            mix_gain->num_subblocks[i] * sizeof(AnimatedParameterData));
        for (int j = 0; j < mix_gain->num_subblocks[i]; j++)
          memcpy(&mix_gain->animated_parameter_data[i][j],
                 &mix_gain_src->animated_parameter_data[blk_index][0],
                 sizeof(mix_gain_src->animated_parameter_data[blk_index][0]));
      }
    } else if (mix_gain_src->animated_parameter_data[blk_index][0]
                   .animation_type == ANIMATION_TYPE_LINEAR) {
      uint64_t start = 0, stop = 0, duration = 0;
      int parameter_blk = 0;
      int num_subblocks = 1;
      int subblocks = 0;
      uint64_t pre_sub_duration = 0;
      for (int i = 0; i < mix_gain_src->num_subblocks[blk_index]; i++) {
        duration = mix_gain_src->subblock_duration[blk_index][i];
        float n = 0.0f;
        float n2 = duration;
        float p0 = mix_gain_src->animated_parameter_data[blk_index][i]
                       .linear_parameter_data.start_point_value;
        float p1 = mix_gain_src->animated_parameter_data[blk_index][i]
                       .linear_parameter_data.end_point_value;
        float a = 0.0f;
        while (duration > 0) {
          float start_point_value = 0.0f;
          float end_point_value = 0.0f;
          if (duration < frame_size &&
              i < mix_gain_src->num_subblocks[blk_index] - 1) {
            num_subblocks = 2;
            mix_gain->subblock_duration[parameter_blk] =
                (uint64_t *)malloc(num_subblocks * sizeof(uint64_t));
            mix_gain->animated_parameter_data[parameter_blk] =
                (AnimatedParameterData *)malloc(num_subblocks *
                                                sizeof(AnimatedParameterData));

            mix_gain->duration[parameter_blk] = frame_size;
            mix_gain->num_subblocks[parameter_blk] = num_subblocks;
            mix_gain->constant_subblock_duration[parameter_blk] = 0;
            stop += duration;

            mix_gain->subblock_duration[parameter_blk][subblocks] =
                pre_sub_duration = duration;
          } else {
            mix_gain->duration[parameter_blk] = frame_size;
            mix_gain->num_subblocks[parameter_blk] = num_subblocks;

            if (num_subblocks > 1) {
              mix_gain->constant_subblock_duration[parameter_blk] = 0;
              mix_gain->subblock_duration[parameter_blk][subblocks] =
                  frame_size - pre_sub_duration;
              stop += frame_size - pre_sub_duration;
            } else {
              mix_gain->subblock_duration[parameter_blk] =
                  (uint64_t *)malloc(num_subblocks * sizeof(uint64_t));
              mix_gain->animated_parameter_data[parameter_blk] =
                  (AnimatedParameterData *)malloc(
                      num_subblocks * sizeof(AnimatedParameterData));
              mix_gain->constant_subblock_duration[parameter_blk] = frame_size;
              mix_gain->subblock_duration[parameter_blk][subblocks] =
                  frame_size;
              stop += frame_size;
              pre_sub_duration = 0;
            }
          }

          mix_gain->animated_parameter_data[parameter_blk][subblocks]
              .animation_type = ANIMATION_TYPE_LINEAR;
          n = start;
          a = n / n2;
          if (a > 1.0f) a = 1.0f;
          mix_gain->animated_parameter_data[parameter_blk][subblocks]
              .linear_parameter_data.start_point_value = (1 - a) * p0 + a * p1;

          n = stop;
          a = n / n2;
          if (a > 1.0f) a = 1.0f;
          mix_gain->animated_parameter_data[parameter_blk][subblocks]
              .linear_parameter_data.end_point_value = (1 - a) * p0 + a * p1;

          start = stop;
          if (duration < frame_size)
            duration = 0;
          else
            duration -= (frame_size - pre_sub_duration);

          subblocks++;
          if (subblocks == num_subblocks) {
            parameter_blk++;
            num_subblocks = 1;
            subblocks = 0;
          }
        }
      }
    } else if (mix_gain_src->animated_parameter_data[blk_index][0]
                   .animation_type == ANIMATION_TYPE_BEZIER) {
      for (int i = 0; i < mix_gain->num_parameter_blks; i++) {
        mix_gain->duration[i] = frame_size;
        mix_gain->num_subblocks[i] = 1;
        mix_gain->constant_subblock_duration[i] = frame_size;
        mix_gain->subblock_duration[i] =
            (uint64_t *)malloc(mix_gain->num_subblocks[i] * sizeof(uint64_t));
        mix_gain->animated_parameter_data[i] = (AnimatedParameterData *)malloc(
            mix_gain->num_subblocks[i] * sizeof(AnimatedParameterData));
      }
      int64_t alpha, beta, gamma, n0, n1, n2;
      n0 = 0;
      n1 = mix_gain_src->animated_parameter_data[blk_index][0]
               .bezier_parameter_data.control_point_relative_time *
           total_duration;
      n2 = total_duration;
      alpha = n0 - 2 * n1 + n2;
      beta = 2 * (n1 - n0);

      float p0 = mix_gain_src->animated_parameter_data[blk_index][0]
                     .bezier_parameter_data.start_point_value;
      float p1 = mix_gain_src->animated_parameter_data[blk_index][0]
                     .bezier_parameter_data.control_point_value;
      float p2 = mix_gain_src->animated_parameter_data[blk_index][0]
                     .bezier_parameter_data.end_point_value;
      float a = 0.0f;
      uint64_t start_m = 0;
      uint64_t sub_duration = 0;
      for (int i = 0; i < mix_gain->num_parameter_blks; i++) {
        for (int j = 0; j < mix_gain->num_subblocks[i]; j++) {
          mix_gain->animated_parameter_data[i][j].animation_type =
              ANIMATION_TYPE_LINEAR;
          gamma = n0 - (mix_gain->constant_subblock_duration[i] * j + start_m);
          if (alpha == 0) {
            a = -gamma;
            a /= beta;
          } else
            a = (-beta + sqrt(beta * beta - 4 * alpha * gamma)) / (2 * alpha);
          mix_gain->animated_parameter_data[i][j]
              .linear_parameter_data.start_point_value =
              (1 - a) * (1 - a) * p0 + 2 * (1 - a) * a * p1 + a * a * p2;

          gamma = n0 -
                  (mix_gain->constant_subblock_duration[i] * (j + 1) + start_m);

          if (alpha == 0) {
            a = -gamma;
            a /= beta;
          } else
            a = (-beta + sqrt(beta * beta - 4 * alpha * gamma)) / (2 * alpha);
          mix_gain->animated_parameter_data[i][j]
              .linear_parameter_data.end_point_value =
              (1 - a) * (1 - a) * p0 + 2 * (1 - a) * a * p1 + a * a * p2;
          sub_duration += mix_gain->constant_subblock_duration[i];
        }
        start_m += sub_duration;
      }
    }
  }
}

typedef struct MixPresentation_t {
  MixPresentation mixp;
  int num_audio_elements;
  int element_index[2];

  MixGainConfig element_mix[2];
  MixGainConfig output_mix;
} MixPresentation_t;

typedef struct UserInputOption {
  int mix_num;
  MixPresentation_t mixp_t[10];
  int elem_num;
  AudioElementConfig element_config[2];

} UserInputOption;

static char *sound_system_str[] = {
    {"SOUND_SYSTEM_A"},        // 0+2+0, 0
    {"SOUND_SYSTEM_B"},        // 0+5+0, 1
    {"SOUND_SYSTEM_C"},        // 2+5+0, 1
    {"SOUND_SYSTEM_D"},        // 4+5+0, 1
    {"SOUND_SYSTEM_E"},        // 4+5+1, 1
    {"SOUND_SYSTEM_F"},        // 3+7+0, 2
    {"SOUND_SYSTEM_G"},        // 4+9+0, 1
    {"SOUND_SYSTEM_H"},        // 9+10+3,2
    {"SOUND_SYSTEM_I"},        // 0+7+0, 1
    {"SOUND_SYSTEM_J"},        // 4+7+0, 1
    {"SOUND_SYSTEM_EXT_712"},  // 2+7+0, 1
    {"SOUND_SYSTEM_EXT_312"},  // 2+3+0, 1
    {"SOUND_SYSTEM_MONO"},     // 0+1+0, 0
};

static int get_sound_system_by_string(char *sound_system) {
  for (int i = 0; i < SOUND_SYSTEM_END; i++) {
    if (!strcmp(sound_system_str[i], sound_system)) return i;
  }
  return -1;
}

static void parse_mix_repsentation(cJSON *json, MixPresentation_t *mixp_t) {
  MixPresentation *mixp = &(mixp_t->mixp);
  cJSON *json_mix = json;
  mixp->count_label = cJSON_GetObjectItem(json_mix, "count_label")->valueint;
  cJSON *json_languages = cJSON_GetObjectItem(json_mix, "language_labels");
  for (int label = 0; label < mixp->count_label; label++) {
    cJSON *json_language = cJSON_GetArrayItem(json_languages, label);
    mixp->language_label[label] = json_language->valuestring;
  }
  cJSON *json_presentation_annotations =
      cJSON_GetObjectItem(json_mix, "presentation_annotations");
  for (int label = 0; label < mixp->count_label; label++) {
    cJSON *json_mix_presentation_anotation =
        cJSON_GetArrayItem(json_presentation_annotations, label);
    mixp->mix_presentation_annotations[label].mix_presentation_friendly_label =
        cJSON_GetObjectItem(json_mix_presentation_anotation,
                            "mix_presentation_friendly_label")
            ->valuestring;
  }
  mixp->num_sub_mixes =
      cJSON_GetObjectItem(json_mix, "num_sub_mixes")->valueint;
  cJSON *json_mix_submix = cJSON_GetObjectItem(json_mix, "sub_mixes");
  mixp->num_audio_elements =
      cJSON_GetObjectItem(json_mix_submix, "num_audio_elements")->valueint;
  mixp_t->num_audio_elements = mixp->num_audio_elements;

  cJSON *json_elements = cJSON_GetObjectItem(json_mix_submix, "audio_elements");
  for (int ele = 0; ele < mixp->num_audio_elements; ele++) {
    cJSON *json_element = cJSON_GetArrayItem(json_elements, ele);
    int element_index =
        cJSON_GetObjectItem(json_element, "element_index")->valueint;
    mixp_t->element_index[ele] = element_index;
    cJSON *json_element_annotations =
        cJSON_GetObjectItem(json_element, "element_annotations");
    for (int label = 0; label < mixp->count_label; label++) {
      cJSON *json_mix_element_anotation =
          cJSON_GetArrayItem(json_element_annotations, label);
      mixp->mix_presentation_element_annotations[ele][label]
          .audio_element_friendly_label =
          cJSON_GetObjectItem(json_mix_element_anotation,
                              "audio_element_friendly_label")
              ->valuestring;
    }
    cJSON *json_element_config =
        cJSON_GetObjectItem(json_element, "element_mix_config");
    cJSON *json_element_mix_gain =
        cJSON_GetObjectItem(json_element_config, "element_mix_gain");
    cJSON *json_element_para =
        cJSON_GetObjectItem(json_element_mix_gain, "param_definition");
    mixp_t->element_mix[element_index].parameter_rate =
        cJSON_GetObjectItem(json_element_para, "parameter_rate")->valueint;

    cJSON *json_element_parameter_blk =
        cJSON_GetObjectItem(json_element_mix_gain, "parameter_block_metadata");
    if (json_element_parameter_blk) {
      int num_parameter_blks = 1;
      mixp_t->element_mix[element_index].num_parameter_blks = 1;
      mixp_t->element_mix[element_index].duration =
          (uint64_t *)malloc(num_parameter_blks * sizeof(uint64_t));
      mixp_t->element_mix[element_index].num_subblocks =
          (uint64_t *)malloc(num_parameter_blks * sizeof(uint64_t));
      mixp_t->element_mix[element_index].constant_subblock_duration =
          (uint64_t *)malloc(num_parameter_blks * sizeof(uint64_t));
      mixp_t->element_mix[element_index].subblock_duration =
          (uint64_t **)malloc(num_parameter_blks * sizeof(uint64_t *));
      mixp_t->element_mix[element_index].animated_parameter_data =
          (AnimatedParameterData **)malloc(num_parameter_blks *
                                           sizeof(AnimatedParameterData *));

      cJSON *json_subblocks =
          cJSON_GetObjectItem(json_element_parameter_blk, "subblocks");
      uint64_t num_subblocks = cJSON_GetArraySize(json_subblocks);
      mixp_t->element_mix[element_index].num_subblocks[0] = num_subblocks;
      mixp_t->element_mix[element_index].subblock_duration[0] =
          (uint64_t *)malloc(num_subblocks * sizeof(uint64_t));
      mixp_t->element_mix[element_index].animated_parameter_data[0] =
          (AnimatedParameterData *)malloc(num_subblocks *
                                          sizeof(AnimatedParameterData));
      for (int block = 0; block < num_subblocks; block++) {
        cJSON *json_subblock = cJSON_GetArrayItem(json_subblocks, block);
        mixp_t->element_mix[element_index].subblock_duration[0][block] =
            cJSON_GetObjectItem(json_subblock, "subblock_duration")->valueint;
        printf(
            "animation_type: %s\n",
            cJSON_GetObjectItem(json_subblock, "animation_type")->valuestring);
        if (!strcmp(cJSON_GetObjectItem(json_subblock, "animation_type")
                        ->valuestring,
                    "STEP")) {
          mixp_t->element_mix[element_index]
              .animated_parameter_data[0][block]
              .animation_type = ANIMATION_TYPE_STEP;
        } else if (!strcmp(cJSON_GetObjectItem(json_subblock, "animation_type")
                               ->valuestring,
                           "LINEAR"))
          mixp_t->element_mix[element_index]
              .animated_parameter_data[0][block]
              .animation_type = ANIMATION_TYPE_LINEAR;
        else if (!strcmp(cJSON_GetObjectItem(json_subblock, "animation_type")
                             ->valuestring,
                         "BEZIER"))
          mixp_t->element_mix[element_index]
              .animated_parameter_data[0][block]
              .animation_type = ANIMATION_TYPE_BEZIER;

        mixp_t->element_mix[element_index]
            .animated_parameter_data[0][block]
            .bezier_parameter_data.start_point_value =
            cJSON_GetObjectItem(json_subblock, "start_point_value")
                ->valuedouble;
        mixp_t->element_mix[element_index]
            .animated_parameter_data[0][block]
            .bezier_parameter_data.end_point_value =
            cJSON_GetObjectItem(json_subblock, "end_point_value")->valuedouble;
        mixp_t->element_mix[element_index]
            .animated_parameter_data[0][block]
            .bezier_parameter_data.control_point_value =
            cJSON_GetObjectItem(json_subblock, "control_point_value")
                ->valuedouble;
        mixp_t->element_mix[element_index]
            .animated_parameter_data[0][block]
            .bezier_parameter_data.control_point_relative_time =
            cJSON_GetObjectItem(json_subblock, "control_point_relative_time")
                ->valuedouble;
      }
    }
    mixp_t->element_mix[element_index].default_mix_gain =
        cJSON_GetObjectItem(json_element_mix_gain, "default_mix_gain")
            ->valuedouble;
  }
  cJSON *json_output_mix_config =
      cJSON_GetObjectItem(json_mix_submix, "output_mix_config");
  cJSON *json_out_mix_gain =
      cJSON_GetObjectItem(json_output_mix_config, "output_mix_gain");
  cJSON *json_out_mix_para =
      cJSON_GetObjectItem(json_out_mix_gain, "param_definition");
  mixp_t->output_mix.parameter_rate =
      cJSON_GetObjectItem(json_out_mix_para, "parameter_rate")->valueint;

  cJSON *json_output_parameter_blk =
      cJSON_GetObjectItem(json_out_mix_gain, "parameter_block_metadata");
  if (json_output_parameter_blk) {
    int num_parameter_blks = 1;
    mixp_t->output_mix.num_parameter_blks = 1;
    mixp_t->output_mix.duration =
        (uint64_t *)malloc(num_parameter_blks * sizeof(uint64_t));
    mixp_t->output_mix.num_subblocks =
        (uint64_t *)malloc(num_parameter_blks * sizeof(uint64_t));
    mixp_t->output_mix.constant_subblock_duration =
        (uint64_t *)malloc(num_parameter_blks * sizeof(uint64_t));
    mixp_t->output_mix.subblock_duration =
        (uint64_t **)malloc(num_parameter_blks * sizeof(uint64_t *));
    mixp_t->output_mix.animated_parameter_data =
        (AnimatedParameterData **)malloc(num_parameter_blks *
                                         sizeof(AnimatedParameterData *));

    cJSON *json_subblocks =
        cJSON_GetObjectItem(json_output_parameter_blk, "subblocks");
    uint64_t num_subblocks = cJSON_GetArraySize(json_subblocks);
    mixp_t->output_mix.num_subblocks[0] = num_subblocks;
    mixp_t->output_mix.subblock_duration[0] =
        (uint64_t *)malloc(num_subblocks * sizeof(uint64_t));
    mixp_t->output_mix.animated_parameter_data[0] =
        (AnimatedParameterData *)malloc(num_subblocks *
                                        sizeof(AnimatedParameterData));
    for (int block = 0; block < num_subblocks; block++) {
      cJSON *json_subblock = cJSON_GetArrayItem(json_subblocks, block);
      mixp_t->output_mix.subblock_duration[0][block] =
          cJSON_GetObjectItem(json_subblock, "subblock_duration")->valueint;
      printf("animation_type: %s\n",
             cJSON_GetObjectItem(json_subblock, "animation_type")->valuestring);
      if (!strcmp(
              cJSON_GetObjectItem(json_subblock, "animation_type")->valuestring,
              "STEP")) {
        mixp_t->output_mix.animated_parameter_data[0][block].animation_type =
            ANIMATION_TYPE_STEP;
      } else if (!strcmp(cJSON_GetObjectItem(json_subblock, "animation_type")
                             ->valuestring,
                         "LINEAR"))
        mixp_t->output_mix.animated_parameter_data[0][block].animation_type =
            ANIMATION_TYPE_LINEAR;
      else if (!strcmp(cJSON_GetObjectItem(json_subblock, "animation_type")
                           ->valuestring,
                       "BEZIER"))
        mixp_t->output_mix.animated_parameter_data[0][block].animation_type =
            ANIMATION_TYPE_BEZIER;

      mixp_t->output_mix.animated_parameter_data[0][block]
          .bezier_parameter_data.start_point_value =
          cJSON_GetObjectItem(json_subblock, "start_point_value")->valuedouble;
      mixp_t->output_mix.animated_parameter_data[0][block]
          .bezier_parameter_data.end_point_value =
          cJSON_GetObjectItem(json_subblock, "end_point_value")->valuedouble;
      mixp_t->output_mix.animated_parameter_data[0][block]
          .bezier_parameter_data.control_point_value =
          cJSON_GetObjectItem(json_subblock, "control_point_value")
              ->valuedouble;
      mixp_t->output_mix.animated_parameter_data[0][block]
          .bezier_parameter_data.control_point_relative_time =
          cJSON_GetObjectItem(json_subblock, "control_point_relative_time")
              ->valuedouble;
    }
  }
  mixp_t->output_mix.default_mix_gain =
      cJSON_GetObjectItem(json_out_mix_gain, "default_mix_gain")->valuedouble;

  mixp->num_layouts =
      cJSON_GetObjectItem(json_mix_submix, "num_layouts")->valueint;
  cJSON *json_layouts = cJSON_GetObjectItem(json_mix_submix, "layouts");
  for (int layouts = 0; layouts < mixp->num_layouts; layouts++) {
    cJSON *json_loudness_layout = cJSON_GetArrayItem(json_layouts, layouts);
    ;
    cJSON *json_ss_layout =
        cJSON_GetObjectItem(json_loudness_layout, "ss_layout");
    mixp->loudness_layout[layouts].layout_type =
        IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
    mixp->loudness_layout[layouts].sound_system = get_sound_system_by_string(
        cJSON_GetObjectItem(json_ss_layout, "sound_system")->valuestring);
  }
}

static void parse_audio_element(cJSON *json, AudioElementConfig *ae_conf) {
  cJSON *json_ae = json;
  if (!strcmp(cJSON_GetObjectItem(json_ae, "audio_element_type")->valuestring,
              "AUDIO_ELEMENT_CHANNEL_BASED")) {
    ae_conf->element_type = AUDIO_ELEMENT_CHANNEL_BASED;
  } else if (!strcmp(cJSON_GetObjectItem(json_ae, "audio_element_type")
                         ->valuestring,
                     "AUDIO_ELEMENT_SCENE_BASED")) {
    ae_conf->element_type = AUDIO_ELEMENT_SCENE_BASED;
  }
  if (ae_conf->element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    cJSON *json_channel_base =
        cJSON_GetObjectItem(json_ae, "scalable_channel_layout_config");
    int index = 0;
    char *channel_layout_string[] = {"1.0.0", "2.0.0",   "5.1.0", "5.1.2",
                                     "5.1.4", "7.1.0",   "7.1.2", "7.1.4",
                                     "3.1.2", "binaural"};
    char mode[128];
    strcpy(mode, cJSON_GetObjectItem(json_channel_base, "mode")->valuestring);
    char *p_start = mode;
    ae_conf->layout_in = IA_CHANNEL_LAYOUT_COUNT;
    for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
      if (!strncmp(channel_layout_string[i], p_start,
                   strlen(channel_layout_string[i]))) {
        fprintf(stderr, "\nInput channel layout:%s\n",
                channel_layout_string[i]);
        ae_conf->layout_in = i;
        break;
      }
    }
    if (ae_conf->layout_in == IA_CHANNEL_LAYOUT_COUNT) {
      fprintf(stderr, stderr, "Please check input channel layout format\n");
      return 0;
    }

    int channel_cb_ptr = 0;
    fprintf(stderr, "Channel layout combinations: ");
    while (
        channel_cb_ptr <
        strlen(p_start + strlen(channel_layout_string[ae_conf->layout_in]))) {
      for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
        if (!strncmp(channel_layout_string[i], p_start + 6 + channel_cb_ptr,
                     5)) {
          fprintf(stderr, "%s ", channel_layout_string[i]);
          ae_conf->layout_cb[index] = i;
          index++;
        }
      }
      channel_cb_ptr += 6;
    }
    fprintf(stderr, "\n \n");

    ae_conf->layout_cb[index] = IA_CHANNEL_LAYOUT_COUNT;

    cJSON *json_demix = cJSON_GetObjectItem(json_channel_base, "demix");
    if (json_demix) {
      ae_conf->demix.set_mode =
          cJSON_GetObjectItem(json_demix, "set_mode")->valueint;
      ae_conf->demix.default_mode =
          cJSON_GetObjectItem(json_demix, "default_mode")->valueint;
      ae_conf->demix.default_weight =
          cJSON_GetObjectItem(json_demix, "default_weight")->valueint;
    }
  } else if (ae_conf->element_type == AUDIO_ELEMENT_SCENE_BASED) {
    cJSON *json_scene_base = cJSON_GetObjectItem(json_ae, "ambisonics_config");
    if (!strcmp(cJSON_GetObjectItem(json_scene_base, "ambisonics_mode")
                    ->valuestring,
                "AMBISONICS_MODE_MONO"))
      ae_conf->ambisonics_mode = 0;
    else if (!strcmp(cJSON_GetObjectItem(json_scene_base, "ambisonics_mode")
                         ->valuestring,
                     "AMBISONICS_MODE_PROJECTION"))
      ae_conf->ambisonics_mode = 1;

    if (ae_conf->ambisonics_mode == 0) {
      cJSON *json_mono =
          cJSON_GetObjectItem(json_scene_base, "ambisonics_mono_config");
      ae_conf->ambisonics_mono_config.output_channel_count =
          cJSON_GetObjectItem(json_mono, "output_channel_count")->valueint;
      ae_conf->ambisonics_mono_config.substream_count =
          cJSON_GetObjectItem(json_mono, "substream_count")->valueint;
      cJSON *json_channel_mapping =
          cJSON_GetObjectItem(json_mono, "channel_mapping");
      if (json_channel_mapping) {
        int child_num = cJSON_GetArraySize(json_channel_mapping);
        // printf("child_num: %d\n", child_num);
        for (int i = 0; i < child_num; i++) {
          ae_conf->ambisonics_mono_config.channel_mapping[i] =
              cJSON_GetArrayItem(json_channel_mapping, i)->valueint;
        }
      }
    } else if (ae_conf->ambisonics_mode == 1) {
      cJSON *json_projection =
          cJSON_GetObjectItem(json_scene_base, "ambisonics_projection_config");
      ae_conf->ambisonics_projection_config.output_channel_count =
          cJSON_GetObjectItem(json_projection, "output_channel_count")
              ->valueint;
      ae_conf->ambisonics_projection_config.substream_count =
          cJSON_GetObjectItem(json_projection, "substream_count")->valueint;
      ae_conf->ambisonics_projection_config.coupled_substream_count =
          cJSON_GetObjectItem(json_projection, "coupled_substream_count")
              ->valueint;
      cJSON *json_demixing_matrix =
          cJSON_GetObjectItem(json_projection, "demixing_matrix");
      if (json_demixing_matrix) {
        int child_num = cJSON_GetArraySize(json_demixing_matrix);
        // printf("child_num: %d\n", child_num);
        for (int i = 0; i < child_num; i++) {
          ae_conf->ambisonics_projection_config.demixing_matrix[i] =
              cJSON_GetArrayItem(json_demixing_matrix, i)->valueint;
        }
      }
    }
  }
}

static void paser_user_input_option(cJSON *json, UserInputOption *user_in) {
  int child_num = cJSON_GetArraySize(json);
  for (int i = 0; i < child_num; i++) {
    cJSON *json_c = cJSON_GetArrayItem(json, i);
    // printf("parsed child json: %d\n", i);
    if (!strcmp(json_c->string, "audio_element_metadata")) {
      parse_audio_element(json_c,
                          &(user_in->element_config[user_in->elem_num]));
      user_in->elem_num++;
    } else if (!strcmp(json_c->string, "mix_presentation_metadata")) {
      parse_mix_repsentation(json_c, &(user_in->mixp_t[user_in->mix_num]));
      user_in->mix_num++;
    }
  }
}

int iamf_simple_profile_test(int argc, char *argv[]) {
  if (argc < 10) {
    print_usage(argv);
    return 0;
  }

  char *channel_layout_string[] = {"1.0.0", "2.0.0",   "5.1.0", "5.1.2",
                                   "5.1.4", "7.1.0",   "7.1.2", "7.1.4",
                                   "3.1.2", "binaural"};

  IAChannelLayoutType channel_layout_in = IA_CHANNEL_LAYOUT_COUNT;
  IAChannelLayoutType channel_layout_cb[IA_CHANNEL_LAYOUT_COUNT];

  int is_standalone = 0;
  int profile = 0;
  int codec_id = IAMF_CODEC_UNKNOWN;
  AudioElementType audio_element_type = AUDIO_ELEMENT_INVALID;
  int ambisonics_mode = 0;
  int is_fragment_mp4 = 0;
  char *in_file = NULL, *out_file = NULL;
  int args = 1;
  int index = 0;
  int chunk_size = 0;

  int measured_target[10];
  int num_layouts = 0;
  AudioElementConfig element_config;
  char *config_file = NULL;

  while (args < argc) {
    if (argv[args][0] == '-') {
      if (strcmp(argv[args], "-codec") == 0) {
        args++;
        if (!strncmp(argv[args], "opus", 4)) {
          codec_id = IAMF_CODEC_OPUS;
          if (strlen(argv[args]) > 4) chunk_size = atoi(argv[args] + 5);
        } else if (!strncmp(argv[args], "aac", 3)) {
          codec_id = IAMF_CODEC_AAC;
          if (strlen(argv[args]) > 3) chunk_size = atoi(argv[args] + 4);
        } else if (!strncmp(argv[args], "flac", 4)) {
          codec_id = IAMF_CODEC_FLAC;
          if (strlen(argv[args]) > 4) chunk_size = atoi(argv[args] + 5);
        } else if (!strncmp(argv[args], "pcm", 3)) {
          codec_id = IAMF_CODEC_PCM;
          if (strlen(argv[args]) > 3) chunk_size = atoi(argv[args] + 4);
        }
      } else if (strcmp(argv[args], "-config") == 0) {
        args++;
        config_file = argv[args];
      } else if (strcmp(argv[args], "-profile") == 0) {
        args++;
        profile = atoi(argv[args]);
      } else if (argv[args][1] == 'i') {
        args++;
        in_file = argv[args];
      } else if (argv[args][1] == 'o') {
        args++;
        if (!strncmp("0", argv[args], 1))
          is_standalone = 1;
        else if (!strncmp("1", argv[args], 1)) {
          is_standalone = 0;
        } else if (!strncmp("2", argv[args], 1)) {
          is_standalone = 0;
          is_fragment_mp4 = 1;
        }
        args++;
        out_file = argv[args];
      } else {
        args++;
      }
    }
    args++;
  }

  // mix config json parse
  FILE *fp = fopen(config_file, "r");
  if (fp == NULL) {
    printf("Error opening file: %s\n", config_file);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  int file_size = ftell(fp);
  rewind(fp);

  char *json_string = malloc(file_size);
  fread(json_string, 1, file_size, fp);
  fclose(fp);

  cJSON *json = cJSON_Parse(json_string);
  if (!json) {
    printf("Error before: [%s]\n", cJSON_GetErrorPtr());
    free(json_string);
    return 1;
  }
  free(json_string);
  UserInputOption user_in;
  memset(&user_in, 0x00, sizeof(UserInputOption));
  paser_user_input_option(json, &user_in);

  MOVMuxContext *movm = NULL;
  FILE *iamf_file = NULL;

  if (is_standalone == 0) {
    if ((movm = mov_write_open(out_file)) == NULL) {
      fprintf(stderr, "Couldn't create MP4 output file.\n");
      return (0);
    }

    //********************** dep codec test***************************//
    movm->codec_id = codec_id;
    //********************** dep codec test***************************//

    //********************** fragment mp4 test***************************//
    if (is_fragment_mp4) {
      movm->flags = IA_MOV_FLAG_FRAGMENT;
      movm->max_fragment_duration = 10000;  // ms
    }
    //********************** fragment mp4 test***************************//

    if (mov_write_trak(movm, 0, 1) < 0) {
      fprintf(stderr, "Couldn't create media track.\n");
      exit(-1);
    }
  } else {
    if (!(iamf_file = fopen(out_file, "wb"))) {
      fprintf(stderr, "Couldn't create output file.\n");
      return (0);
    }
  }

  //////////////////////read PCM data from wav[start]///////////////////////////
  void *in_wavf = NULL;
  char in_wav[255] = {0};
  if (strlen(in_file) < 255) strncpy(in_wav, in_file, strlen(in_file));
  // sprintf(in_wav, "%s.wav", in_file);
  in_wavf = dep_wav_read_open(in_wav);
  if (!in_wavf) {
    fprintf(stderr, "Could not open input file %s\n", in_wav);
    goto failure;
  }
  int size;
  int format;
  int channels;
  int sample_rate;
  int bits_per_sample;
  int endianness;
  unsigned int data_length;

  dep_wav_get_header(in_wavf, &format, &channels, &sample_rate,
                     &bits_per_sample, &endianness, &data_length);
  if (in_wavf) dep_wav_read_close(in_wavf);
  fprintf(stderr,
          "input wav: format[%d] channels[%d] sample_rate[%d] "
          "bits_per_sample[%d] endianness[%s] data_length[%d]\n",
          format, channels, sample_rate, bits_per_sample,
          (endianness == 1) ? "little-endian" : "big-endian", data_length);

  if (chunk_size == 0) {
    if (codec_id == IAMF_CODEC_OPUS)
      chunk_size = 960;
    else if (codec_id == IAMF_CODEC_AAC)
      chunk_size = 1024;
    else if (codec_id == IAMF_CODEC_FLAC)
      chunk_size = 1024;
    else if (codec_id == IAMF_CODEC_PCM)
      chunk_size = 960;
  }

  int bsize_of_samples = chunk_size * channels * bits_per_sample / 8;
  int bsize_of_1sample = channels * bits_per_sample / 8;
  int bsize_of_1ch_float_samples = chunk_size * sizeof(float);
  int bsize_of_float_samples = chunk_size * channels * sizeof(float);
  unsigned char *wav_samples =
      (unsigned char *)malloc(bsize_of_samples * sizeof(unsigned char));

  clock_t start_t = 0, stop_t = 0;
  /**
   * 1. Create immersive encoder handle.
   * */
  int error = 0;
  IAMF_Encoder *ia_enc =
      IAMF_encoder_create(sample_rate, bits_per_sample, endianness,
                          codec_id,  // 1:opus, 2:aac
                          chunk_size, &error);
  int element_id = IAMF_audio_element_add(ia_enc, user_in.element_config[0]);

  /**
   * 2. immersive encoder control.
   * */
  // IAMF_encoder_ctl(ia_enc, element_id,
  // IA_SET_RECON_GAIN_FLAG((int)(recon_gain_flag))); IAMF_encoder_ctl(ia_enc,
  // element_id, IA_SET_OUTPUT_GAIN_FLAG((int)output_gain_flag));
  IAMF_encoder_ctl(ia_enc, element_id,
                   IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));
  IAMF_encoder_ctl(ia_enc, element_id, IA_SET_IAMF_PROFILE((int)profile));
  if (user_in.element_config[0].demix.set_mode == 1) {
    IAMF_encoder_ctl(ia_enc, element_id,
                     IA_SET_IAMF_DEFAULT_DEMIX_MODE(
                         (int)user_in.element_config[0].demix.default_mode));
    IAMF_encoder_ctl(ia_enc, element_id,
                     IA_SET_IAMF_DEFAULT_DEMIX_WEIGHT(
                         (int)user_in.element_config[0].demix.default_weight));
  } else if (user_in.element_config[0].demix.set_mode == 2) {
    IAMF_encoder_ctl(ia_enc, element_id, IA_SET_IAMF_DISABLE_DEMIX((int)1));
  }
  int preskip = 0;
  IAMF_encoder_ctl(ia_enc, element_id, IA_GET_LOOKAHEAD(&preskip));

  /**
   * 2. set mix presentation.
   * */
  // Calculate the loudness for preset target layout, if there is no seteo
  // scalable laylout for channel-based case, please set target with s0 at
  // least.
  for (int i = 0; i < user_in.mix_num; i++) {
    user_in.mixp_t[i].mixp.audio_element_id[0] = element_id;
    in_wavf = dep_wav_read_open(in_wav);
    if (user_in.mixp_t[i].mixp.num_layouts > 0) {
      int pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                         bsize_of_samples);
      int count = 0;
      IAMF_encoder_target_loudness_measure_start(ia_enc,
                                                 &(user_in.mixp_t[i].mixp));
      while (pcm_frames) {
        if (pcm_frames <= 0) break;
        count++;
        IAFrame ia_frame;
        memset(&ia_frame, 0x00, sizeof(ia_frame));

        ia_frame.element_id = element_id;
        ia_frame.samples = pcm_frames / bsize_of_1sample;
        ia_frame.pcm = wav_samples;
        IAMF_encoder_target_loudness_measure(ia_enc, &(user_in.mixp_t[i].mixp),
                                             &ia_frame);
        pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                       bsize_of_samples);
      }
      IAMF_encoder_target_loudness_measure_stop(ia_enc,
                                                &(user_in.mixp_t[i].mixp));
    }
    if (in_wavf) dep_wav_read_close(in_wavf);
    parameter_block_split2(&user_in.mixp_t[i].mixp.element_mix_config[0],
                           &user_in.mixp_t[i].element_mix[0], chunk_size,
                           sample_rate);
    parameter_block_split2(&user_in.mixp_t[i].mixp.output_mix_config,
                           &user_in.mixp_t[i].output_mix, chunk_size,
                           sample_rate);

    IAMF_encoder_set_mix_presentation(ia_enc, user_in.mixp_t[i].mixp);
    mix_presentation_free(&(user_in.mixp_t[i].mixp));
    mix_gain_config_free(&user_in.mixp_t[i].element_mix[0]);
    mix_gain_config_free(&user_in.mixp_t[i].output_mix);
  }

  if (user_in.element_config[0].element_type == AUDIO_ELEMENT_SCENE_BASED)
    goto ENCODING;
  /**
   * 3. ASC and HEQ pre-process.
   * */
  in_wavf = dep_wav_read_open(in_wav);
  start_t = clock();
  int pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                     bsize_of_samples);
  IAMF_encoder_dmpd_start(ia_enc, element_id);
  while (pcm_frames) {
    IAMF_encoder_dmpd_process(ia_enc, element_id, wav_samples,
                              pcm_frames / bsize_of_1sample);
    pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                   bsize_of_samples);
  }
  IAMF_encoder_dmpd_stop(ia_enc, element_id);
  stop_t = clock();
  fprintf(stderr, "dmpd total time %f(s)\n",
          (float)(stop_t - start_t) / CLOCKS_PER_SEC);

  if (in_wavf) dep_wav_read_close(in_wavf);

  start_t = clock();
  /**
   * 4. loudness and gain pre-process.
   * */
  in_wavf = dep_wav_read_open(in_wav);
  pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                 bsize_of_samples);
  ProgressBar bar;
  ProgressBarInit(&bar);
  int total, current;

  wav_reader_s *wr;
  bar.startBar(&bar, "Loudness", 0);
  total = data_length;
  current = 0;
  IAMF_encoder_scalable_loudnessgain_start(ia_enc, element_id);
  while (pcm_frames) {
    IAMF_encoder_scalable_loudnessgain_measure(ia_enc, element_id, wav_samples,
                                               pcm_frames / bsize_of_1sample);

    pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                   bsize_of_samples);
    wr = (wav_reader_s *)in_wavf;
    current = total - wr->data_length;
    float pct = ((float)current / 1000) / ((float)total / 1000) * 100;
    if (bar._x < (int)pct) bar.proceedBar(&bar, (int)pct);
  }
  bar.endBar(&bar, 100);

  IAMF_encoder_scalable_loudnessgain_stop(ia_enc, element_id);

  if (in_wavf) dep_wav_read_close(in_wavf);
  /**
   * 5. calculate recon gain.
   * */
  in_wavf = dep_wav_read_open(in_wav);
  pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                 bsize_of_samples);
  IAMF_encoder_reconstruct_gain_start(ia_enc, element_id);
  while (pcm_frames) {
    IAMF_encoder_reconstruct_gain(ia_enc, element_id, wav_samples,
                                  pcm_frames / bsize_of_1sample);
    pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                   bsize_of_samples);
  }

  if (in_wavf) dep_wav_read_close(in_wavf);
ENCODING:
  /**
   * 6. get immersive audio global descriptor sample group.
   * */
  if (is_standalone == 0) {
    /////////////////////////audio trak setting,
    /// start//////////////////////////////////////////////
    // audio trak
    unsigned char channel_map714[] = {1, 2, 6, 8, 10, 8, 10, 12, 6};
    mov_audio_track *audio_t = movm->audio_trak;
    // audio_t[0].ia_descriptor.size = IAMF_encoder_get_descriptor(
    //    ia_enc, audio_t[0].ia_descriptor.data, 1024);
    audio_t[0].av_descriptor[audio_t[0].descriptor_entries].size =
        IAMF_encoder_get_descriptor(
            ia_enc,
            audio_t[0].av_descriptor[audio_t[0].descriptor_entries].data, 1024);
    audio_t[0].descriptor_entries++;
    /*
    spec, 7.2.3  IA Sample Entry
    Both channelcount and samplerate fields of AudioSampleEntry shall be
    ignored.
    */
    audio_t[0].samplerate = 48000;
    audio_t[0].channels = channel_map714[channel_layout_in];
    audio_t[0].bits = 16;
    // codec specific info
    if (movm->codec_id == IAMF_CODEC_OPUS) {
      audio_t[0].iamf.roll_distance = -4;
    } else if (movm->codec_id == IAMF_CODEC_AAC) {
      audio_t[0].iamf.roll_distance = -1;
    } else {
      fprintf(stderr, "wrong codec input\n");
    }
    audio_t[0].iamf.initial_padding = preskip;
    /////////////////////////audio trak setting,
    /// end//////////////////////////////////////////////

    mov_write_head(movm);
  }

  /**
   * 7. immersive audio encode.
   * */
  uint64_t frame_count = 0;
  uint32_t max_packet_size = MAX_OUTPUT_CHANNELS * chunk_size *
                             MAX_BITS_PER_SAMPLE * 2;  // one audio elements
  unsigned char *encode_ia = malloc(max_packet_size);
  in_wavf = dep_wav_read_open(in_wav);
  pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                 bsize_of_samples);
  int demix_mode = 0;
  while (1) {
    IAFrame ia_frame;
    memset(&ia_frame, 0x00, sizeof(ia_frame));

    /*trimming test
    if (frame_count == 0)
      ia_frame.num_samples_to_trim_at_start = 320;
    */
    ia_frame.element_id = element_id;
    ia_frame.samples = pcm_frames / bsize_of_1sample;
    if (ia_frame.samples > 0)
      ia_frame.pcm = wav_samples;
    else
      ia_frame.pcm = NULL;  // output pending encoded data
    ia_frame.next = NULL;
    IAPacket ia_packet;
    memset(&ia_packet, 0x00, sizeof(ia_packet));
    ia_packet.data = encode_ia;
    if (IAMF_encoder_encode(ia_enc, &ia_frame, &ia_packet, max_packet_size) <
        0) {
      break;  // encoding is end
    }
    if (is_standalone == 0) {
      AVPacket pkt;
      memset(&pkt, 0x00, sizeof(pkt));
      pkt.size = ia_packet.packet_size;
      pkt.buf = ia_packet.data;
      pkt.samples = ia_packet.samples;
      if (pkt.size > 0) mov_write_audio2(movm, 0, &pkt);
      // mov_write_audio(movm, 0, encode_ia, ia_packet.packet_size, pcm_frames /
      // bsize_of_1sample);
    } else {
      fwrite(ia_packet.data, ia_packet.packet_size, 1, iamf_file);
    }

    pcm_frames = dep_wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                   bsize_of_samples);
    frame_count++;
  }

  stop_t = clock();
  fprintf(stderr, "encoding total time %f(s), total count: %" PRId64 "\n",
          (float)(stop_t - start_t) / CLOCKS_PER_SEC, frame_count);

  if (is_standalone == 0) {
    mov_write_tail(movm);
    mov_write_close(movm);
  } else {
    if (iamf_file) fclose(iamf_file);
  }

  /**
   * 8. destroy IAMF encoder handle.
   * */
  IAMF_encoder_destroy(ia_enc);
  if (wav_samples) {
    free(wav_samples);
  }
  if (encode_ia) free(encode_ia);
failure:
  if (in_wavf) dep_wav_read_close(in_wavf);

  cJSON_Delete(json);
  return 0;
}

typedef struct {
  IAMF_Encoder *ia_enc;
  int num_of_elements;
  int element_id[2];

  char *in_file[2];
  void *in_wavf[2];
  int size[2];
  int format[2];
  int channels[2];
  int sample_rate[2];
  int bits_per_sample[2];
  int bsize_of_samples[2];
  int bsize_of_1sample[2];
  unsigned int data_length[2];
  int endianness[2];
  unsigned char *wav_samples[2];

  int num_layouts;
  int measured_target[10];
  AnimatedParameterData animated_parameter[2];
} BaseProfileHandler;

void iamf_pre_process(BaseProfileHandler *handler, int element_index,
                      int element_type) {
  if (element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    /**
     * 4. ASC and HEQ pre-process.
     * */
    handler->in_wavf[element_index] =
        dep_wav_read_open(handler->in_file[element_index]);

    IAMF_encoder_dmpd_start(handler->ia_enc,
                            handler->element_id[element_index]);
    int pcm_frames =
        dep_wav_read_data(handler->in_wavf[element_index],
                          (unsigned char *)handler->wav_samples[element_index],
                          handler->bsize_of_samples[element_index]);
    while (pcm_frames) {
      IAMF_encoder_dmpd_process(
          handler->ia_enc, handler->element_id[element_index],
          handler->wav_samples[element_index],
          pcm_frames / handler->bsize_of_1sample[element_index]);
      pcm_frames = dep_wav_read_data(
          handler->in_wavf[element_index],
          (unsigned char *)handler->wav_samples[element_index],
          handler->bsize_of_samples[element_index]);
    }
    IAMF_encoder_dmpd_stop(handler->ia_enc, handler->element_id[element_index]);

    dep_wav_read_close(handler->in_wavf[element_index]);

    /**
     * 4. loudness and gain pre-process.
     * */
    handler->in_wavf[element_index] =
        dep_wav_read_open(handler->in_file[element_index]);
    pcm_frames =
        dep_wav_read_data(handler->in_wavf[element_index],
                          (unsigned char *)handler->wav_samples[element_index],
                          handler->bsize_of_samples[element_index]);
    ProgressBar bar;
    ProgressBarInit(&bar);
    int total, current;

    wav_reader_s *wr;
    bar.startBar(&bar, "Loudness", 0);
    total = handler->data_length[element_index];
    current = 0;
    IAMF_encoder_scalable_loudnessgain_start(
        handler->ia_enc, handler->element_id[element_index]);
    while (pcm_frames) {
      IAMF_encoder_scalable_loudnessgain_measure(
          handler->ia_enc, handler->element_id[element_index],
          handler->wav_samples[element_index],
          pcm_frames / handler->bsize_of_1sample[element_index]);

      pcm_frames = dep_wav_read_data(
          handler->in_wavf[element_index],
          (unsigned char *)handler->wav_samples[element_index],
          handler->bsize_of_samples[element_index]);
      wr = (wav_reader_s *)handler->in_wavf[element_index];
      current = total - wr->data_length;
      float pct = ((float)current / 1000) / ((float)total / 1000) * 100;
      if (bar._x < (int)pct) bar.proceedBar(&bar, (int)pct);
    }
    bar.endBar(&bar, 100);

    IAMF_encoder_scalable_loudnessgain_stop(handler->ia_enc,
                                            handler->element_id[element_index]);

    dep_wav_read_close(handler->in_wavf[element_index]);

    /**
     * 6. calculate recon gain.
     * */
    handler->in_wavf[element_index] =
        dep_wav_read_open(handler->in_file[element_index]);

    pcm_frames =
        dep_wav_read_data(handler->in_wavf[element_index],
                          (unsigned char *)handler->wav_samples[element_index],
                          handler->bsize_of_samples[element_index]);
    IAMF_encoder_reconstruct_gain_start(handler->ia_enc,
                                        handler->element_id[element_index]);
    while (pcm_frames) {
      IAMF_encoder_reconstruct_gain(
          handler->ia_enc, handler->element_id[element_index],
          handler->wav_samples[element_index],
          pcm_frames / handler->bsize_of_1sample[element_index]);
      pcm_frames = dep_wav_read_data(
          handler->in_wavf[element_index],
          (unsigned char *)handler->wav_samples[element_index],
          handler->bsize_of_samples[element_index]);
    }

    dep_wav_read_close(handler->in_wavf[element_index]);
  } else if (element_type == AUDIO_ELEMENT_SCENE_BASED) {
  }
}

int iamf_base_profile_test(int argc, char *argv[]) {
  if (argc < 12) {
    print_usage(argv);
    return 0;
  }
  BaseProfileHandler handler = {0};
  memset(&handler, 0x00, sizeof(handler));
  handler.animated_parameter[0].animation_type =
      handler.animated_parameter[1].animation_type = ANIMATION_TYPE_INVALID;

  IAChannelLayoutType channel_layout_in[2] = {
      IA_CHANNEL_LAYOUT_COUNT,
  };
  IAChannelLayoutType channel_layout_cb[2][IA_CHANNEL_LAYOUT_COUNT];

  int is_standalone = 0;
  int profile = 0;
  int codec_id = IAMF_CODEC_UNKNOWN;
  AudioElementType audio_element_type = AUDIO_ELEMENT_INVALID;
  int ambisonics_mode[2] = {
      0,
  };
  int is_fragment_mp4 = 0;
  char *in_file = NULL, *in_file2 = NULL, *out_file = NULL;
  int args = 1;
  int index = 0;
  int element_id = 0;
  int chunk_size = 0;
  char *config_file = NULL;

  while (args < argc) {
    if (argv[args][0] == '-') {
      if ((strcmp(argv[args], "-codec") == 0)) {
        args++;
        if (!strncmp(argv[args], "opus", 4)) {
          codec_id = IAMF_CODEC_OPUS;
          if (strlen(argv[args]) > 4) chunk_size = atoi(argv[args] + 5);
        } else if (!strncmp(argv[args], "aac", 3)) {
          codec_id = IAMF_CODEC_AAC;
          if (strlen(argv[args]) > 3) chunk_size = atoi(argv[args] + 4);
        } else if (!strncmp(argv[args], "flac", 4)) {
          codec_id = IAMF_CODEC_FLAC;
          if (strlen(argv[args]) > 4) chunk_size = atoi(argv[args] + 5);
        } else if (!strncmp(argv[args], "pcm", 3)) {
          codec_id = IAMF_CODEC_PCM;
          if (strlen(argv[args]) > 3) chunk_size = atoi(argv[args] + 4);
        }
      } else if (argv[args][1] == 'i') {
        args++;
        handler.in_file[element_id] = argv[args];
        element_id++;
      } else if (strcmp(argv[args], "-profile") == 0) {
        args++;
        profile = atoi(argv[args]);
      } else if (strcmp(argv[args], "-config") == 0) {
        args++;
        config_file = argv[args];
      } else if (argv[args][1] == 'o') {
        args++;
        if (!strncmp("0", argv[args], 1))
          is_standalone = 1;
        else if (!strncmp("1", argv[args], 1)) {
          is_standalone = 0;
        } else if (!strncmp("2", argv[args], 1)) {
          is_standalone = 0;
          is_fragment_mp4 = 1;
        }
        args++;
        out_file = argv[args];
      } else {
        args++;
      }
    }
    args++;
  }
  handler.num_of_elements = element_id;

  // mix config json parse
  FILE *fp = fopen(config_file, "r");
  if (fp == NULL) {
    printf("Error opening file: %s\n", config_file);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  int file_size = ftell(fp);
  rewind(fp);

  char *json_string = malloc(file_size);
  fread(json_string, 1, file_size, fp);
  fclose(fp);

  cJSON *json = cJSON_Parse(json_string);
  if (!json) {
    printf("Error before: [%s]\n", cJSON_GetErrorPtr());
    free(json_string);
    return 1;
  }
  free(json_string);
  UserInputOption user_in;
  memset(&user_in, 0x00, sizeof(UserInputOption));
  paser_user_input_option(json, &user_in);

  MOVMuxContext *movm = NULL;
  FILE *iamf_file = NULL;

  if (is_standalone == 0) {
    if ((movm = mov_write_open(out_file)) == NULL) {
      fprintf(stderr, "Couldn't create MP4 output file.\n");
      return (0);
    }

    //********************** dep codec test***************************//
    movm->codec_id = codec_id;
    //********************** dep codec test***************************//

    //********************** fragment mp4 test***************************//
    if (is_fragment_mp4) {
      movm->flags = IA_MOV_FLAG_FRAGMENT;
      movm->max_fragment_duration = 10000;  // ms
    }
    //********************** fragment mp4 test***************************//

    if (mov_write_trak(movm, 0, 1) < 0) {
      fprintf(stderr, "Couldn't create media track.\n");
      exit(-1);
    }
  } else {
    if (!(iamf_file = fopen(out_file, "wb"))) {
      fprintf(stderr, "Couldn't create output file.\n");
      return (0);
    }
  }

  // read wav input files
  if (chunk_size == 0) {
    if (codec_id == IAMF_CODEC_OPUS)
      chunk_size = 960;
    else if (codec_id == IAMF_CODEC_AAC)
      chunk_size = 1024;
    else if (codec_id == IAMF_CODEC_FLAC)
      chunk_size = 1024;
    else if (codec_id == IAMF_CODEC_PCM)
      chunk_size = 960;
  }

  for (int i = 0; i < handler.num_of_elements; i++) {
    handler.in_wavf[i] = dep_wav_read_open(handler.in_file[i]);
    if (!handler.in_wavf[i]) {
      fprintf(stderr, "Could not open input file %s\n", handler.in_wavf[i]);
      goto failure;
    }

    dep_wav_get_header(handler.in_wavf[i], &(handler.format[i]),
                       &(handler.channels[i]), &(handler.sample_rate[i]),
                       &(handler.bits_per_sample[i]), &(handler.endianness[i]),
                       &(handler.data_length[i]));
    fprintf(stderr,
            "input wav: format[%d] channels[%d] sample_rate[%d] "
            "bits_per_sample[%d] endianness[%s] data_length[%d]\n",
            handler.format[i], handler.channels[i], handler.sample_rate[i],
            handler.bits_per_sample[i],
            (handler.endianness[i] == 1) ? "little-endian" : "big-endian",
            handler.data_length[i]);

    handler.bsize_of_samples[i] =
        chunk_size * handler.channels[i] * handler.bits_per_sample[i] / 8;
    handler.bsize_of_1sample[i] =
        handler.channels[i] * handler.bits_per_sample[i] / 8;
    handler.wav_samples[i] = (unsigned char *)malloc(
        chunk_size * handler.channels[i] * handler.bits_per_sample[i] / 8 *
        sizeof(unsigned char));

    dep_wav_read_close(handler.in_wavf[i]);
  }

  /**
   * 1. Create immersive encoder handle.
   * */
  int error = 0;
  handler.ia_enc = IAMF_encoder_create(
      handler.sample_rate[0], handler.bits_per_sample[0], handler.endianness[0],
      codec_id,  // 1:opus, 2:aac
      chunk_size, &error);

  for (int i = 0; i < handler.num_of_elements; i++) {
    handler.element_id[i] =
        IAMF_audio_element_add(handler.ia_enc, user_in.element_config[i]);
  }

  /**
   * 2. immersive encoder control.
   * */
  int preskip = 0;
  for (int i = 0; i < handler.num_of_elements; i++) {
    IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                     IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));
    IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                     IA_SET_IAMF_PROFILE((int)profile));
    if (user_in.element_config[i].demix.set_mode == 1) {
      IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                       IA_SET_IAMF_DEFAULT_DEMIX_MODE(
                           (int)user_in.element_config[i].demix.default_mode));
      IAMF_encoder_ctl(
          handler.ia_enc, handler.element_id[i],
          IA_SET_IAMF_DEFAULT_DEMIX_WEIGHT(
              (int)user_in.element_config[i].demix.default_weight));
    } else if (user_in.element_config[i].demix.set_mode == 2) {
      IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                       IA_SET_IAMF_DISABLE_DEMIX((int)1));
    }
    IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                     IA_GET_LOOKAHEAD(&preskip));
  }

  /**
   * 3. set mix presentation.
   * */
  for (int i = 0; i < user_in.mix_num; i++) {
    for (int j = 0; j < user_in.mixp_t[i].num_audio_elements; j++) {
      int element_index = user_in.mixp_t[i].element_index[j];
      user_in.mixp_t[i].mixp.audio_element_id[j] =
          handler.element_id[element_index];
    }

    if (user_in.mixp_t[i].mixp.num_layouts > 0) {
      for (int j = 0; j < handler.num_of_elements; j++) {
        handler.in_wavf[j] = dep_wav_read_open(handler.in_file[j]);
      }
      int pcm_frames[2] = {0, 0};
      pcm_frames[0] = dep_wav_read_data(handler.in_wavf[0],
                                        (unsigned char *)handler.wav_samples[0],
                                        handler.bsize_of_samples[0]);
      pcm_frames[1] = dep_wav_read_data(handler.in_wavf[1],
                                        (unsigned char *)handler.wav_samples[1],
                                        handler.bsize_of_samples[1]);
      int count = 0;
      IAMF_encoder_target_loudness_measure_start(handler.ia_enc,
                                                 &user_in.mixp_t[i].mixp);
      while (1) {
        if (pcm_frames[0] <= 0 || pcm_frames[1] <= 0) break;
        count++;

        IAFrame ia_frame, ia_frame2;
        memset(&ia_frame, 0x00, sizeof(ia_frame));
        memset(&ia_frame2, 0x00, sizeof(ia_frame2));

        int element_index = user_in.mixp_t[i].element_index[0];
        ia_frame.element_id = handler.element_id[element_index];
        ia_frame.samples =
            pcm_frames[element_index] / handler.bsize_of_1sample[element_index];
        ia_frame.pcm = handler.wav_samples[element_index];

        if (user_in.mixp_t[i].num_audio_elements > 1) {
          int element_index2 = user_in.mixp_t[i].element_index[1];
          ia_frame2.element_id = handler.element_id[element_index2];
          ia_frame2.samples = pcm_frames[element_index2] /
                              handler.bsize_of_1sample[element_index2];
          ia_frame2.pcm = handler.wav_samples[element_index2];
          ia_frame.next = &ia_frame2;
        }

        IAMF_encoder_target_loudness_measure(
            handler.ia_enc, &(user_in.mixp_t[i].mixp), &ia_frame);
        pcm_frames[0] = dep_wav_read_data(
            handler.in_wavf[0], (unsigned char *)handler.wav_samples[0],
            handler.bsize_of_samples[0]);
        pcm_frames[1] = dep_wav_read_data(
            handler.in_wavf[1], (unsigned char *)handler.wav_samples[1],
            handler.bsize_of_samples[1]);
      }
      IAMF_encoder_target_loudness_measure_stop(handler.ia_enc,
                                                &(user_in.mixp_t[i].mixp));
      for (int j = 0; j < handler.num_of_elements; j++) {
        dep_wav_read_close(handler.in_wavf[j]);
      }
    }
    uint64_t duration[2] = {0, 0};
    duration[0] = handler.data_length[0] / handler.bsize_of_1sample[0];
    duration[1] = handler.data_length[1] / handler.bsize_of_1sample[1];
    uint64_t min_duration =
        duration[0] < duration[1] ? duration[0] : duration[1] + preskip;
    min_duration = (min_duration / chunk_size == 0)
                       ? 0
                       : chunk_size + (min_duration / chunk_size) * chunk_size;
    for (int j = 0; j < user_in.mixp_t[i].num_audio_elements; j++) {
      int element_index = user_in.mixp_t[i].element_index[j];
      parameter_block_split2(&user_in.mixp_t[i].mixp.element_mix_config[j],
                             &user_in.mixp_t[i].element_mix[element_index],
                             chunk_size, handler.sample_rate[j]);
      mix_gain_config_free(&user_in.mixp_t[i].element_mix[element_index]);
    }
    parameter_block_split2(&user_in.mixp_t[i].mixp.output_mix_config,
                           &user_in.mixp_t[i].output_mix, chunk_size,
                           handler.sample_rate[0]);
    IAMF_encoder_set_mix_presentation(handler.ia_enc, user_in.mixp_t[i].mixp);

    mix_presentation_free(&(user_in.mixp_t[i].mixp));
    mix_gain_config_free(&user_in.mixp_t[i].output_mix);
  }

  for (int i = 0; i < handler.num_of_elements; i++) {
    iamf_pre_process(&handler, i, user_in.element_config[i].element_type);
  }

  /**
   * 7. get immersive audio global descriptor sample group.
   * */
  if (is_standalone == 0) {
    /////////////////////////audio trak setting,
    /// start//////////////////////////////////////////////
    // audio trak
    unsigned char channel_map714[] = {1, 2, 6, 8, 10, 8, 10, 12, 6};
    mov_audio_track *audio_t = movm->audio_trak;
    audio_t[0].av_descriptor[audio_t[0].descriptor_entries].size =
        IAMF_encoder_get_descriptor(
            handler.ia_enc,
            audio_t[0].av_descriptor[audio_t[0].descriptor_entries].data, 1024);
    audio_t[0].descriptor_entries++;

    /*
    spec, 7.2.3  IA Sample Entry
    Both channelcount and samplerate fields of AudioSampleEntry shall be
    ignored.
    */
    audio_t[0].samplerate = 48000;
    audio_t[0].channels = 2;
    audio_t[0].bits = 16;
    // codec specific info
    if (movm->codec_id == IAMF_CODEC_OPUS) {
      audio_t[0].iamf.roll_distance = -4;
    } else if (movm->codec_id == IAMF_CODEC_AAC) {
      audio_t[0].iamf.roll_distance = -1;
    } else {
      fprintf(stderr, "wrong codec input\n");
    }
    audio_t[0].iamf.initial_padding = preskip;
    /////////////////////////audio trak setting,
    /// end//////////////////////////////////////////////

    mov_write_head(movm);
  }

  /**
   * 8. immersive audio encode.
   * */
  for (int i = 0; i < handler.num_of_elements; i++) {
    handler.in_wavf[i] = dep_wav_read_open(handler.in_file[i]);
  }
  uint64_t frame_count = 0;
  uint32_t max_packet_size =
      (MAX_OUTPUT_CHANNELS * chunk_size * MAX_BITS_PER_SAMPLE) *
      3;  // two audio elements
  unsigned char *encode_ia = malloc(max_packet_size);
  int pcm_frames = dep_wav_read_data(handler.in_wavf[0],
                                     (unsigned char *)handler.wav_samples[0],
                                     handler.bsize_of_samples[0]);
  int pcm_frames2 = dep_wav_read_data(handler.in_wavf[1],
                                      (unsigned char *)handler.wav_samples[1],
                                      handler.bsize_of_samples[1]);

  while (1) {
    IAFrame ia_frame, ia_frame2;
    memset(&ia_frame, 0x00, sizeof(ia_frame));
    memset(&ia_frame2, 0x00, sizeof(ia_frame2));
    ia_frame.element_id = handler.element_id[0];
    ia_frame.samples = pcm_frames / handler.bsize_of_1sample[0];
    if (ia_frame.samples > 0)
      ia_frame.pcm = handler.wav_samples[0];
    else
      ia_frame.pcm = NULL;  // output pending encoded data

    ia_frame2.element_id = handler.element_id[1];
    ia_frame2.samples = pcm_frames2 / handler.bsize_of_1sample[1];
    ia_frame2.pcm = handler.wav_samples[1];
    if (ia_frame2.samples > 0)
      ia_frame2.pcm = handler.wav_samples[1];
    else
      ia_frame2.pcm = NULL;  // output pending encoded data
    ia_frame2.next = NULL;
    ia_frame.next = &ia_frame2;

    IAPacket ia_packet;
    memset(&ia_packet, 0x00, sizeof(ia_packet));
    ia_packet.data = encode_ia;
    if (IAMF_encoder_encode(handler.ia_enc, &ia_frame, &ia_packet,
                            max_packet_size) < 0) {
      break;  // encoding is end
    }
    if (is_standalone == 0) {
      AVPacket pkt;
      memset(&pkt, 0x00, sizeof(pkt));
      pkt.size = ia_packet.packet_size;
      pkt.buf = ia_packet.data;
      pkt.samples = ia_packet.samples;
      if (pkt.size > 0) mov_write_audio2(movm, 0, &pkt);
    } else {
      fwrite(ia_packet.data, ia_packet.packet_size, 1, iamf_file);
    }
    pcm_frames = dep_wav_read_data(handler.in_wavf[0],
                                   (unsigned char *)handler.wav_samples[0],
                                   handler.bsize_of_samples[0]);
    pcm_frames2 = dep_wav_read_data(handler.in_wavf[1],
                                    (unsigned char *)handler.wav_samples[1],
                                    handler.bsize_of_samples[1]);
    frame_count++;
  }

  fprintf(stderr, "encoding total count: %" PRId64 "\n", frame_count);

  if (is_standalone == 0) {
    mov_write_tail(movm);
    mov_write_close(movm);
  } else {
    if (iamf_file) fclose(iamf_file);
  }
  if (encode_ia) free(encode_ia);
  /**
   * 9. destroy IAMF handle.
   * */
  IAMF_encoder_destroy(handler.ia_enc);
failure:
  for (int i = 0; i < handler.num_of_elements; i++) {
    if (handler.in_wavf[i]) dep_wav_read_close(handler.in_wavf[i]);
    if (handler.wav_samples[i]) free(handler.wav_samples[i]);
  }

  cJSON_Delete(json);
  return 0;
}

static int get_profile_num(char *argv[]) { return (atoi(argv[2])); }
int main(int argc, char *argv[]) {
  if (argc < 10) {
    print_usage(argv);
    return 0;
  }
  if (get_profile_num(argv) == 0)
    iamf_simple_profile_test(argc, argv);
  else if (get_profile_num(argv) == 1)
    iamf_base_profile_test(argc, argv);
  else
    fprintf(stderr, "Wrong profile\n");
  return 0;
}
