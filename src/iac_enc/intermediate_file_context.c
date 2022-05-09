#include "immersive_audio_encoder_private.h"

void ia_intermediate_file_writeopen(IAEncoder *st, int file_type, const char* file_name)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  switch (file_type)
  {
  case 0:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_downmix_m_wav[map].file = wav_write_open3(downmix_m_wav[map], WAVE_FORMAT_FLOAT2, st->input_sample_rate, 16 * 2, channel_map714[map]);
        st->fc.f_downmix_m_wav[map].info.channels = channel_map714[map];
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, downmix_m_wav[map]))
        {
          st->fc.f_downmix_m_wav[map].file = wav_write_open3(downmix_m_wav[map], WAVE_FORMAT_FLOAT2, st->input_sample_rate, 16 * 2, channel_map714[map]);
          st->fc.f_downmix_m_wav[map].info.channels = channel_map714[map];
          break;
        }
      }
    }
    break;
  case 1:
    if (!strcmp(file_name, "ALL"))
    {
      int pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_downmix_s_wav[map].file = wav_write_open3(downmix_s_wav[map], WAVE_FORMAT_FLOAT2, st->input_sample_rate, 16 * 2, channel_map714[map] - pre_ch);
        st->fc.f_downmix_s_wav[map].info.channels = channel_map714[map] - pre_ch;
        pre_ch = channel_map714[map];

      }
    }
    else
    {
      int pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, downmix_s_wav[map]))
        {
          st->fc.f_downmix_s_wav[map].file = wav_write_open3(downmix_s_wav[map], WAVE_FORMAT_FLOAT2, st->input_sample_rate, 16 * 2, channel_map714[map] - pre_ch);
          st->fc.f_downmix_s_wav[map].info.channels = channel_map714[map] - pre_ch;
          break;
        }
        pre_ch = channel_map714[map];
      }
    }
    break;
  case 2:
    if (!strcmp(file_name, "ALL"))
    {
      int pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_gaindown_wav[map].file = wav_write_open3(gaindown_wav[map], WAVE_FORMAT_PCM2, st->input_sample_rate, 16, channel_map714[map] - pre_ch);
        st->fc.f_gaindown_wav[map].info.channels = channel_map714[map] - pre_ch;
        pre_ch = channel_map714[map];
      }
    }
    else
    {
      int pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, gaindown_wav[map]))
        {
          st->fc.f_gaindown_wav[map].file = wav_write_open3(gaindown_wav[map], WAVE_FORMAT_PCM2, st->input_sample_rate, 16, channel_map714[map] - pre_ch);
          st->fc.f_gaindown_wav[map].info.channels = channel_map714[map] - pre_ch;
          break;
        }
        pre_ch = channel_map714[map];
      }
    }
    break;
  case 3:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_upmix_wav[map].file = wav_write_open3(upmix_wav[map], WAVE_FORMAT_FLOAT2, st->input_sample_rate, 16 * 2, channel_map714[map]);
        st->fc.f_upmix_wav[map].info.channels = channel_map714[map];
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, upmix_wav[map]))
        {
          st->fc.f_upmix_wav[map].file = wav_write_open3(upmix_wav[map], WAVE_FORMAT_FLOAT2, st->input_sample_rate, 16 * 2, channel_map714[map]);
          st->fc.f_upmix_wav[map].info.channels = channel_map714[map];
          break;
        }
      }
    }
    break;
  case 4:
    if (!strcmp(file_name, "ALL"))
    {
      int pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_decoded_wav[map].file = wav_write_open3(decoded_wav[map], WAVE_FORMAT_PCM2, st->input_sample_rate, 16, channel_map714[map] - pre_ch);
        st->fc.f_decoded_wav[map].info.channels = channel_map714[map] - pre_ch;
        pre_ch = channel_map714[map];
      }
    }
    else
    {
      int pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, decoded_wav[map]))
        {
          st->fc.f_decoded_wav[map].file = wav_write_open3(decoded_wav[map], WAVE_FORMAT_PCM2, st->input_sample_rate, 16, channel_map714[map] - pre_ch);
          st->fc.f_decoded_wav[map].info.channels = channel_map714[map] - pre_ch;
          break;
        }
        pre_ch = channel_map714[map];
      }
    }
    break;
  case 5:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_encoded_ia[map].file = (void*)fopen(encoded_ia[map], "wb");
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, encoded_ia[map]))
        {
          st->fc.f_encoded_ia[map].file = (void*)fopen(encoded_ia[map], "wb");
          break;
        }
      }
    }
    break;
  case 6:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_scalefactor_cfg[map].file = (void*)fopen(scalefactor_cfg[map], "wb");
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, scalefactor_cfg[map]))
        {
          st->fc.f_scalefactor_cfg[map].file = (void*)fopen(scalefactor_cfg[map], "wb");
          break;
        }
      }
    }
    break;
  default:
    printf("wrong type inputing");
    break;
  }
}

void ia_intermediate_file_readopen(IAEncoder *st, int file_type, const char* file_name)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  switch (file_type)
  {
  case 0:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_downmix_m_wav[map].file = wav_read_open2(downmix_m_wav[map]);
        wav_get_header2(st->fc.f_downmix_m_wav[map].file, NULL, &(st->fc.f_downmix_m_wav[map].info.channels), NULL, &(st->fc.f_downmix_m_wav[map].info.bits_per_sample), NULL);
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, downmix_m_wav[map]))
        {
          st->fc.f_downmix_m_wav[map].file = wav_read_open2(downmix_m_wav[map]);
          wav_get_header2(st->fc.f_downmix_m_wav[map].file, NULL, &(st->fc.f_downmix_m_wav[map].info.channels), NULL, &(st->fc.f_downmix_m_wav[map].info.bits_per_sample), NULL);
          break;
        }
      }
    }
    break;
  case 1:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_downmix_s_wav[map].file = wav_read_open2(downmix_s_wav[map]);
        wav_get_header2(st->fc.f_downmix_s_wav[map].file, NULL, &(st->fc.f_downmix_s_wav[map].info.channels), NULL, &(st->fc.f_downmix_s_wav[map].info.bits_per_sample), NULL);
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, downmix_s_wav[map]))
        {
          st->fc.f_downmix_s_wav[map].file = wav_read_open2(downmix_s_wav[map]);
          wav_get_header2(st->fc.f_downmix_s_wav[map].file, NULL, &(st->fc.f_downmix_s_wav[map].info.channels), NULL, &(st->fc.f_downmix_s_wav[map].info.bits_per_sample), NULL);
          break;
        }
      }
    }
    break;
  case 2:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_gaindown_wav[map].file = wav_read_open2(gaindown_wav[map]);
        wav_get_header2(st->fc.f_gaindown_wav[map].file, NULL, &(st->fc.f_gaindown_wav[map].info.channels), NULL, &(st->fc.f_gaindown_wav[map].info.bits_per_sample), NULL);
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, gaindown_wav[map]))
        {
          st->fc.f_gaindown_wav[map].file = wav_read_open2(gaindown_wav[map]);
          wav_get_header2(st->fc.f_gaindown_wav[map].file, NULL, &(st->fc.f_gaindown_wav[map].info.channels), NULL, &(st->fc.f_gaindown_wav[map].info.bits_per_sample), NULL);
          break;
        }
      }
    }
    break;
  case 3:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_upmix_wav[map].file = wav_read_open2(upmix_wav[map]);
        wav_get_header2(st->fc.f_upmix_wav[map].file, NULL, &(st->fc.f_upmix_wav[map].info.channels), NULL, &(st->fc.f_upmix_wav[map].info.bits_per_sample), NULL);
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, upmix_wav[map]))
        {
          st->fc.f_upmix_wav[map].file = wav_read_open2(upmix_wav[map]);
          wav_get_header2(st->fc.f_upmix_wav[map].file, NULL, &(st->fc.f_upmix_wav[map].info.channels), NULL, &(st->fc.f_upmix_wav[map].info.bits_per_sample), NULL);
          break;
        }
      }
    }
    break;
  case 4:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_decoded_wav[map].file = wav_read_open2(decoded_wav[map]);
        wav_get_header2(st->fc.f_decoded_wav[map].file, NULL, &(st->fc.f_decoded_wav[map].info.channels), NULL, &(st->fc.f_decoded_wav[map].info.bits_per_sample), NULL);
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, decoded_wav[map]))
        {
          st->fc.f_decoded_wav[map].file = wav_read_open2(decoded_wav[map]);
          wav_get_header2(st->fc.f_decoded_wav[map].file, NULL, &(st->fc.f_decoded_wav[map].info.channels), NULL, &(st->fc.f_decoded_wav[map].info.bits_per_sample), NULL);
          break;
        }
      }
    }
    break;
  case 5:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_encoded_ia[map].file = (void*)fopen(encoded_ia[map], "rb");
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, encoded_ia[map]))
        {
          st->fc.f_encoded_ia[map].file = (void*)fopen(encoded_ia[map], "rb");
          break;
        }
      }
    }
    break;
  case 6:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        st->fc.f_scalefactor_cfg[map].file = (void*)fopen(scalefactor_cfg[map], "rb");
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (!strcmp(file_name, scalefactor_cfg[map]))
        {
          st->fc.f_scalefactor_cfg[map].file = (void*)fopen(scalefactor_cfg[map], "rb");
          break;
        }
      }
    }
    break;
  default:
    printf("wrong type inputing");
    break;
  }
}

void ia_intermediate_file_write(IAEncoder *st, int file_type, const char* file_name, void * input, int size)
{
  switch (file_type)
  {
  case 0:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, downmix_m_wav[i]))
      {
        int size_ = st->fc.f_downmix_m_wav[i].info.channels * size *sizeof(opus_int16) * 2;//float PCM
        wav_write_data2(st->fc.f_downmix_m_wav[i].file, (unsigned char *)input, size_);
        break;
      }
    }
    break;
  case 1:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, downmix_s_wav[i]))
      {
        int size_ = st->fc.f_downmix_s_wav[i].info.channels * size *sizeof(opus_int16) * 2;//float PCM
        wav_write_data2(st->fc.f_downmix_s_wav[i].file, (unsigned char *)input, size_);
        break;
      }
    }
    break;
  case 2:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, gaindown_wav[i]))
      {
        int size_ = st->fc.f_gaindown_wav[i].info.channels * size *sizeof(opus_int16);// PCM
        wav_write_data2(st->fc.f_gaindown_wav[i].file, (unsigned char *)input, size_);
        break;
      }
    }
    break;
  case 3:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, upmix_wav[i]))
      {
        int size_ = st->fc.f_upmix_wav[i].info.channels * size *sizeof(opus_int16) * 2;//float PCM
        wav_write_data2(st->fc.f_upmix_wav[i].file, (unsigned char *)input, size_);
        break;
      }
    }
    break;
  case 4:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, decoded_wav[i]))
      {
        int size_ = st->fc.f_decoded_wav[i].info.channels * size *sizeof(opus_int16);// PCM
        wav_write_data2(st->fc.f_decoded_wav[i].file, (unsigned char *)input, size_);
        break;
      }
    }
    break;
  case 5:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, encoded_ia[i]))
      {
        int *p = &size;
        fwrite(p, sizeof(int), 1, st->fc.f_encoded_ia[i].file);
        fwrite((unsigned char *)input, size, 1, st->fc.f_encoded_ia[i].file);
        break;
      }
    }
    break;
  case 6:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, scalefactor_cfg[i]))
      {
        fwrite(input, size, 1, st->fc.f_scalefactor_cfg[i].file);
        break;
      }
    }
    break;
  default:
    printf("wrong type inputting");
    break;
  }
}

int ia_intermediate_file_read(IAEncoder *st, int file_type, const char* file_name, void * output, int size)
{
  int length = 0;
  switch (file_type)
  {
  case 0:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, downmix_m_wav[i]))
      {
        int size_ = st->fc.f_downmix_m_wav[i].info.channels * size *sizeof(opus_int16) * 2;//float PCM
        length = wav_read_data2(st->fc.f_downmix_m_wav[i].file, (unsigned char *)output, size_);
        break;
      }
    }
    break;
  case 1:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, downmix_s_wav[i]))
      {
        int size_ = st->fc.f_downmix_s_wav[i].info.channels * size *sizeof(opus_int16) * 2;//float PCM
        length = wav_read_data2(st->fc.f_downmix_s_wav[i].file, (unsigned char *)output, size_);
        break;
      }
    }
    break;
  case 2:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, gaindown_wav[i]))
      {
        int size_ = st->fc.f_gaindown_wav[i].info.channels * size *sizeof(opus_int16);// PCM
        length = wav_read_data2(st->fc.f_gaindown_wav[i].file, (unsigned char *)output, size_);
        break;
      }
    }
    break;
  case 3:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, upmix_wav[i]))
      {
        int size_ = st->fc.f_upmix_wav[i].info.channels * size *sizeof(opus_int16) * 2;//float PCM
        length = wav_read_data2(st->fc.f_upmix_wav[i].file, (unsigned char *)output, size_);
        break;
      }
    }
    break;
  case 4:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, decoded_wav[i]))
      {
        int size_ = st->fc.f_decoded_wav[i].info.channels * size *sizeof(opus_int16);// PCM
        length = wav_read_data2(st->fc.f_decoded_wav[i].file, (unsigned char *)output, size_);
        break;
      }
    }
    break;
  case 5:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, encoded_ia[i]))
      {
        length = fread((unsigned char *)output, size, 1, st->fc.f_encoded_ia[i].file);
        break;
      }
    }
    break;
  case 6:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (!strcmp(file_name, scalefactor_cfg[i]))
      {
        length = fread(output, size, 1, st->fc.f_scalefactor_cfg[i].file);
        break;
      }
    }
    break;
  default:
    printf("wrong type inputting");
    break;
  }
  return length;
}

void ia_intermediate_file_writeclose(IAEncoder *st, int file_type, const char* file_name)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  switch (file_type)
  {
  case 0:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_downmix_m_wav[map].file)
        {
          wav_write_close2(st->fc.f_downmix_m_wav[map].file);
          st->fc.f_downmix_m_wav[map].file = NULL;
        }

      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, downmix_m_wav[i]))
        {
          if (st->fc.f_downmix_m_wav[i].file)
          {
            wav_write_close2(st->fc.f_downmix_m_wav[i].file);
            st->fc.f_downmix_m_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 1:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_downmix_s_wav[map].file)
        {
          wav_write_close2(st->fc.f_downmix_s_wav[map].file);
          st->fc.f_downmix_s_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, downmix_s_wav[i]))
        {
          if (st->fc.f_downmix_s_wav[i].file)
          {
            wav_write_close2(st->fc.f_downmix_s_wav[i].file);
            st->fc.f_downmix_s_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 2:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_gaindown_wav[map].file)
        {
          wav_write_close2(st->fc.f_gaindown_wav[map].file);
          st->fc.f_gaindown_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, gaindown_wav[i]))
        {
          if (st->fc.f_gaindown_wav[i].file)
          {
            wav_write_close2(st->fc.f_gaindown_wav[i].file);
            st->fc.f_gaindown_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 3:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_upmix_wav[map].file)
        {
          wav_write_close2(st->fc.f_upmix_wav[map].file);
          st->fc.f_upmix_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, upmix_wav[i]))
        {
          if (st->fc.f_upmix_wav[i].file)
          {
            wav_write_close2(st->fc.f_upmix_wav[i].file);
            st->fc.f_upmix_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 4:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_decoded_wav[map].file)
        {
          wav_write_close2(st->fc.f_decoded_wav[map].file);
          st->fc.f_decoded_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, decoded_wav[i]))
        {
          if (st->fc.f_decoded_wav[i].file)
          {
            wav_write_close2(st->fc.f_decoded_wav[i].file);
            st->fc.f_decoded_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 5:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_encoded_ia[map].file)
        {
          fclose(st->fc.f_encoded_ia[map].file);
          st->fc.f_encoded_ia[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, encoded_ia[i]))
        {
          if (st->fc.f_encoded_ia[i].file)
          {
            fclose(st->fc.f_encoded_ia[i].file);
            st->fc.f_encoded_ia[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 6:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_scalefactor_cfg[map].file)
        {
          fclose(st->fc.f_scalefactor_cfg[map].file);
          st->fc.f_scalefactor_cfg[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, scalefactor_cfg[i]))
        {
          if (st->fc.f_scalefactor_cfg[i].file)
          {
            fclose(st->fc.f_scalefactor_cfg[i].file);
            st->fc.f_scalefactor_cfg[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  default:
    printf("wrong type inputing");
    break;
  }
}

void ia_intermediate_file_readclose(IAEncoder *st, int file_type, const char* file_name)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  switch (file_type)
  {
  case 0:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_downmix_m_wav[map].file)
        {
          wav_read_close2(st->fc.f_downmix_m_wav[map].file);
          st->fc.f_downmix_m_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, downmix_m_wav[i]))
        {
          if (st->fc.f_downmix_m_wav[i].file)
          {
            wav_read_close2(st->fc.f_downmix_m_wav[i].file);
            st->fc.f_downmix_m_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 1:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_downmix_s_wav[map].file)
        {
          wav_read_close2(st->fc.f_downmix_s_wav[map].file);
          st->fc.f_downmix_s_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, downmix_s_wav[i]))
        {
          if (st->fc.f_downmix_s_wav[i].file)
          {
            wav_read_close2(st->fc.f_downmix_s_wav[i].file);
            st->fc.f_downmix_s_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 2:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_gaindown_wav[map].file)
        {
          wav_read_close2(st->fc.f_gaindown_wav[map].file);
          st->fc.f_gaindown_wav[map].file = NULL;
        }

      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, gaindown_wav[i]))
        {
          if (st->fc.f_gaindown_wav[i].file)
          {
            wav_read_close2(st->fc.f_gaindown_wav[i].file);
            st->fc.f_gaindown_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 3:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_upmix_wav[map].file)
        {
          wav_read_close2(st->fc.f_upmix_wav[map].file);
          st->fc.f_upmix_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, upmix_wav[i]))
        {
          if (st->fc.f_upmix_wav[i].file)
          {
            wav_read_close2(st->fc.f_upmix_wav[i].file);
            st->fc.f_upmix_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 4:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_decoded_wav[map].file)
        {
          wav_read_close2(st->fc.f_decoded_wav[map].file);
          st->fc.f_decoded_wav[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, decoded_wav[i]))
        {
          if (st->fc.f_decoded_wav[i].file)
          {
            wav_read_close2(st->fc.f_decoded_wav[i].file);
            st->fc.f_decoded_wav[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 5:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_encoded_ia[map].file)
        {
          fclose(st->fc.f_encoded_ia[map].file);
          st->fc.f_encoded_ia[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, encoded_ia[i]))
        {
          if (st->fc.f_encoded_ia[i].file)
          {
            fclose(st->fc.f_encoded_ia[i].file);
            st->fc.f_encoded_ia[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  case 6:
    if (!strcmp(file_name, "ALL"))
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        unsigned char map = st->channel_layout_map[i];
        if (map == CHANNEL_LAYOUT_MAX)
          break;
        if (st->fc.f_scalefactor_cfg[map].file)
        {
          fclose(st->fc.f_scalefactor_cfg[map].file);
          st->fc.f_scalefactor_cfg[map].file = NULL;
        }
      }
    }
    else
    {
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        if (!strcmp(file_name, scalefactor_cfg[i]))
        {
          if (st->fc.f_scalefactor_cfg[i].file)
          {
            fclose(st->fc.f_scalefactor_cfg[i].file);
            st->fc.f_scalefactor_cfg[i].file = NULL;
          }
          break;
        }
      }
    }
    break;
  default:
    printf("wrong type inputing");
    break;
  }
}
