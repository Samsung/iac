#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "IAMF_encoder.h"

#include "wavreader.h"
#include "wavwriter.h"
#include "mp4mux.h"
#include "opus_header.h"
#include "aac_header.h"
#include "progressbar.h"

#include <time.h>
#define IA_FRAME_MAXSIZE 1024
#define MAX_PACKET_SIZE  (MAX_CHANNELS*sizeof(int16_t)*IA_FRAME_MAXSIZE) // IA_FRAME_MAXSIZE*2/channel


typedef struct {
  FILE *wav;
  uint32_t data_length;

  int format;
  int sample_rate;
  int bits_per_sample;
  int channels;
  int byte_rate;
  int block_align;

  int streamed;
}wav_reader_s;

void print_usage(char* argv[])
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "-profile   : <0/1(simpe/base)>\n");
  fprintf(stderr, "-codec   : <opus/aac/pcm>\n");
  fprintf(stderr, "-mode    : <audio element type(0:channle-based,1:scene-based)/input channel layout/channel layout combinations>\n");
  fprintf(stderr, "<input wav file>\n");
  fprintf(stderr, "-o   : <0/1(mp4/bitstream)>\n");
  fprintf(stderr, "<output file>\n\n");
  fprintf(stderr, "Example:\niamfpackager -profile 0 -codec opus -mode 0/7.1.4/2.0.0+3.1.2+5.1.2 input.wav -o 1 simple_profile.iamf\n");
  fprintf(stderr, "or\niamfpackager -profile 1 -codec opus -mode 0/7.1.4/2.0.0+3.1.2+5.1.2 input1.wav input2.wav -o 1 base_profile.iamf\n");
}

int iamf_simple_profile_test(int argc, char *argv[])
{

  if (argc < 11) {
    print_usage(argv);
    return 0;
  }

  char *channel_layout_string[] = { "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4", "3.1.2", "binaural" };

  IAChannelLayoutType channel_layout_in = IA_CHANNEL_LAYOUT_COUNT;
  IAChannelLayoutType channel_layout_cb[IA_CHANNEL_LAYOUT_COUNT];

  int is_standalone = 0;
  int codec_id = IA_CODEC_UNKNOWN;
  AudioElementType audio_element_type = AUDIO_ELEMENT_INVALID;
  int ambisonics_mode = 0;
  int is_fragment_mp4 = 0;
  char *in_file = NULL, *out_file = NULL;
  int args = 1;
  int index = 0;
  while (args < argc)
  {
    if (argv[args][0] == '-')
    {
      if (argv[args][1] == 'c')
      {
        args++;
        if (!strncmp(argv[args], "opus", 4))
          codec_id = IA_CODEC_OPUS;
        else if (!strncmp(argv[args], "aac", 3))
          codec_id = IA_CODEC_AAC;
        else if (!strncmp(argv[args], "pcm", 3))
          codec_id = IA_CODEC_PCM;
      }
      else if (argv[args][1] == 'm')
      {
        args++;
        if (argv[args][0] == '0')
        {
          audio_element_type = AUDIO_ELEMENT_CHANNEL_BASED;
          fprintf(stderr, "\nAudio Element Type:%s\n", audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED ? "Channel-Base" : "Scene-Base");
          char *p_start = &(argv[args][2]);
          for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++)
          {
            if (!strncmp(channel_layout_string[i], p_start, strlen(channel_layout_string[i])))
            {
              fprintf(stderr, "\nInput channel layout:%s\n", channel_layout_string[i]);
              channel_layout_in = i;
              break;
            }
          }
          if (channel_layout_in == IA_CHANNEL_LAYOUT_COUNT)
          {
            fprintf(stderr, stderr, "Please check input channel layout format\n");
            return 0;
          }

          int channel_cb_ptr = 0;
          fprintf(stderr, "Channel layout combinations: ");
          while (channel_cb_ptr < strlen(p_start + strlen(channel_layout_string[channel_layout_in])))
          {
            for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++)
            {
              if (!strncmp(channel_layout_string[i], p_start + 6 + channel_cb_ptr, 5))
              {
                fprintf(stderr, "%s ", channel_layout_string[i]);
                channel_layout_cb[index] = i;
                index++;
              }
            }
            channel_cb_ptr += 6;
          }
          fprintf(stderr, "\n \n");

          channel_layout_cb[index] = IA_CHANNEL_LAYOUT_COUNT;
        }
        else if (argv[args][0] == '1')
        {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode = 0;
          fprintf(stderr, "\nAudio Element Type:%s\n", audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED ? "Channel-Base" : "Scene-Base");
        }
        else if (argv[args][0] == '2')
        {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode = 1;
          fprintf(stderr, "\nAudio Element Type:%s\n", audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED ? "Channel-Base" : "Scene-Base");
        }
        else
        {
          fprintf(stderr, "wrong mode:%c\n", argv[args][1]);
          return (0);
        }

        
      }
      else if (argv[args][1] == 'p')
      {
        args++;
      }
    }
    else
    {
      in_file = argv[args++];
      args++;
      if (!strncmp("0", argv[args++], 1))
        is_standalone = 0;
      else
        is_standalone = 1;
      out_file = argv[args];
    }
    args++;
  }

  MOVMuxContext *movm = NULL;
  FILE *iamf_file = NULL;

  if (is_standalone == 0)
  {
    if ((movm = mov_write_open(out_file)) == NULL)
    {
      fprintf(stderr, "Couldn't create MP4 output file.\n");
      return (0);
    }

    //********************** dep codec test***************************//
    movm->codec_id = codec_id;
    //********************** dep codec test***************************//

    //********************** fragment mp4 test***************************//
    if (is_fragment_mp4)
      movm->flags = IA_MOV_FLAG_FRAGMENT;
    movm->max_fragment_duration = 10000; //ms
                                         //********************** fragment mp4 test***************************//

    if (mov_write_trak(movm, 0, 1) < 0)
    {
      fprintf(stderr, "Couldn't create media track.\n");
      exit(-1);
    }
  }
  else
  {

    if (!(iamf_file = fopen(out_file, "wb")))
    {
      fprintf(stderr, "Couldn't create output file.\n");
      return (0);
    }
  }

  //////////////////////read PCM data from wav[start]///////////////////////////
  void *in_wavf = NULL;
  char in_wav[255] = { 0 };
  if (strlen(in_file)<255)
    strncpy(in_wav, in_file, strlen(in_file));
  //sprintf(in_wav, "%s.wav", in_file);
  in_wavf = wav_read_open(in_wav);
  if (!in_wavf)
  {
    fprintf(stderr, "Could not open input file %s\n", in_wav);
    goto failure;
  }
  int size;
  int format;
  int channels;
  int sample_rate;
  int bits_per_sample;
  unsigned int data_length;

  wav_get_header(in_wavf, &format, &channels, &sample_rate, &bits_per_sample, &data_length);
  fprintf(stderr, "input wav: format[%d] channels[%d] sample_rate[%d] bits_per_sample[%d] data_length[%d]\n",
    format, channels, sample_rate, bits_per_sample, data_length);

  int chunk_size = 0;
  if (codec_id == IA_CODEC_OPUS)
    chunk_size = 960;
  else if (codec_id == IA_CODEC_AAC)
    chunk_size = 1024;
  else if (codec_id == IA_CODEC_PCM)
    chunk_size = 960;

  int bsize_of_samples = chunk_size * channels * bits_per_sample / 8;
  int bsize_of_1sample = channels * bits_per_sample / 8;
  int bsize_of_1ch_float_samples = chunk_size * sizeof(float);
  int bsize_of_float_samples = chunk_size * channels * sizeof(float);
  unsigned char * wav_samples = (unsigned char*)malloc(bsize_of_samples *sizeof(unsigned char));

  clock_t start_t = 0, stop_t = 0;
  /**
  * 1. Create immersive encoder handle.
  * */
  int error = 0;
  IAMF_Encoder *ia_enc = IAMF_encoder_create(sample_rate,
    codec_id,  //1:opus, 2:aac
    chunk_size,
    &error);
  AudioElementConfig element_config;
  if (audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED)
  {
    element_config.layout_in = channel_layout_in;
    element_config.layout_cb = channel_layout_cb;
  }
  else if (audio_element_type == AUDIO_ELEMENT_SCENE_BASED)
  {
    element_config.input_channels = channels;
    if (ambisonics_mode == 0)
    {
      element_config.ambisonics_mode = ambisonics_mode;
      element_config.ambisonics_mono_config.output_channel_count = channels;
      element_config.ambisonics_mono_config.substream_count = channels;
      for (int i = 0; i < channels; i++)
      {
        element_config.ambisonics_mono_config.channel_mapping[i] = i;
      }
    }
    else if (ambisonics_mode == 1)
    {
      element_config.ambisonics_mode = ambisonics_mode;
      element_config.ambisonics_projection_config.substream_count = (channels + 1)/ 2;
      element_config.ambisonics_projection_config.coupled_substream_count = channels / 2;
    }
  }
  int element_id = IAMF_audio_element_add(ia_enc,
    audio_element_type,
    element_config);


  /**
  * 2. immersive encoder control.
  * */
  //IAMF_encoder_ctl(ia_enc, element_id, IA_SET_RECON_GAIN_FLAG((int)(recon_gain_flag)));
  //IAMF_encoder_ctl(ia_enc, element_id, IA_SET_OUTPUT_GAIN_FLAG((int)output_gain_flag));
  IAMF_encoder_ctl(ia_enc, element_id, IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));
  int preskip = 0;
  IAMF_encoder_ctl(ia_enc, element_id, IA_GET_LOOKAHEAD(&preskip));

  // set mix presentation
  MixPresentation mix_presentation = { 0, };
  mix_presentation.mix_presentation_annotations.mix_presentation_friendly_label = "CHN";
  mix_presentation.num_sub_mixes = 1;
  mix_presentation.num_audio_elements = 1;
  mix_presentation.audio_element_id[0] = element_id;
  mix_presentation.mix_presentation_element_annotations[0].mix_presentation_friendly_label = "CHN";
  mix_presentation.element_mix_config[0].default_mix_gain = 0.0f;
  mix_presentation.output_mix_config.default_mix_gain = 0.0f;

  IAMF_encoder_set_mix_presentation(ia_enc, mix_presentation);

  if (audio_element_type == AUDIO_ELEMENT_SCENE_BASED)
    goto ENCODING;
  /**
  * 3. ASC and HEQ pre-process.
  * */
  if (index > 0) // non-scalable
  {
    start_t = clock();
    IAMF_encoder_dmpd_start(ia_enc, element_id);
    int pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    while (pcm_frames)
    {
      IAMF_encoder_dmpd_process(ia_enc, element_id, wav_samples, pcm_frames / bsize_of_1sample);
      pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

    }
    IAMF_encoder_dmpd_stop(ia_enc, element_id);
    stop_t = clock();
    fprintf(stderr, "dmpd total time %f(s)\n", (float)(stop_t - start_t) / CLOCKS_PER_SEC);
  }

  if (in_wavf)
    wav_read_close(in_wavf);


  start_t = clock();
  /**
  * 4. loudness and gain pre-process.
  * */
  in_wavf = wav_read_open(in_wav);
  int pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  ProgressBar bar;
  ProgressBarInit(&bar);
  int total, current;

  wav_reader_s *wr;
  bar.startBar(&bar, "Loudness", 0);
  total = data_length;
  current = 0;
  while (pcm_frames)
  {
    IAMF_encoder_loudness_gain(ia_enc, element_id, wav_samples, pcm_frames / bsize_of_1sample);

    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    wr = (wav_reader_s *)in_wavf;
    current = total - wr->data_length;
    float pct = ((float)current / 1000) / ((float)total / 1000) * 100;
    if (bar._x < (int)pct)
      bar.proceedBar(&bar, (int)pct);
  }
  bar.endBar(&bar, 100);

  IAMF_encoder_loudness_gain_end(ia_enc, element_id);

  if (in_wavf)
    wav_read_close(in_wavf);
  /**
  * 5. calculate recon gain.
  * */
  in_wavf = wav_read_open(in_wav);
  pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

  while (pcm_frames)
  {
    IAMF_encoder_recon_gain(ia_enc, element_id, wav_samples, pcm_frames / bsize_of_1sample);
    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  }

ENCODING:
  if (in_wavf)
    wav_read_close(in_wavf);



  /**
  * 6. get immersive audio global descriptor sample group.
  * */
  if (is_standalone == 0)
  {
    /////////////////////////audio trak setting, start//////////////////////////////////////////////
    //audio trak
    unsigned char sub_bitstream_count_map[] = { 1,1,4,5,6,5,6,7,4 };
    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
    mov_audio_track *audio_t = movm->audio_trak;
    audio_t[0].ia_descriptor.size_of_mix_presentations_group_entry =
      get_IAMF_encoder_mix_presentations(ia_enc, audio_t[0].ia_descriptor.mix_presentations_group_entry);

    audio_t[0].ia_descriptor.size_of_audio_elements_group_entry =
      get_IAMF_encoder_audio_elements(ia_enc, audio_t[0].ia_descriptor.audio_elements_group_entry);

    int profile_vesrion = get_IAMF_encoder_profile_version(ia_enc);

    /*
    spec, 7.2.3  IA Sample Entry
    Both channelcount and samplerate fields of AudioSampleEntry shall be ignored.
    */
    audio_t[0].iamf.profile_version = profile_vesrion;
    audio_t[0].samplerate = 48000;
    audio_t[0].channels = channel_map714[channel_layout_in];
    audio_t[0].bits = 16;
    //audio_t[0].iamf.codec_id = 0;
    //audio_t[0].iamf.sub_bitstream_count = sub_bitstream_count_map[channel_layout_in]; //
    // codec specific info
    if (movm->codec_id == IA_CODEC_OPUS)
    {
      OpusHeader *header = (OpusHeader *)audio_t[0].iamf.csc;
      header->output_channel_count = 2;
      header->preskip = 312;
      header->input_sample_rate = 48000;
      header->output_gain = 0;
      header->channel_mapping_family = 0;
      audio_t[0].iamf.roll_distance = -4;
    }
    else if (movm->codec_id == IA_CODEC_AAC)
    {
      AacHeader *header = (AacHeader *)audio_t[0].iamf.csc;
      header->sample_rate = 48000;
      header->object_type = 2;
      header->sampling_index = 3;//48khz
      header->chan_config = 2; // 2 channels
      audio_t[0].iamf.roll_distance = -1;
    }
    else
    {
      fprintf(stderr, "wrong codec input\n");
    }
    /////////////////////////audio trak setting, end//////////////////////////////////////////////

    mov_write_head(movm);
  }



  /**
  * 7. immersive audio encode.
  * */
  uint64_t frame_count = 0;
  unsigned char encode_ia[MAX_PACKET_SIZE * 3];
  unsigned char demix_group[MAX_PACKET_SIZE];
  in_wavf = wav_read_open(in_wav);
  pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  int demix_mode = 0;
  while (pcm_frames)
  {
    IAFrame ia_frame;
    memset(&ia_frame, 0x00, sizeof(ia_frame));
    //if (frame_count == 0)
    //  ia_frame.num_samples_to_trim_at_start = 320;
    ia_frame.element_id = element_id;
    ia_frame.frame_size = pcm_frames / bsize_of_1sample;
    ia_frame.pcm = wav_samples;
    ia_frame.next = NULL;
    IAPacket ia_packet;
    memset(&ia_packet, 0x00, sizeof(ia_packet));
    ia_packet.data = encode_ia;
    ia_packet.demix_group = demix_group;
    IAMF_encoder_encode(ia_enc, &ia_frame, &ia_packet, MAX_PACKET_SIZE);

    if (is_standalone == 0)
    {
      AVPacket pkt;
      memset(&pkt, 0x00, sizeof(pkt));
      pkt.size = ia_packet.packet_size;
      pkt.buf = ia_packet.data;
      pkt.size_of_demix_group = ia_packet.size_of_demix_group;
      pkt.demix_group = ia_packet.demix_group;
      pkt.samples = pcm_frames / bsize_of_1sample;
      mov_write_audio2(movm, 0, &pkt);
      //mov_write_audio(movm, 0, encode_ia, ia_packet.packet_size, pcm_frames / bsize_of_1sample);
    }
    else
    {
      fwrite(ia_packet.data, ia_packet.packet_size, 1, iamf_file);
    }

    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    frame_count++;
  }

  stop_t = clock();
  fprintf(stderr, "encoding total time %f(s), total count: %ld\n", (float)(stop_t - start_t) / CLOCKS_PER_SEC, frame_count);

  if (is_standalone == 0)
  { 
    mov_write_tail(movm);
    mov_write_close(movm);
  }
  else
  {
    if(iamf_file)
      fclose(iamf_file);
  }


  /**
  * 7. destroy channel group encoder handle.
  * */
  IAMF_encoder_destroy(ia_enc);
  if (wav_samples)
  {
    free(wav_samples);
  }
failure:
  if (in_wavf)
    wav_read_close(in_wavf);


  return 0;
}

int iamf_base_profile_test(int argc, char *argv[])
{

  if (argc < 12) {
    print_usage(argv);
    return 0;
  }

  char *channel_layout_string[] = { "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4", "3.1.2", "binaural" };

  IAChannelLayoutType channel_layout_in = IA_CHANNEL_LAYOUT_COUNT;
  IAChannelLayoutType channel_layout_cb[IA_CHANNEL_LAYOUT_COUNT];

  int is_standalone = 0;
  int codec_id = IA_CODEC_UNKNOWN;
  AudioElementType audio_element_type = AUDIO_ELEMENT_INVALID;
  int ambisonics_mode = 0;
  int is_fragment_mp4 = 0;
  char *in_file = NULL, *in_file2 = NULL, *out_file = NULL;
  int args = 1;
  int index = 0;
  while (args < argc)
  {
    if (argv[args][0] == '-')
    {
      if (argv[args][1] == 'c')
      {
        args++;
        if (!strncmp(argv[args], "opus", 4))
          codec_id = IA_CODEC_OPUS;
        else if (!strncmp(argv[args], "aac", 3))
          codec_id = IA_CODEC_AAC;
        else if (!strncmp(argv[args], "pcm", 3))
          codec_id = IA_CODEC_PCM;
      }
      else if (argv[args][1] == 'm')
      {
        args++;
        if (argv[args][0] == '0')
        {
          audio_element_type = AUDIO_ELEMENT_CHANNEL_BASED;
          fprintf(stderr, "\nAudio Element Type:%s\n", audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED ? "Channel-Base" : "Scene-Base");
          char *p_start = &(argv[args][2]);
          for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++)
          {
            if (!strncmp(channel_layout_string[i], p_start, strlen(channel_layout_string[i])))
            {
              fprintf(stderr, "\nInput channel layout:%s\n", channel_layout_string[i]);
              channel_layout_in = i;
              break;
            }
          }
          if (channel_layout_in == IA_CHANNEL_LAYOUT_COUNT)
          {
            fprintf(stderr, stderr, "Please check input channel layout format\n");
            return 0;
          }

          int channel_cb_ptr = 0;
          fprintf(stderr, "Channel layout combinations: ");
          while (channel_cb_ptr < strlen(p_start + strlen(channel_layout_string[channel_layout_in])))
          {
            for (int i = 0; i < IA_CHANNEL_LAYOUT_COUNT; i++)
            {
              if (!strncmp(channel_layout_string[i], p_start + 6 + channel_cb_ptr, 5))
              {
                fprintf(stderr, "%s ", channel_layout_string[i]);
                channel_layout_cb[index] = i;
                index++;
              }
            }
            channel_cb_ptr += 6;
          }
          fprintf(stderr, "\n \n");

          channel_layout_cb[index] = IA_CHANNEL_LAYOUT_COUNT;
        }
        else if (argv[args][0] == '1')
        {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode = 0;
          fprintf(stderr, "\nAudio Element Type:%s\n", audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED ? "Channel-Base" : "Scene-Base");
        }
        else if (argv[args][0] == '2')
        {
          audio_element_type = AUDIO_ELEMENT_SCENE_BASED;
          ambisonics_mode = 1;
          fprintf(stderr, "\nAudio Element Type:%s\n", audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED ? "Channel-Base" : "Scene-Base");
        }
        else
        {
          fprintf(stderr, "wrong mode:%c\n", argv[args][1]);
          return (0);
        }

      }
      else if (argv[args][1] == 'p')
      {
        args++;
      }

    }
    else
    {
      in_file = argv[args++];
      in_file2 = argv[args++];
      args++;
      if (!strncmp("0", argv[args++], 1))
        is_standalone = 0;
      else
        is_standalone = 1;
      out_file = argv[args];
    }
    args++;
  }

  MOVMuxContext *movm = NULL;
  FILE *iamf_file = NULL;
  if (!(iamf_file = fopen(out_file, "wb")))
  {
    fprintf(stderr, "Couldn't create output file.\n");
    return (0);
  }

  //////////////////////read PCM data from wav[start]///////////////////////////
  void *in_wavf = NULL;
  char in_wav[255] = { 0 };
  if (strlen(in_file)<255)
    strncpy(in_wav, in_file, strlen(in_file));

  in_wavf = wav_read_open(in_wav);
  if (!in_wavf)
  {
    fprintf(stderr, "Could not open input file %s\n", in_wav);
    goto failure;
  }
  int size;
  int format;
  int channels;
  int sample_rate;
  int bits_per_sample;
  unsigned int data_length;

  wav_get_header(in_wavf, &format, &channels, &sample_rate, &bits_per_sample, &data_length);
  fprintf(stderr, "input wav: format[%d] channels[%d] sample_rate[%d] bits_per_sample[%d] data_length[%d]\n",
    format, channels, sample_rate, bits_per_sample, data_length);

  int chunk_size = 0;
  if (codec_id == IA_CODEC_OPUS)
    chunk_size = 960;
  else if (codec_id == IA_CODEC_AAC)
    chunk_size = 1024;
  else if (codec_id == IA_CODEC_PCM)
    chunk_size = 960;

  int bsize_of_samples = chunk_size * channels * bits_per_sample / 8;
  int bsize_of_1sample = channels * bits_per_sample / 8;
  int bsize_of_1ch_float_samples = chunk_size * sizeof(float);
  int bsize_of_float_samples = chunk_size * channels * sizeof(float);
  unsigned char * wav_samples = (unsigned char*)malloc(bsize_of_samples *sizeof(unsigned char));

  void *in_wavf2 = NULL;
  char in_wav2[255] = { 0 };
  if (strlen(in_file2)<255)
    strncpy(in_wav2, in_file2, strlen(in_file2));

  in_wavf2 = wav_read_open(in_wav2);
  if (!in_wavf2)
  {
    fprintf(stderr, "Could not open input file %s\n", in_wav2);
    goto failure;
  }
  int size2;
  int format2;
  int channels2;
  int sample_rate2;
  int bits_per_sample2;
  unsigned int data_length2;

  wav_get_header(in_wavf2, &format2, &channels2, &sample_rate2, &bits_per_sample2, &data_length2);
  fprintf(stderr, "input wav2: format[%d] channels[%d] sample_rate[%d] bits_per_sample[%d] data_length[%d]\n",
    format2, channels2, sample_rate2, bits_per_sample2, data_length2);

  int bsize_of_samples2 = chunk_size * channels2 * bits_per_sample2 / 8;
  int bsize_of_1sample2 = channels2 * bits_per_sample2 / 8;
  int bsize_of_1ch_float_samples2 = chunk_size * sizeof(float);
  int bsize_of_float_samples2 = chunk_size * channels2 * sizeof(float);
  unsigned char * wav_samples2 = (unsigned char*)malloc(bsize_of_samples2 *sizeof(unsigned char));

  clock_t start_t, stop_t;
  /**
  * 1. Create immersive encoder handle.
  * */
  int error = 0;
  IAMF_Encoder *ia_enc = IAMF_encoder_create(sample_rate,
    codec_id,  //1:opus, 2:aac
    chunk_size,
    &error);

  AudioElementConfig element_config;
  if (audio_element_type == AUDIO_ELEMENT_CHANNEL_BASED)
  {
    element_config.layout_in = channel_layout_in;
    element_config.layout_cb = channel_layout_cb;
  }
  else if (audio_element_type == AUDIO_ELEMENT_SCENE_BASED)
  {
    element_config.input_channels = channels;
    if (ambisonics_mode == 0)
    {
      element_config.ambisonics_mode = ambisonics_mode;
      element_config.ambisonics_mono_config.output_channel_count = channels;
      element_config.ambisonics_mono_config.substream_count = channels;
      for (int i = 0; i < channels; i++)
      {
        element_config.ambisonics_mono_config.channel_mapping[i] = i;
      }
    }
    else if (ambisonics_mode == 1)
    {
      element_config.ambisonics_mode = ambisonics_mode;
      element_config.ambisonics_projection_config.substream_count = (channels + 1) / 2;
      element_config.ambisonics_projection_config.coupled_substream_count = channels / 2;
    }
  }
  int element_id = IAMF_audio_element_add(ia_enc,
    audio_element_type,
    element_config);


  IAChannelLayoutType channel_layout_cb2[IA_CHANNEL_LAYOUT_COUNT]
    = { IA_CHANNEL_LAYOUT_COUNT , };
  AudioElementConfig element_config2;
  element_config2.layout_in = IA_CHANNEL_LAYOUT_STEREO;
  element_config2.layout_cb = channel_layout_cb2;
  int element_id2 = IAMF_audio_element_add(ia_enc,
    AUDIO_ELEMENT_CHANNEL_BASED,
    element_config2);


  /**
  * 2. immersive encoder control.
  * */
  //IAMF_encoder_ctl(ia_enc, element_id, IA_SET_RECON_GAIN_FLAG((int)(recon_gain_flag)));
  //IAMF_encoder_ctl(ia_enc, element_id, IA_SET_OUTPUT_GAIN_FLAG((int)output_gain_flag));
  IAMF_encoder_ctl(ia_enc, element_id, IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));
  int preskip = 0;
  IAMF_encoder_ctl(ia_enc, element_id, IA_GET_LOOKAHEAD(&preskip));


  //IAMF_encoder_ctl(ia_enc, element_id2, IA_SET_RECON_GAIN_FLAG((int)(0)));
  //IAMF_encoder_ctl(ia_enc, element_id2, IA_SET_OUTPUT_GAIN_FLAG((int)0));
  IAMF_encoder_ctl(ia_enc, element_id2, IA_SET_STANDALONE_REPRESENTATION((int)is_standalone));

  // set mix presentation
  // set mix presentation
  MixPresentation mix_presentation = { 0, };
  mix_presentation.mix_presentation_annotations.mix_presentation_friendly_label = "CHN";
  mix_presentation.num_sub_mixes = 1;
  mix_presentation.num_audio_elements = 2;
  mix_presentation.audio_element_id[0] = element_id;
  mix_presentation.mix_presentation_element_annotations[0].mix_presentation_friendly_label = "CHN";
  mix_presentation.element_mix_config[0].default_mix_gain = 0.0f;
  mix_presentation.audio_element_id[1] = element_id2;
  mix_presentation.mix_presentation_element_annotations[1].mix_presentation_friendly_label = "CHN";
  mix_presentation.element_mix_config[1].default_mix_gain = 0.0f;
  mix_presentation.output_mix_config.default_mix_gain = 0.0f;

  IAMF_encoder_set_mix_presentation(ia_enc, mix_presentation);

  if (audio_element_type == AUDIO_ELEMENT_SCENE_BASED)
    goto ENCODING;

  /**
  * 3. ASC and HEQ pre-process.
  * */
  //if (index > 0) // non-scalable
  {
    start_t = clock();
    IAMF_encoder_dmpd_start(ia_enc, element_id);
    int pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    while (pcm_frames)
    {
      IAMF_encoder_dmpd_process(ia_enc, element_id, wav_samples, pcm_frames / bsize_of_1sample);
      pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

    }
    IAMF_encoder_dmpd_stop(ia_enc, element_id);
    stop_t = clock();
    fprintf(stderr, "dmpd total time %f(s)\n", (float)(stop_t - start_t) / CLOCKS_PER_SEC);
  }

  if (in_wavf)
    wav_read_close(in_wavf);


  start_t = clock();
  /**
  * 4. loudness and gain pre-process.
  * */
  in_wavf = wav_read_open(in_wav);
  int pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  ProgressBar bar;
  ProgressBarInit(&bar);
  int total, current;

  wav_reader_s *wr;
  bar.startBar(&bar, "Loudness", 0);
  total = data_length;
  current = 0;
  while (pcm_frames)
  {
    IAMF_encoder_loudness_gain(ia_enc, element_id, wav_samples, pcm_frames / bsize_of_1sample);

    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    wr = (wav_reader_s *)in_wavf;
    current = total - wr->data_length;
    float pct = ((float)current / 1000) / ((float)total / 1000) * 100;
    if (bar._x < (int)pct)
      bar.proceedBar(&bar, (int)pct);
  }
  bar.endBar(&bar, 100);

  IAMF_encoder_loudness_gain_end(ia_enc, element_id);

  if (in_wavf)
    wav_read_close(in_wavf);


  ////second audio element
  int pcm_frames2 = wav_read_data(in_wavf2, (unsigned char *)wav_samples2, bsize_of_samples2);
  ProgressBar bar2;
  ProgressBarInit(&bar2);
  int total2, current2;

  wav_reader_s *wr2;
  bar2.startBar(&bar2, "Loudness", 0);
  total2 = data_length2;
  current2 = 0;
  while (pcm_frames2)
  {
    IAMF_encoder_loudness_gain(ia_enc, element_id2, wav_samples2, pcm_frames2 / bsize_of_1sample2);

    pcm_frames2 = wav_read_data(in_wavf2, (unsigned char *)wav_samples2, bsize_of_samples2);
    wr2 = (wav_reader_s *)in_wavf2;
    current2 = total2 - wr2->data_length;
    float pct = ((float)current2 / 1000) / ((float)total2 / 1000) * 100;
    if (bar2._x < (int)pct)
      bar2.proceedBar(&bar2, (int)pct);
  }
  bar2.endBar(&bar2, 100);

  IAMF_encoder_loudness_gain_end(ia_enc, element_id2);
  if (in_wavf2)
    wav_read_close(in_wavf2);

  /**
  * 5. calculate recon gain.
  * */
  in_wavf = wav_read_open(in_wav);
  pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

  while (pcm_frames)
  {
    IAMF_encoder_recon_gain(ia_enc, element_id, wav_samples, pcm_frames / bsize_of_1sample);
    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  }

ENCODING:
  if (in_wavf)
    wav_read_close(in_wavf);

  /**
  * 7. immersive audio encode.
  * */
  uint64_t frame_count = 0;
  unsigned char encode_ia[MAX_PACKET_SIZE * 3];
  in_wavf = wav_read_open(in_wav);
  pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

  in_wavf2 = wav_read_open(in_wav2);
  pcm_frames2 = wav_read_data(in_wavf2, (unsigned char *)wav_samples2, bsize_of_samples2);

  int read_ending = 0, read_ending2 = 0;
  while (pcm_frames || pcm_frames2)
  {
    if (pcm_frames <= 0 && read_ending == 0)
    {
      read_ending = 1;
      IAMF_audio_element_delete(ia_enc, element_id);
      IAMF_encoder_clear_mix_presentation(ia_enc);

      MixPresentation mix_presentation = { 0, };
	  
      mix_presentation.mix_presentation_annotations.mix_presentation_friendly_label = "CHN";
      mix_presentation.num_sub_mixes = 1;
      mix_presentation.num_audio_elements = 1;
      mix_presentation.audio_element_id[0] = element_id2;
      mix_presentation.mix_presentation_element_annotations[0].mix_presentation_friendly_label = "CHN";
      mix_presentation.element_mix_config[0].default_mix_gain = 0.0f;
      mix_presentation.output_mix_config.default_mix_gain = 0.0f;
      IAMF_encoder_set_mix_presentation(ia_enc, mix_presentation);
    }
    if (pcm_frames2 <= 0 && read_ending2 == 0)
    {
      read_ending2 = 1;
      IAMF_audio_element_delete(ia_enc, element_id2);
      IAMF_encoder_clear_mix_presentation(ia_enc);

      MixPresentation mix_presentation = { 0, };
	  
      mix_presentation.mix_presentation_annotations.mix_presentation_friendly_label = "CHN";
      mix_presentation.num_sub_mixes = 1;
      mix_presentation.num_audio_elements = 1;
      mix_presentation.audio_element_id[0] = element_id;
      mix_presentation.mix_presentation_element_annotations[0].mix_presentation_friendly_label = "CHN";
      mix_presentation.element_mix_config[0].default_mix_gain = 0.0f;
      mix_presentation.output_mix_config.default_mix_gain = 0.0f;
      IAMF_encoder_set_mix_presentation(ia_enc, mix_presentation);
    }
    IAFrame ia_frame, ia_frame2;
    memset(&ia_frame, 0x00, sizeof(ia_frame));
    memset(&ia_frame2, 0x00, sizeof(ia_frame2));
    if (read_ending == 0 && read_ending2 == 0)
    {
      ia_frame.element_id = element_id;
      ia_frame.frame_size = pcm_frames / bsize_of_1sample;
      ia_frame.pcm = wav_samples;

      ia_frame2.element_id = element_id2;
      ia_frame2.frame_size = pcm_frames2 / bsize_of_1sample2;
      ia_frame2.pcm = wav_samples2;
      ia_frame2.next = NULL;
      ia_frame.next = &ia_frame2;
    }
    else if (read_ending == 0 && read_ending2 == 1)
    {
      ia_frame.element_id = element_id;
      ia_frame.frame_size = pcm_frames / bsize_of_1sample;
      ia_frame.pcm = wav_samples;
      ia_frame.next = NULL;
    }
    else if (read_ending == 1 && read_ending2 == 0)
    {
      ia_frame.element_id = element_id2;
      ia_frame.frame_size = pcm_frames2 / bsize_of_1sample2;
      ia_frame.pcm = wav_samples2;
      ia_frame.next = NULL;
    }

    IAPacket ia_packet;
    memset(&ia_packet, 0x00, sizeof(ia_packet));
    ia_packet.data = encode_ia;
    IAMF_encoder_encode(ia_enc, &ia_frame, &ia_packet, MAX_PACKET_SIZE);

    fwrite(ia_packet.data, ia_packet.packet_size, 1, iamf_file);

    if(read_ending == 0)
      pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    if (read_ending2 == 0)
      pcm_frames2 = wav_read_data(in_wavf2, (unsigned char *)wav_samples2, bsize_of_samples2);
    frame_count++;
  }

  stop_t = clock();
  fprintf(stderr, "encoding total time %f(s), total count: %ld\n", (float)(stop_t - start_t) / CLOCKS_PER_SEC, frame_count);

  fclose(iamf_file);
  /**
  * 7. destroy channel group encoder handle.
  * */
  IAMF_encoder_destroy(ia_enc);
  if (wav_samples)
  {
    free(wav_samples);
  }
  if (wav_samples2)
  {
    free(wav_samples2);
  }
failure:
  if (in_wavf)
    wav_read_close(in_wavf);

  if (in_wavf2)
    wav_read_close(in_wavf2);

  return 0;
}

int main(int argc, char *argv[])
{
  if(argc == 11)
    iamf_simple_profile_test(argc, argv);
  else if(argc == 12)
    iamf_base_profile_test(argc, argv);
  else 
	print_usage(argv);
  return 0;

}
