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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "IAMF_encoder.h"
#include "mp4mux.h"
#include "progressbar.h"
#include "wavreader.h"
#include "wavwriter.h"
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
  fprintf(stderr, "-profile : <0/1(simpe/base)>\n");
  fprintf(stderr,
          "-codec   : <codec name/frame size(opus,aac,flac,pcm/1024)>\n");
  fprintf(stderr,
          "-mode    : <audio element type(0:channle-based,1:scene-based)/input "
          "channel layout/channel layout combinations>\n");
  fprintf(stderr, "-gain    : <default mix gain(dB) (Not Necessary)>\n");
  fprintf(stderr, "-i       : <input wav file>\n");
  fprintf(stderr,
          "-target  : <target layout for measuring loudness(sound system) (Not "
          "Necessary)>\n");
  fprintf(stderr, "-o       : <0/1/2(bitstream/mp4/fmp4)> <output file>\n");
  fprintf(stderr,
          "Example:\niamfpackager -profile 0 -codec opus -mode "
          "0/7.1.4/2.0.0+3.1.2+5.1.2 -i input.wav -o 0 simple_profile.iamf\n");
  fprintf(stderr,
          "or\niamfpackager -profile 1 -codec opus -mode 0/7.1.4/3.1.2+5.1.2 "
          "-gain 0.0 "
          "-i input1.wav -mode 1 -gain 0.0 -i input2.wav -target s0+s2+s9 -o 0 "
          "base_profile.iamf\n");
}

int iamf_simple_profile_test(int argc, char *argv[]) {
  if (argc < 12) {
    print_usage(argv);
    return 0;
  }

  char *channel_layout_string[] = {"1.0.0", "2.0.0",   "5.1.0", "5.1.2",
                                   "5.1.4", "7.1.0",   "7.1.2", "7.1.4",
                                   "3.1.2", "binaural"};

  IAChannelLayoutType channel_layout_in = IA_CHANNEL_LAYOUT_COUNT;
  IAChannelLayoutType channel_layout_cb[IA_CHANNEL_LAYOUT_COUNT];

  int is_standalone = 0;
  int codec_id = IAMF_CODEC_UNKNOWN;
  AudioElementType audio_element_type = AUDIO_ELEMENT_INVALID;
  int ambisonics_mode = 0;
  int is_fragment_mp4 = 0;
  char *in_file = NULL, *out_file = NULL;
  int args = 1;
  int index = 0;
  int chunk_size = 0;
  while (args < argc) {
    if (argv[args][0] == '-') {
      if (argv[args][1] == 'c') {
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
      } else if (argv[args][1] == 'm') {
        args++;
        if (argv[args][0] == '0') {
          audio_element_type = AUDIO_ELEMENT_CHANNEL_BASED;
          fprintf(stderr, "\nAudio Element Type:%s\n",
                  audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED
                      ? "Channel-Base"
                      : "Scene-Base");
          char *p_start = &(argv[args][2]);
          for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
            if (!strncmp(channel_layout_string[i], p_start,
                         strlen(channel_layout_string[i]))) {
              fprintf(stderr, "\nInput channel layout:%s\n",
                      channel_layout_string[i]);
              channel_layout_in = i;
              break;
            }
          }
          if (channel_layout_in == IA_CHANNEL_LAYOUT_COUNT) {
            fprintf(stderr, stderr,
                    "Please check input channel layout format\n");
            return 0;
          }

          int channel_cb_ptr = 0;
          fprintf(stderr, "Channel layout combinations: ");
          while (channel_cb_ptr <
                 strlen(p_start +
                        strlen(channel_layout_string[channel_layout_in]))) {
            for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
              if (!strncmp(channel_layout_string[i],
                           p_start + 6 + channel_cb_ptr, 5)) {
                fprintf(stderr, "%s ", channel_layout_string[i]);
                channel_layout_cb[index] = i;
                index++;
              }
            }
            channel_cb_ptr += 6;
          }
          fprintf(stderr, "\n \n");

          channel_layout_cb[index] = IA_CHANNEL_LAYOUT_COUNT;
        } else if (argv[args][0] == '1') {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode = 0;
          fprintf(stderr, "\nAudio Element Type:%s\n",
                  audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED
                      ? "Channel-Base"
                      : "Scene-Base");
        } else if (argv[args][0] == '2') {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode = 1;
          fprintf(stderr, "\nAudio Element Type:%s\n",
                  audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED
                      ? "Channel-Base"
                      : "Scene-Base");
        } else {
          fprintf(stderr, "wrong mode:%c\n", argv[args][1]);
          return (0);
        }

      } else if (argv[args][1] == 'p') {
        args++;
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
  in_wavf = wav_read_open(in_wav);
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

  wav_get_header(in_wavf, &format, &channels, &sample_rate, &bits_per_sample,
                 &endianness, &data_length);
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
  AudioElementConfig element_config;
  if (audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED) {
    element_config.layout_in = channel_layout_in;
    element_config.layout_cb = channel_layout_cb;
  } else if (audio_element_type == AUDIO_ELEMENT_SCENE_BASED) {
    element_config.input_channels = channels;
    if (ambisonics_mode == 0) {
      element_config.ambisonics_mode = ambisonics_mode;
      element_config.ambisonics_mono_config.output_channel_count = channels;
      element_config.ambisonics_mono_config.substream_count = channels;
      for (int i = 0; i < channels; i++) {
        element_config.ambisonics_mono_config.channel_mapping[i] = i;
      }
    } else if (ambisonics_mode == 1) {
      element_config.ambisonics_mode = ambisonics_mode;
      element_config.ambisonics_projection_config.substream_count =
          (channels + 1) / 2;
      element_config.ambisonics_projection_config.coupled_substream_count =
          channels / 2;
    }
  }
  int element_id =
      IAMF_audio_element_add(ia_enc, audio_element_type, element_config);

  /**
   * 2. immersive encoder control.
   * */
  // IAMF_encoder_ctl(ia_enc, element_id,
  // IA_SET_RECON_GAIN_FLAG((int)(recon_gain_flag))); IAMF_encoder_ctl(ia_enc,
  // element_id, IA_SET_OUTPUT_GAIN_FLAG((int)output_gain_flag));
  IAMF_encoder_ctl(ia_enc, element_id,
                   IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));
  int preskip = 0;
  IAMF_encoder_ctl(ia_enc, element_id, IA_GET_LOOKAHEAD(&preskip));

  // set mix presentation
  MixPresentation mix_presentation = {
      0,
  };
  mix_presentation.mix_presentation_annotations
      .mix_presentation_friendly_label = "CHN";
  mix_presentation.num_sub_mixes = 1;
  mix_presentation.num_audio_elements = 1;
  mix_presentation.audio_element_id[0] = element_id;
  mix_presentation.mix_presentation_element_annotations[0]
      .mix_presentation_friendly_label = "CHN";
  mix_presentation.element_mix_config[0].default_mix_gain = 0.0f;
  mix_presentation.output_mix_config.default_mix_gain = 0.0f;

  IAMF_encoder_set_mix_presentation(ia_enc, mix_presentation);

  if (audio_element_type == AUDIO_ELEMENT_SCENE_BASED) goto ENCODING;
  /**
   * 3. ASC and HEQ pre-process.
   * */
  if (index > 0)  // non-scalable
  {
    start_t = clock();
    IAMF_encoder_dmpd_start(ia_enc, element_id);
    int pcm_frames =
        wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    while (pcm_frames) {
      IAMF_encoder_dmpd_process(ia_enc, element_id, wav_samples,
                                pcm_frames / bsize_of_1sample);
      pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples,
                                 bsize_of_samples);
    }
    IAMF_encoder_dmpd_stop(ia_enc, element_id);
    stop_t = clock();
    fprintf(stderr, "dmpd total time %f(s)\n",
            (float)(stop_t - start_t) / CLOCKS_PER_SEC);
  }

  if (in_wavf) wav_read_close(in_wavf);

  start_t = clock();
  /**
   * 4. loudness and gain pre-process.
   * */
  in_wavf = wav_read_open(in_wav);
  int pcm_frames =
      wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  ProgressBar bar;
  ProgressBarInit(&bar);
  int total, current;

  wav_reader_s *wr;
  bar.startBar(&bar, "Loudness", 0);
  total = data_length;
  current = 0;
  while (pcm_frames) {
    IAMF_encoder_scalable_loudnessgain_measure(ia_enc, element_id, wav_samples,
                                               pcm_frames / bsize_of_1sample);

    pcm_frames =
        wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    wr = (wav_reader_s *)in_wavf;
    current = total - wr->data_length;
    float pct = ((float)current / 1000) / ((float)total / 1000) * 100;
    if (bar._x < (int)pct) bar.proceedBar(&bar, (int)pct);
  }
  bar.endBar(&bar, 100);

  IAMF_encoder_scalable_loudnessgain_stop(ia_enc, element_id);

  if (in_wavf) wav_read_close(in_wavf);
  /**
   * 5. calculate recon gain.
   * */
  in_wavf = wav_read_open(in_wav);
  pcm_frames =
      wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

  while (pcm_frames) {
    IAMF_encoder_reconstruct_gain(ia_enc, element_id, wav_samples,
                                  pcm_frames / bsize_of_1sample);
    pcm_frames =
        wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  }

ENCODING:
  if (in_wavf) wav_read_close(in_wavf);

  /**
   * 6. get immersive audio global descriptor sample group.
   * */
  if (is_standalone == 0) {
    /////////////////////////audio trak setting,
    /// start//////////////////////////////////////////////
    // audio trak
    unsigned char channel_map714[] = {1, 2, 6, 8, 10, 8, 10, 12, 6};
    mov_audio_track *audio_t = movm->audio_trak;
    audio_t[0].ia_descriptor.size = IAMF_encoder_get_descriptor(
        ia_enc, audio_t[0].ia_descriptor.data, 1024);
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
  in_wavf = wav_read_open(in_wav);
  pcm_frames =
      wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  int demix_mode = 0;
  while (1) {
    IAFrame ia_frame;
    memset(&ia_frame, 0x00, sizeof(ia_frame));

    /*trimming test
    if (frame_count == 0)
      ia_frame.num_samples_to_trim_at_start = 320;
    */
    ia_frame.element_id = element_id;
    ia_frame.frame_size = pcm_frames / bsize_of_1sample;
    if (ia_frame.frame_size > 0)
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

    pcm_frames =
        wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    frame_count++;
  }

  stop_t = clock();
  fprintf(stderr, "encoding total time %f(s), total count: %ld\n",
          (float)(stop_t - start_t) / CLOCKS_PER_SEC, frame_count);

  if (is_standalone == 0) {
    mov_write_tail(movm);
    mov_write_close(movm);
  } else {
    if (iamf_file) fclose(iamf_file);
  }

  /**
   * 7. destroy channel group encoder handle.
   * */
  IAMF_encoder_destroy(ia_enc);
  if (wav_samples) {
    free(wav_samples);
  }
  if (encode_ia) free(encode_ia);
failure:
  if (in_wavf) wav_read_close(in_wavf);

  return 0;
}

typedef struct {
  IAMF_Encoder *ia_enc;
  int num_of_elements;

  AudioElementType audio_element_type[2];
  AudioElementConfig element_config[2];
  int element_id[2];
  float default_mix_gain[2];

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
} BaseProfileHandler;

void iamf_pre_process(BaseProfileHandler *handler, int element_index) {
  if (handler->audio_element_type[element_index] ==
      AUDIO_ELEMENT_CHANNEL_BASED) {
    /**
     * 3. ASC and HEQ pre-process.
     * */
    handler->in_wavf[element_index] =
        wav_read_open(handler->in_file[element_index]);

    IAMF_encoder_dmpd_start(handler->ia_enc,
                            handler->element_id[element_index]);
    int pcm_frames =
        wav_read_data(handler->in_wavf[element_index],
                      (unsigned char *)handler->wav_samples[element_index],
                      handler->bsize_of_samples[element_index]);
    while (pcm_frames) {
      IAMF_encoder_dmpd_process(
          handler->ia_enc, handler->element_id[element_index],
          handler->wav_samples[element_index],
          pcm_frames / handler->bsize_of_1sample[element_index]);
      pcm_frames =
          wav_read_data(handler->in_wavf[element_index],
                        (unsigned char *)handler->wav_samples[element_index],
                        handler->bsize_of_samples[element_index]);
    }
    IAMF_encoder_dmpd_stop(handler->ia_enc, handler->element_id[element_index]);

    wav_read_close(handler->in_wavf[element_index]);

    /**
     * 4. loudness and gain pre-process.
     * */
    handler->in_wavf[element_index] =
        wav_read_open(handler->in_file[element_index]);
    pcm_frames =
        wav_read_data(handler->in_wavf[element_index],
                      (unsigned char *)handler->wav_samples[element_index],
                      handler->bsize_of_samples[element_index]);
    ProgressBar bar;
    ProgressBarInit(&bar);
    int total, current;

    wav_reader_s *wr;
    bar.startBar(&bar, "Loudness", 0);
    total = handler->data_length[element_index];
    current = 0;
    while (pcm_frames) {
      IAMF_encoder_scalable_loudnessgain_measure(
          handler->ia_enc, handler->element_id[element_index],
          handler->wav_samples[element_index],
          pcm_frames / handler->bsize_of_1sample[element_index]);

      pcm_frames =
          wav_read_data(handler->in_wavf[element_index],
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

    wav_read_close(handler->in_wavf[element_index]);

    /**
     * 5. calculate recon gain.
     * */
    handler->in_wavf[element_index] =
        wav_read_open(handler->in_file[element_index]);

    pcm_frames =
        wav_read_data(handler->in_wavf[element_index],
                      (unsigned char *)handler->wav_samples[element_index],
                      handler->bsize_of_samples[element_index]);

    while (pcm_frames) {
      IAMF_encoder_reconstruct_gain(
          handler->ia_enc, handler->element_id[element_index],
          handler->wav_samples[element_index],
          pcm_frames / handler->bsize_of_1sample[element_index]);
      pcm_frames =
          wav_read_data(handler->in_wavf[element_index],
                        (unsigned char *)handler->wav_samples[element_index],
                        handler->bsize_of_samples[element_index]);
    }

    wav_read_close(handler->in_wavf[element_index]);
  } else if (handler->audio_element_type[element_index] ==
             AUDIO_ELEMENT_SCENE_BASED) {
  }
}

int iamf_base_profile_test(int argc, char *argv[]) {
  if (argc < 16 || argc > 22) {
    print_usage(argv);
    return 0;
  }
  BaseProfileHandler handler = {0};
  memset(&handler, 0x00, sizeof(handler));

  char *channel_layout_string[] = {"1.0.0", "2.0.0",   "5.1.0", "5.1.2",
                                   "5.1.4", "7.1.0",   "7.1.2", "7.1.4",
                                   "3.1.2", "binaural"};

  IAChannelLayoutType channel_layout_in[2] = {
      IA_CHANNEL_LAYOUT_COUNT,
  };
  IAChannelLayoutType channel_layout_cb[2][IA_CHANNEL_LAYOUT_COUNT];

  int is_standalone = 0;
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
  while (args < argc) {
    if (argv[args][0] == '-') {
      if (argv[args][1] == 'c') {
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
      } else if (argv[args][1] == 'm') {
        args++;
        if (argv[args][0] == '0') {
          index = 0;
          audio_element_type = AUDIO_ELEMENT_CHANNEL_BASED;
          fprintf(stderr, "\nAudio Element Type:%s\n",
                  audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED
                      ? "Channel-Base"
                      : "Scene-Base");
          char *p_start = &(argv[args][2]);
          for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
            if (!strncmp(channel_layout_string[i], p_start,
                         strlen(channel_layout_string[i]))) {
              fprintf(stderr, "\nInput channel layout:%s\n",
                      channel_layout_string[i]);
              channel_layout_in[element_id] = i;
              break;
            }
          }
          if (channel_layout_in == IA_CHANNEL_LAYOUT_COUNT) {
            fprintf(stderr, stderr,
                    "Please check input channel layout format\n");
            return 0;
          }

          int channel_cb_ptr = 0;
          fprintf(stderr, "Channel layout combinations: ");
          while (
              channel_cb_ptr <
              strlen(
                  p_start +
                  strlen(
                      channel_layout_string[channel_layout_in[element_id]]))) {
            for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++) {
              if (!strncmp(channel_layout_string[i],
                           p_start + 6 + channel_cb_ptr, 5)) {
                fprintf(stderr, "%s ", channel_layout_string[i]);
                channel_layout_cb[element_id][index] = i;
                index++;
              }
            }
            channel_cb_ptr += 6;
          }
          fprintf(stderr, "\n \n");

          channel_layout_cb[element_id][index] = IA_CHANNEL_LAYOUT_COUNT;
        } else if (argv[args][0] == '1') {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode[element_id] = 0;
          fprintf(stderr, "\nAudio Element Type:%s\n",
                  audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED
                      ? "Channel-Base"
                      : "Scene-Base");
        } else if (argv[args][0] == '2') {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode[element_id] = 1;
          fprintf(stderr, "\nAudio Element Type:%s\n",
                  audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED
                      ? "Channel-Base"
                      : "Scene-Base");
        } else {
          fprintf(stderr, "wrong mode:%c\n", argv[args][1]);
          return (0);
        }

        handler.audio_element_type[element_id] = audio_element_type;
      } else if (argv[args][1] == 'g') {
        args++;
        handler.default_mix_gain[element_id] = atof(argv[args]);
      } else if (argv[args][1] == 'i') {
        args++;
        handler.in_file[element_id] = argv[args];
        element_id++;
      } else if (argv[args][1] == 'p') {
        args++;
      } else if (argv[args][1] == 't') {
        char ss_temp[128];
        args++;
        int layout_ptr = 0;
        int ss_ptr = 0;
        while (layout_ptr < strlen(argv[args])) {
          if (argv[args][layout_ptr] == 's') {
            ss_ptr = 0;
          } else if (argv[args][layout_ptr] >= '0' &&
                     argv[args][layout_ptr] <= '9') {
            ss_temp[ss_ptr++] = argv[args][layout_ptr];
          } else {
            ss_temp[ss_ptr] = '\0';
            handler.measured_target[handler.num_layouts++] = atoi(ss_temp);
          }
          layout_ptr++;
        }
        ss_temp[ss_ptr] = '\0';
        handler.measured_target[handler.num_layouts++] = atoi(ss_temp);
      } else if (argv[args][1] == 'o') {
        args++;
        if (!strncmp("0", argv[args], 1))
          is_standalone = 1;
        else if (!strncmp("1", argv[args], 1)) {
          printf("base profile only support fragment mp4");
          return (0);
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
    handler.in_wavf[i] = wav_read_open(handler.in_file[i]);
    if (!handler.in_wavf[i]) {
      fprintf(stderr, "Could not open input file %s\n", handler.in_wavf[i]);
      goto failure;
    }

    wav_get_header(handler.in_wavf[i], &(handler.format[i]),
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

    if (handler.audio_element_type[i] == AUDIO_ELEMENT_CHANNEL_BASED) {
      handler.element_config[i].layout_in = channel_layout_in[i];
      handler.element_config[i].layout_cb = channel_layout_cb[i];
    } else if (handler.audio_element_type[i] == AUDIO_ELEMENT_SCENE_BASED) {
      handler.element_config[i].ambisonics_mode = ambisonics_mode[i];
      handler.element_config[i].input_channels = handler.channels[i];
      if (ambisonics_mode[i] == 0) {
        handler.element_config[i].ambisonics_mono_config.output_channel_count =
            handler.channels[i];
        handler.element_config[i].ambisonics_mono_config.substream_count =
            handler.channels[i];
        for (int j = 0; j < handler.channels[i]; j++) {
          handler.element_config[i].ambisonics_mono_config.channel_mapping[j] =
              i;
        }
      } else if (ambisonics_mode[i] == 1) {
        handler.element_config[i].ambisonics_projection_config.substream_count =
            (handler.channels[i] + 1) / 2;
        handler.element_config[i]
            .ambisonics_projection_config.coupled_substream_count =
            handler.channels[i] / 2;
      }
    }
    wav_read_close(handler.in_wavf[i]);
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
        IAMF_audio_element_add(handler.ia_enc, handler.audio_element_type[i],
                               handler.element_config[i]);
  }

  /**
   * 2. immersive encoder control.
   * */
  int preskip = 0;
  for (int i = 0; i < handler.num_of_elements; i++) {
    IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                     IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));
    IAMF_encoder_ctl(handler.ia_enc, handler.element_id[i],
                     IA_GET_LOOKAHEAD(&preskip));
  }
  // set mix presentation
  MixPresentation mix_presentation = {
      0,
  };
  mix_presentation.mix_presentation_annotations
      .mix_presentation_friendly_label = "CHN";
  mix_presentation.num_sub_mixes = 1;
  mix_presentation.num_audio_elements = 2;
  mix_presentation.audio_element_id[0] = handler.element_id[0];
  mix_presentation.mix_presentation_element_annotations[0]
      .mix_presentation_friendly_label = "CHN";
  mix_presentation.element_mix_config[0].default_mix_gain =
      handler.default_mix_gain[0];
  mix_presentation.audio_element_id[1] = handler.element_id[1];
  mix_presentation.mix_presentation_element_annotations[1]
      .mix_presentation_friendly_label = "CHN";
  mix_presentation.element_mix_config[1].default_mix_gain =
      handler.default_mix_gain[1];
  mix_presentation.output_mix_config.default_mix_gain = 0.0f;

  mix_presentation.num_layouts = handler.num_layouts;
  for (int i = 0; i < mix_presentation.num_layouts; i++) {
    mix_presentation.loudness_layout[i].layout_type =
        IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
    mix_presentation.loudness_layout[i].sound_system =
        handler.measured_target[i];
  }
  // Calculate the loudness for preset target layout
  if (mix_presentation.num_layouts > 0) {
    for (int i = 0; i < handler.num_of_elements; i++) {
      handler.in_wavf[i] = wav_read_open(handler.in_file[i]);
    }
    IAMF_encoder_target_loudness_measure_start(handler.ia_enc,
                                               &mix_presentation);
    int pcm_frames = wav_read_data(handler.in_wavf[0],
                                   (unsigned char *)handler.wav_samples[0],
                                   handler.bsize_of_samples[0]);
    int pcm_frames2 = wav_read_data(handler.in_wavf[1],
                                    (unsigned char *)handler.wav_samples[1],
                                    handler.bsize_of_samples[1]);
    int count = 0;
    while (pcm_frames) {
      if (pcm_frames <= 0 || pcm_frames2 <= 0) break;
      count++;
      IAFrame ia_frame, ia_frame2;
      memset(&ia_frame, 0x00, sizeof(ia_frame));
      memset(&ia_frame2, 0x00, sizeof(ia_frame2));

      ia_frame.element_id = handler.element_id[0];
      ia_frame.frame_size = pcm_frames / handler.bsize_of_1sample[0];
      ia_frame.pcm = handler.wav_samples[0];

      ia_frame2.element_id = handler.element_id[1];
      ia_frame2.frame_size = pcm_frames2 / handler.bsize_of_1sample[1];
      ia_frame2.pcm = handler.wav_samples[1];
      ia_frame2.next = NULL;
      ia_frame.next = &ia_frame2;
      IAMF_encoder_target_loudness_measure(handler.ia_enc, &mix_presentation,
                                           &ia_frame);
      pcm_frames = wav_read_data(handler.in_wavf[0],
                                 (unsigned char *)handler.wav_samples[0],
                                 handler.bsize_of_samples[0]);
      pcm_frames2 = wav_read_data(handler.in_wavf[1],
                                  (unsigned char *)handler.wav_samples[1],
                                  handler.bsize_of_samples[1]);
    }
    IAMF_encoder_target_loudness_measure_stop(handler.ia_enc,
                                              &mix_presentation);

    for (int i = 0; i < handler.num_of_elements; i++) {
      wav_read_close(handler.in_wavf[i]);
    }
  }

  IAMF_encoder_set_mix_presentation(handler.ia_enc, mix_presentation);

  for (int i = 0; i < handler.num_of_elements; i++) {
    iamf_pre_process(&handler, i);
  }

  /**
   * 6. get immersive audio global descriptor sample group.
   * */
  if (is_standalone == 0) {
    /////////////////////////audio trak setting,
    /// start//////////////////////////////////////////////
    // audio trak
    unsigned char channel_map714[] = {1, 2, 6, 8, 10, 8, 10, 12, 6};
    mov_audio_track *audio_t = movm->audio_trak;
    audio_t[0].ia_descriptor.size = IAMF_encoder_get_descriptor(
        handler.ia_enc, audio_t[0].ia_descriptor.data, 1024);

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
   * 7. immersive audio encode.
   * */
  for (int i = 0; i < handler.num_of_elements; i++) {
    handler.in_wavf[i] = wav_read_open(handler.in_file[i]);
  }
  uint64_t frame_count = 0;
  uint32_t max_packet_size =
      (MAX_OUTPUT_CHANNELS * chunk_size * MAX_BITS_PER_SAMPLE) *
      3;  // two audio elements
  unsigned char *encode_ia = malloc(max_packet_size);
  int pcm_frames =
      wav_read_data(handler.in_wavf[0], (unsigned char *)handler.wav_samples[0],
                    handler.bsize_of_samples[0]);
  int pcm_frames2 =
      wav_read_data(handler.in_wavf[1], (unsigned char *)handler.wav_samples[1],
                    handler.bsize_of_samples[1]);

  int read_ending = 0, read_ending2 = 0;
  while (pcm_frames || pcm_frames2) {
    if (pcm_frames <= 0 && read_ending == 0) {
      read_ending = 1;
      IAMF_audio_element_delete(handler.ia_enc, handler.element_id[0]);
      IAMF_encoder_clear_mix_presentation(handler.ia_enc);

      MixPresentation mix_presentation = {
          0,
      };

      mix_presentation.mix_presentation_annotations
          .mix_presentation_friendly_label = "CHN";
      mix_presentation.num_sub_mixes = 1;
      mix_presentation.num_audio_elements = 1;
      mix_presentation.audio_element_id[0] = handler.element_id[1];
      mix_presentation.mix_presentation_element_annotations[0]
          .mix_presentation_friendly_label = "CHN";
      mix_presentation.element_mix_config[0].default_mix_gain =
          handler.default_mix_gain[1];
      mix_presentation.output_mix_config.default_mix_gain = 0.0f;
      IAMF_encoder_set_mix_presentation(handler.ia_enc, mix_presentation);

      if (is_standalone == 0) {
        mov_audio_track *audio_t = movm->audio_trak;
        audio_t[0].ia_descriptor.size = IAMF_encoder_get_descriptor(
            handler.ia_enc, audio_t[0].ia_descriptor.data, 1024);
        audio_t[0].ia_descriptor.is_changed = 1;
        mp4_flush_segment(movm);
      }
    }
    if (pcm_frames2 <= 0 && read_ending2 == 0) {
      read_ending2 = 1;
      IAMF_audio_element_delete(handler.ia_enc, handler.element_id[1]);
      IAMF_encoder_clear_mix_presentation(handler.ia_enc);

      MixPresentation mix_presentation = {
          0,
      };

      mix_presentation.mix_presentation_annotations
          .mix_presentation_friendly_label = "CHN";
      mix_presentation.num_sub_mixes = 1;
      mix_presentation.num_audio_elements = 1;
      mix_presentation.audio_element_id[0] = handler.element_id[0];
      mix_presentation.mix_presentation_element_annotations[0]
          .mix_presentation_friendly_label = "CHN";
      mix_presentation.element_mix_config[0].default_mix_gain =
          handler.default_mix_gain[0];
      mix_presentation.output_mix_config.default_mix_gain = 0.0f;
      IAMF_encoder_set_mix_presentation(handler.ia_enc, mix_presentation);

      if (is_standalone == 0) {
        mov_audio_track *audio_t = movm->audio_trak;
        audio_t[0].ia_descriptor.size = IAMF_encoder_get_descriptor(
            handler.ia_enc, audio_t[0].ia_descriptor.data, 1024);
        audio_t[0].ia_descriptor.is_changed = 1;
        mp4_flush_segment(movm);
      }
    }
    IAFrame ia_frame, ia_frame2;
    memset(&ia_frame, 0x00, sizeof(ia_frame));
    memset(&ia_frame2, 0x00, sizeof(ia_frame2));

    /*trimming test
    if (frame_count == 0)
    {
      ia_frame.num_samples_to_trim_at_start = 320;
      ia_frame2.num_samples_to_trim_at_start = 320;
    }
    */
    if (read_ending == 0 && read_ending2 == 0) {
      ia_frame.element_id = handler.element_id[0];
      ia_frame.frame_size = pcm_frames / handler.bsize_of_1sample[0];
      ia_frame.pcm = handler.wav_samples[0];

      ia_frame2.element_id = handler.element_id[1];
      ia_frame2.frame_size = pcm_frames2 / handler.bsize_of_1sample[1];
      ia_frame2.pcm = handler.wav_samples[1];
      ia_frame2.next = NULL;
      ia_frame.next = &ia_frame2;
    } else if (read_ending == 0 && read_ending2 == 1) {
      ia_frame.element_id = handler.element_id[0];
      ia_frame.frame_size = pcm_frames / handler.bsize_of_1sample[0];
      ia_frame.pcm = handler.wav_samples[0];
      ia_frame.next = NULL;
    } else if (read_ending == 1 && read_ending2 == 0) {
      ia_frame.element_id = handler.element_id[1];
      ia_frame.frame_size = pcm_frames2 / handler.bsize_of_1sample[1];
      ia_frame.pcm = handler.wav_samples[1];
      ia_frame.next = NULL;
    }

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
      pkt.samples = ia_frame.frame_size;
      if (pkt.size > 0) mov_write_audio2(movm, 0, &pkt);
      // mov_write_audio(movm, 0, encode_ia, ia_packet.packet_size, pcm_frames /
      // bsize_of_1sample);
    } else {
      fwrite(ia_packet.data, ia_packet.packet_size, 1, iamf_file);
    }

    if (read_ending == 0)
      pcm_frames = wav_read_data(handler.in_wavf[0],
                                 (unsigned char *)handler.wav_samples[0],
                                 handler.bsize_of_samples[0]);
    if (read_ending2 == 0)
      pcm_frames2 = wav_read_data(handler.in_wavf[1],
                                  (unsigned char *)handler.wav_samples[1],
                                  handler.bsize_of_samples[1]);
    frame_count++;
  }

  fprintf(stderr, "encoding total count: %ld\n", frame_count);

  if (is_standalone == 0) {
    mov_write_tail(movm);
    mov_write_close(movm);
  } else {
    if (iamf_file) fclose(iamf_file);
  }
  if (encode_ia) free(encode_ia);
  /**
   * 7. destroy channel group encoder handle.
   * */
  IAMF_encoder_destroy(handler.ia_enc);
failure:
  for (int i = 0; i < handler.num_of_elements; i++) {
    if (handler.in_wavf[i]) wav_read_close(handler.in_wavf[i]);
    if (handler.wav_samples[i]) free(handler.wav_samples[i]);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc == 12)
    iamf_simple_profile_test(argc, argv);
  else if (argc > 15 && argc < 23)
    iamf_base_profile_test(argc, argv);
  else
    print_usage(argv);
  return 0;
}
