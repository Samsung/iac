#include "immersive_audio_encoder.h"
#include "immersive_audio_encoder_private.h"


/*****************************************************OPUS********************************************/
//opus headers
#include "opus_multistream.h"
#include "opus_projection.h"

static int opus_encode_init(IAEncoder *st)
{
  int ret = 0;
  unsigned char def_stream_map[255] = { 0,1 };
  for (int i = 0; i < 255; i++)
    def_stream_map[i] = i;
  int error = 0;
  int bitrate = 0;
  int stream_count = 0, coupled_stream_count = 0;
  int mapping_family = 0;

  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
    {
      if (j < st->ia_encoder_dcg[i].coupled_stream_count)
      {
        if ((st->ia_encoder_dcg[i].dep_encoder[j] = opus_multistream_surround_encoder_create(st->input_sample_rate,
          2,//channels
          mapping_family,//mapping_family
          &stream_count,//streams
          &coupled_stream_count,//coupled_streams
          def_stream_map,
          OPUS_APPLICATION_AUDIO,
          &error)) == NULL)
        {
          fprintf(stderr, "can not initialize opus encoder.\n");
          exit(-1);
        }
        bitrate = 128000;
      }
      else
      {
        if ((st->ia_encoder_dcg[i].dep_encoder[j] = opus_multistream_surround_encoder_create(st->input_sample_rate,
          1,//channels
          mapping_family,//mapping_family
          &stream_count,//streams
          &coupled_stream_count,//coupled_streams
          def_stream_map,
          OPUS_APPLICATION_AUDIO,
          &error)) == NULL)
        {
          fprintf(stderr, "can not initialize opus encoder.\n");
          exit(-1);
        }
        bitrate = 64000;
      }
      //set param

      opus_multistream_encoder_ctl(st->ia_encoder_dcg[i].dep_encoder[j], OPUS_SET_BITRATE(bitrate));
      opus_multistream_encoder_ctl(st->ia_encoder_dcg[i].dep_encoder[j], OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
      opus_multistream_encoder_ctl(st->ia_encoder_dcg[i].dep_encoder[j], OPUS_SET_VBR(1));
      opus_multistream_encoder_ctl(st->ia_encoder_dcg[i].dep_encoder[j], OPUS_SET_COMPLEXITY(10));
      int32_t preskip = 0;
      opus_multistream_encoder_ctl(st->ia_encoder_dcg[i].dep_encoder[j], OPUS_GET_LOOKAHEAD(&preskip));
      opus_multistream_encoder_ctl(st->ia_encoder_dcg[i].dep_encoder[j], IA_SET_FORCE_MODE(IA_MODE_CELT_ONLY));
    }
  }
  return 0;
}

static int opus_encode_frame(IAEncoder *st, int cg, int stream, int channels, int16_t *pcm_data, unsigned char* encoded_frame)
{
  int encoded_size;
  encoded_size = opus_multistream_encode(st->ia_encoder_dcg[cg].dep_encoder[stream],
    pcm_data,
    st->frame_size,
    encoded_frame,
    MAX_PACKET_SIZE);
  return encoded_size;
}

static int opus_encode_close(IAEncoder *st)
{
  int ret = 0;
  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
    {
      if (st->ia_encoder_dcg[i].dep_encoder[j])
      {
        opus_multistream_encoder_destroy(st->ia_encoder_dcg[i].dep_encoder[j]);
        st->ia_encoder_dcg[i].dep_encoder[j] = NULL;
      }
    }
  }
  return ret;
}


static int opus_decode_init(IAEncoder *st)
{
  int ret = 0;
  int error = 0;
  unsigned char def_stream_map[255] = { 0, };
  for (int i = 0; i < 255; i++)
    def_stream_map[i] = i;

  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_decoder_dcg[i].stream_count; j++)
    {
      if (j < st->ia_encoder_dcg[i].coupled_stream_count)
      {
        st->ia_decoder_dcg[i].dep_decoder[j] = opus_multistream_decoder_create(st->input_sample_rate, 2,
          1,
          1,
          def_stream_map,
          &error);
        if (error != 0)
        {
          fprintf(stderr, "opus_decoder_create failed %d", error);
        }
      }
      else
      {
        st->ia_decoder_dcg[i].dep_decoder[j] = opus_multistream_decoder_create(st->input_sample_rate, 1,
          1,
          0,
          def_stream_map,
          &error);
        if (error != 0)
        {
          fprintf(stderr, "opus_decoder_create failed %d", error);
        }
      }
    }
  }
  return ret;
}

static int opus_decode_frame(IAEncoder *st, int cg, int stream, int channels, unsigned char* encoded_frame, int encoded_size, int16_t *pcm_data)
{
  int decoded_size;
  decoded_size = sizeof(int16_t) * channels * st->frame_size;
  int ret;
  decoded_size = sizeof(int16_t) * channels * st->frame_size;
  ret = opus_multistream_decode(st->ia_decoder_dcg[cg].dep_decoder[stream],
    encoded_frame, encoded_size, (int16_t*)pcm_data, decoded_size, 0);
  return ret;
}

static int opus_decode_close(IAEncoder *st)
{
  int ret = 0;

  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_decoder_dcg[i].stream_count; j++)
    {
      opus_multistream_decoder_destroy(st->ia_decoder_dcg[i].dep_decoder[j]);
    }
  }
  return ret;
}




/*****************************************************AAC********************************************/
//aac headers
#include "aacenc_lib.h"
#include "aacdecoder_lib.h"

#define FF_PROFILE_AAC_LOW  1

static const char *aac_get_error(AACENC_ERROR err)
{
  switch (err) {
  case AACENC_OK:
    return "No error";
  case AACENC_INVALID_HANDLE:
    return "Invalid handle";
  case AACENC_MEMORY_ERROR:
    return "Memory allocation error";
  case AACENC_UNSUPPORTED_PARAMETER:
    return "Unsupported parameter";
  case AACENC_INVALID_CONFIG:
    return "Invalid config";
  case AACENC_INIT_ERROR:
    return "Initialization error";
  case AACENC_INIT_AAC_ERROR:
    return "AAC library initialization error";
  case AACENC_INIT_SBR_ERROR:
    return "SBR library initialization error";
  case AACENC_INIT_TP_ERROR:
    return "Transport library initialization error";
  case AACENC_INIT_META_ERROR:
    return "Metadata library initialization error";
  case AACENC_ENCODE_ERROR:
    return "Encoding error";
  case AACENC_ENCODE_EOF:
    return "End of file";
  default:
    return "Unknown error";
  }
}

static int aac_encode_init(IAEncoder *st)
{
  AACENC_ERROR err;
  int ret = 0;
  int aot = FF_PROFILE_AAC_LOW + 1; //2: MPEG-4 AAC Low Complexity.
  int sce = 0, cpe = 0;
  int afterburner = 1;
  int signaling = 0;
  int mode = MODE_UNKNOWN;
  int64_t bit_rate = 0;
  int cutoff = 0;
  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
    {
      if (j < st->ia_encoder_dcg[i].coupled_stream_count)
      {
        sce = 0;
        cpe = 1;
        mode = MODE_2;
        if ((err = aacEncOpen(&(st->ia_encoder_dcg[i].dep_encoder[j]), 0, 2)) != AACENC_OK) {
          printf("Unable to open the encoder: %s\n", aac_get_error(err));
          goto error;
        }
      }
      else
      {
        sce = 1;
        cpe = 0;
        mode = MODE_1;
        if ((err = aacEncOpen(&(st->ia_encoder_dcg[i].dep_encoder[j]), 0, 1)) != AACENC_OK) {
          printf("Unable to open the encoder: %s\n");
          goto error;
        }
      }
      //set param
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_AOT, aot)) != AACENC_OK) {
        printf("Unable to set the AOT %d: %s\n", aot, aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_SAMPLERATE,
        st->input_sample_rate)) != AACENC_OK) {
        printf("Unable to set the sample rate %d: %s\n", st->input_sample_rate, aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_CHANNELMODE, mode)) != AACENC_OK) {
        printf("Unable to set channel mode %d: %s\n", mode, aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_CHANNELORDER, 1)) != AACENC_OK) {
        printf("Unable to set wav channel order %d: %s\n", mode, aac_get_error(err));
        goto error;
      }
#if 0
      bit_rate = (96 * sce + 128 * cpe) * st->input_sample_rate / 44;
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_BITRATE, bit_rate)) != AACENC_OK) {
        printf("Unable to set the bitrate %lld: %s\n", bit_rate, aac_get_error(err));
        goto error;
      }
#else
      int vbr = 4;
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_BITRATEMODE, vbr)) != AACENC_OK) {
        printf("Unable to set the bitrate %lld: %s\n", bit_rate, aac_get_error(err));
        goto error;
      }
#endif
      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_TRANSMUX, TT_MP4_RAW)) != AACENC_OK) {
        printf("Unable to set the transmux format: %s\n", aac_get_error(err));
        goto error;
      }

      if ((err = aacEncoder_SetParam(st->ia_encoder_dcg[i].dep_encoder[j], AACENC_AFTERBURNER, afterburner)) != AACENC_OK) {
        printf("Unable to set afterburner to %d: %s\n", afterburner, aac_get_error(err));
        goto error;
      }

      if ((err = aacEncEncode(st->ia_encoder_dcg[i].dep_encoder[j], NULL, NULL, NULL, NULL)) != AACENC_OK) {
        printf("Unable to initialize the encoder: %s\n", aac_get_error(err));
        goto error;
      }
    }
  }
  return ret;


error:
  ret = -1;
  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
    {
      if (st->ia_encoder_dcg[i].dep_encoder[j])
      {
        aacEncClose(&(st->ia_encoder_dcg[i].dep_encoder[j]));
        st->ia_encoder_dcg[i].dep_encoder[j] = 0;
      }
    }
  }
  return ret;
}

static int aac_encode_frame(IAEncoder *st, int cg, int stream, int channels, int16_t *pcm_data, unsigned char* encoded_frame)
{
  int encoded_size;
  AACENC_ERROR err;

  int in_identifier = IN_AUDIO_DATA;
  int out_identifier = OUT_BITSTREAM_DATA;
  AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
  AACENC_InArgs in_args = { 0 };
  AACENC_OutArgs out_args = { 0 };
  int in_elem_size = sizeof(int16_t);
  int in_size = channels * in_elem_size * st->frame_size;
  in_args.numInSamples = st->frame_size * channels;
  in_buf.numBufs = 1;
  in_buf.bufs = &pcm_data;
  in_buf.bufferIdentifiers = &in_identifier;
  in_buf.bufSizes = &in_size;
  in_buf.bufElSizes = &in_elem_size;

  int out_size, out_elem_size;
  out_size = MAX_PACKET_SIZE;
  out_elem_size = 1;
  out_buf.numBufs = 1;
  out_buf.bufs = &encoded_frame;
  out_buf.bufferIdentifiers = &out_identifier;
  out_buf.bufSizes = &out_size;
  out_buf.bufElSizes = &out_elem_size;

  if ((err = aacEncEncode(st->ia_encoder_dcg[cg].dep_encoder[stream], &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK)
  {
    if (err == AACENC_ENCODE_EOF)
    {
      fprintf(stderr, "Encoding failed\n");
      return -1;
    }
    else if (!out_args.numOutBytes)
      return 0;
  }
  encoded_size = out_args.numOutBytes;
  return encoded_size;
}

static int aac_encode_close(IAEncoder *st)
{
  AACENC_ERROR err;
  int ret = 0;
  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
    {
      aacEncClose(&(st->ia_encoder_dcg[i].dep_encoder[j]));
      st->ia_encoder_dcg[i].dep_encoder[j] = 0;
    }
  }
  return ret;
}


static int aac_decode_init(IAEncoder *st)
{
  int ret = 0;
  AAC_DECODER_ERROR err;
  /*
  object type; f(5)
  frequency index; f(4)
  channel configuration; f(4)
  GASpecificConfig{
  frameLengthFlag; f(1)
  dependsOnCoreCoder; f(1)
  extensionFlag; f(1)
  }
  */
  //UCHAR extra_data_s[10] = { 0x11,0x88 }; //single stream
  //UCHAR extra_data_c[10] = { 0x11,0x90 };; //coupled stream
  UINT extra_data_size = 2;
  UCHAR *extra_data_s = (UCHAR *)malloc(extra_data_size*sizeof(extra_data_s));
  UCHAR *extra_data_c = (UCHAR *)malloc(extra_data_size*sizeof(extra_data_s));
  extra_data_s[0] = 0x11; extra_data_s[1] = 0x88;
  extra_data_c[0] = 0x11; extra_data_c[1] = 0x90;
  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_decoder_dcg[i].stream_count; j++)
    {
      st->ia_decoder_dcg[i].dep_decoder[j] = aacDecoder_Open(TT_MP4_RAW, 1);
      if (aacDecoder_SetParam(st->ia_decoder_dcg[i].dep_decoder[j], AAC_CONCEAL_METHOD, 1) != AAC_DEC_OK)
      {
        fprintf(stderr, "Unable to set error concealment method\n");
        return -1;
      }
#if 1
      if (!st->ia_decoder_dcg[i].dep_decoder[j])
      {
        fprintf(stderr, "Error opening decoder\n");
      }
      if (j < st->ia_encoder_dcg[i].coupled_stream_count)
      {
        if ((err = aacDecoder_ConfigRaw(st->ia_decoder_dcg[i].dep_decoder[j], &extra_data_c,
          &extra_data_size)) != AAC_DEC_OK) {
          fprintf(stderr, "Unable to set extradata\n");
          return -1;
        }
      }
      else
      {
        if ((err = aacDecoder_ConfigRaw(st->ia_decoder_dcg[i].dep_decoder[j], &extra_data_s,
          &extra_data_size)) != AAC_DEC_OK) {
          fprintf(stderr, "Unable to set extradata\n");
          return -1;
        }
      }
#endif
    }
  }
  if (extra_data_s)
    free(extra_data_s);
  if (extra_data_c)
    free(extra_data_c);
  return ret;
}

static int aac_decode_frame(IAEncoder *st, int cg, int stream, int channels, unsigned char* encoded_frame, int encoded_size, int16_t *pcm_data)
{
  int decoded_size;
  decoded_size = sizeof(int16_t) * channels * st->frame_size;
  int ret;
  AAC_DECODER_ERROR err;
  UINT valid = encoded_size;
  err = aacDecoder_Fill(st->ia_decoder_dcg[cg].dep_decoder[stream], &encoded_frame, &encoded_size, &valid);
  if (err != AAC_DEC_OK) {
    printf("aacDecoder_Fill() failed: %x\n", err);
    return -1;
  }
  err = aacDecoder_DecodeFrame(st->ia_decoder_dcg[cg].dep_decoder[stream], (INT_PCM *)pcm_data, decoded_size, 0);
  if (err != AAC_DEC_OK) {
    printf("aacDecoder_DecodeFrame() failed: %x\n", err);
    return -1;
  }
  ret = decoded_size;
  return ret;
}

static int aac_decode_close(IAEncoder *st)
{
  int ret = 0;

  for (int i = 0; i < st->channel_groups; i++)
  {
    for (int j = 0; j < st->ia_decoder_dcg[i].stream_count; j++)
    {
      if (st->ia_decoder_dcg[i].dep_decoder[j])
        aacDecoder_Close(st->ia_decoder_dcg[i].dep_decoder[j]);
    }
  }
  return ret;
}


encode_creator_t dep_encoders[] = {
  { IA_DEP_CODEC_OPUS, opus_encode_init, opus_encode_frame, opus_encode_close },
  { IA_DEP_CODEC_AAC, aac_encode_init, aac_encode_frame, aac_encode_close },
  { -1, NULL, NULL }
};

decode_creator_t dep_decoders[] = {
  { IA_DEP_CODEC_OPUS, opus_decode_init, opus_decode_frame, opus_decode_close },
  { IA_DEP_CODEC_AAC, aac_decode_init, aac_decode_frame, aac_decode_close },
  { -1, NULL, NULL }
};