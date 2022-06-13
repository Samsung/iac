#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "immersive_audio_encoder.h"

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
  fprintf(stderr, "-codec   : <opus/aac>\n");
  fprintf(stderr, "-mode    : <input channel layout/channel layout combinations>\n");
  fprintf(stderr, "<input wav file>\n");
  fprintf(stderr, "<output mp4 file>\n\n");
  fprintf(stderr, "Example:\nencode2mp4 -codec opus -mode 7.1.4/2.0.0+3.1.2+5.1.2 replace_audio.wav replace_audio.mp4\n");
}

int main(int argc, char *argv[])
{

#if 1
  if (argc < 7) {
    print_usage(argv);
    return 0;
  }

  char *channel_layout_string[] = { "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4", "3.1.2" };

  CHANNEL_LAYOUT channel_layout_in = CHANNEL_LAYOUT_MAX;
  unsigned char channel_layout_cb[CHANNEL_LAYOUT_MAX];

  int recon_gain_flag = 1;
  int output_gain_flag = 1;
  int codec_id = 0;
  int is_fragment_mp4 = 0;
  char *in_file, *out_file;
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
          codec_id = 0;
        else if (!strncmp(argv[args], "aac", 3))
          codec_id = 1;
      }
      else if (argv[args][1] == 'm')
      {
        args++;
        for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
        {
          if (!strncmp(channel_layout_string[i], argv[args], 5))
          {
            printf("\nInput channel layout:%s\n", channel_layout_string[i]);
            channel_layout_in = i;
            break;
          }
        }
        if (channel_layout_in == CHANNEL_LAYOUT_MAX)
        {
          printf(stderr, "Please check input channel layout format\n");
          return 0;
        }

        int channel_cb_ptr = 0;
        printf("Channel layout combinations: ");
        while (channel_cb_ptr < strlen(argv[args] + 6))
        {
          for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
          {
            if (!strncmp(channel_layout_string[i], argv[args] + 6 + channel_cb_ptr, 5))
            {
              printf("%s ", channel_layout_string[i]);
              channel_layout_cb[index] = i;
              index++;
            }
          }
          channel_cb_ptr += 6;
        }
        printf("\n \n");

        channel_layout_cb[index] = CHANNEL_LAYOUT_MAX;
        if (index == 0)
        {
          recon_gain_flag = 0;
          output_gain_flag = 0;
        }
      }
    }
    else 
    {
      in_file = argv[args++];
      out_file = argv[args];
    }
    args++;
  }

#else
  CHANNEL_LAYOUT channel_layout_in = CHANNEL_LAYOUT_714;
  unsigned char channel_layout_cb[CHANNEL_LAYOUT_MAX]
    = { CHANNEL_LAYOUT_200 ,CHANNEL_LAYOUT_312  ,CHANNEL_LAYOUT_512,  CHANNEL_LAYOUT_MAX , };

  //unsigned char channel_layout_cb[CHANNEL_LAYOUT_MAX]
  //  = { CHANNEL_LAYOUT_200 ,CHANNEL_LAYOUT_510 ,CHANNEL_LAYOUT_512 ,CHANNEL_LAYOUT_MAX , };

  //unsigned char channel_layout_cb[CHANNEL_LAYOUT_MAX]
  //  = { CHANNEL_LAYOUT_200 ,CHANNEL_LAYOUT_510 ,CHANNEL_LAYOUT_710 ,CHANNEL_LAYOUT_MAX , };
  //char *in_file = "sine1k.wav";
  char *in_file = "replace_audio.wav";
  int recon_gain_flag = 1;
  int output_gain_flag = 1;
  int is_fragment_mp4 = 0;
  int codec_id = IA_DEP_CODEC_OPUS;

  char out_file[256];
  char *ptr;
  char prefix[256] = { 0 };
  ptr = strrchr(in_file, '.');
  if (ptr == 0) {
    if (strlen(in_file)<256)
      strncpy(prefix, in_file, strlen(in_file));
  }
  else {
    if (ptr - in_file<256)
      strncpy(prefix, in_file, ptr - in_file);
  }
  sprintf(out_file, "%s.mp4", prefix);

#endif



  MOVMuxContext *movm;
  if ((movm = mov_write_open(out_file)) == NULL)
  {
    fprintf(stderr, "Couldn't create MP4 output file.\n");
    return (0);
  }

  //********************** dep codec test***************************//
  movm->codec_id = codec_id;
  //********************** dep codec test***************************//

  //********************** fragment mp4 test***************************//
  if(is_fragment_mp4)
    movm->flags = IA_MOV_FLAG_FRAGMENT;
  movm->max_fragment_duration = 10000; //ms
 //********************** fragment mp4 test***************************//

  if (mov_write_trak(movm, 0, 1) < 0)
  {
    fprintf(stderr, "Couldn't create media track.\n");
    exit(-1);
  }



  //////////////////////read PCM data from wav[start]///////////////////////////
  void *in_wavf = NULL;
  char in_wav[255] = {0};
  if(strlen(in_file)<255)
    strncpy(in_wav, in_file, strlen(in_file));
  //sprintf(in_wav, "%s.wav", in_file);
  in_wavf = wav_read_open(in_wav);
  if (!in_wavf)
  {
    fprintf(stderr, "Could not open input file %s\n");
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
  if(codec_id == IA_DEP_CODEC_OPUS)
    chunk_size = 960;
  else if (codec_id == IA_DEP_CODEC_AAC)
    chunk_size = 1024;

  int bsize_of_samples = chunk_size * channels * bits_per_sample / 8;
  int bsize_of_1sample = channels * bits_per_sample / 8;
  int bsize_of_1ch_float_samples = chunk_size * sizeof(float);
  int bsize_of_float_samples = chunk_size * channels * sizeof(float);
  unsigned char * wav_samples = (unsigned char*)malloc(bsize_of_samples *sizeof(unsigned char));

  clock_t start_t, stop_t;
  /**
  * 1. Create immersive encoder handle.
  * */
  int error = 0;
  IAEncoder *ia_enc = immersive_audio_encoder_create(sample_rate,
    channel_layout_in, // orignal channels
    channel_layout_cb,  // channel layout combination
    codec_id,  //0:opus, 1:aac
    &error);

  /**
  * 2. immersive encoder control.
  * */
  immersive_audio_encoder_ctl(ia_enc, IA_SET_RECON_GAIN_FLAG((int)(recon_gain_flag)));
  immersive_audio_encoder_ctl(ia_enc, IA_SET_OUTPUT_GAIN_FLAG((int)output_gain_flag));
  //immersive_audio_encoder_ctl(ia_enc, IA_SET_SUBSTREAM_SIZE_FLAG(0));
  immersive_audio_encoder_ctl(ia_enc, IA_SET_SCALE_FACTOR_MODE((int)(2)));
  //immersive_audio_encoder_ctl(ia_enc, IA_SET_TEMP_DOWNMIX_FILE, in_file); //temp code. Need to remove in the future

  /**
  * 3. ASC and HEQ pre-process.
  * */
  if (index > 0) // non-scalable
  {
    start_t = clock();
    immersive_audio_encoder_dmpd_start(ia_enc);
    int pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    while (pcm_frames)
    {
      immersive_audio_encoder_dmpd_process(ia_enc, wav_samples, chunk_size);
      pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);

    }
    immersive_audio_encoder_dmpd_stop(ia_enc);
    stop_t = clock();
    printf("dmpd total time %f(s)\n",(float)(stop_t-start_t)/CLOCKS_PER_SEC);
  }

  if (in_wavf)
    wav_read_close(in_wavf);


  start_t = clock();
  /**
  * 4. loudness and gain pre-process.
  * */
  immersive_audio_encoder_loudness_gain_start(ia_enc);
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
    immersive_audio_encoder_loudness_gain(ia_enc, wav_samples, chunk_size);

    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    wr = (wav_reader_s *)in_wavf;
    current = total - wr->data_length;
    float pct = ((float)current / 1000) / ((float)total / 1000) * 100;
    if (bar._x < (int)pct)
      bar.proceedBar(&bar, (int)pct);
  }
  bar.endBar(&bar, 100);

  immersive_audio_encoder_loudness_gain_stop(ia_enc);

  /**
  * 5. gaindown and calculate scalable factor.
  * */
  immersive_audio_encoder_gaindown(ia_enc);
  immersive_audio_encoder_scalefactor(ia_enc);

  if (in_wavf)
    wav_read_close(in_wavf);



/////////////////////////audio trak setting, start//////////////////////////////////////////////

  //audio trak
  unsigned char sub_bitstream_count_map[] = { 1,1,4,5,6,5,6,7,4 };
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  mov_audio_track *audio_t = movm->audio_trak;
  audio_t[0].samplerate = 48000;
  audio_t[0].channels = channel_map714[channel_layout_in];
  audio_t[0].bits = 16;
  //audio_t[0].aiac.codec_id = 0;
  //audio_t[0].aiac.sub_bitstream_count = sub_bitstream_count_map[channel_layout_in]; //
  // codec specific info
  if (movm->codec_id == IA_DEP_CODEC_OPUS)
  {
    OpusHeader *header = (OpusHeader *)audio_t[0].aiac.csc;
    header->output_channel_count = 2;
    header->preskip = 312;
    header->input_sample_rate = 48000;
    header->output_gain = 0;
    header->channel_mapping_family = 0;
  }
  else if (movm->codec_id == IA_DEP_CODEC_AAC)
  {
    AacHeader *header = (AacHeader *)audio_t[0].aiac.csc;
    header->sample_rate = 48000;
    header->object_type = 2;
    header->sampling_index = 3;//48khz
    header->chan_config = 2; // 2 channels
  }
  else
  {
    printf("wrong codec input\n");
  }

  /**
  * 6. get immersive audio static metadata info.
  * */
  IA_STATIC_METADATA ia_static_metadata = get_immersive_audio_encoder_ia_static_metadata(ia_enc);
  audio_t[0].ia_static_meta.ambisonics_mode = ia_static_metadata.ambisonics_mode;
  audio_t[0].ia_static_meta.channel_audio_layer = ia_static_metadata.channel_audio_layer;
  audio_t[0].ia_static_meta.ambisonics_mode = ia_static_metadata.ambisonics_mode;
  for (int i = 0; i < ia_static_metadata.channel_audio_layer; i++)
  {
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].loudspeaker_layout = ia_static_metadata.channel_audio_layer_config[i].loudspeaker_layout;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].output_gain_is_present_flag = ia_static_metadata.channel_audio_layer_config[i].output_gain_is_present_flag;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].recon_gain_is_present_flag = ia_static_metadata.channel_audio_layer_config[i].recon_gain_is_present_flag;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].substream_count = ia_static_metadata.channel_audio_layer_config[i].substream_count;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].coupled_substream_count = ia_static_metadata.channel_audio_layer_config[i].coupled_substream_count;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].loudness = ia_static_metadata.channel_audio_layer_config[i].loudness;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].output_gain_flags = ia_static_metadata.channel_audio_layer_config[i].output_gain_flags;
    audio_t[0].ia_static_meta.ch_audio_layer_config[i].output_gain = ia_static_metadata.channel_audio_layer_config[i].output_gain;
  }
  /////////////////////////audio trak setting, end//////////////////////////////////////////////

  /**
  * 7. immersive audio encode.
  * */
  uint64_t frame_count = 0;
  mov_write_head(movm);
  unsigned char encode_ia[MAX_PACKET_SIZE * 3];
  in_wavf = wav_read_open(in_wav);
  pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
  int demix_mode = 0;
  while (pcm_frames)
  {
    int encoded_size = immersive_audio_encode(ia_enc, wav_samples, chunk_size, encode_ia, &demix_mode, MAX_PACKET_SIZE);
    mov_write_audio2(movm, 0, encode_ia, encoded_size, chunk_size, demix_mode);

    pcm_frames = wav_read_data(in_wavf, (unsigned char *)wav_samples, bsize_of_samples);
    frame_count ++;
  }



  stop_t = clock();
  printf("encoding total time %f(s), total count: %ld\n",(float)(stop_t-start_t)/CLOCKS_PER_SEC, frame_count);
  
  mov_write_tail(movm);
  mov_write_close(movm);

  /**
  * 7. destroy channel group encoder handle.
  * */
  immersive_audio_encoder_destroy(ia_enc);
  if (wav_samples)
  {
    free(wav_samples);
  }
failure:
  if (in_wavf)
    wav_read_close(in_wavf);


  return 0;

}
