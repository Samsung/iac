#include "scalable_format.h"

int enc_get_layout_channel_count(int type)
{
  return enc_gs_layout_channel_count[type];
}


uint8_t* enc_get_layout_channels(int type)
{
  return  (uint8_t *)enc_gs_layout_channels[type];
}

uint8_t* enc_get_layout_channels2(int type)
{
  return  (uint8_t *)enc_gs_layout_channels2[type];
}

int enc_convert_12channel(int ch)
{
  return enc_gs_12channel[ch];
}

int enc_has_c_channel(int cnt, uint8_t *channels)
{
  for (int i = 0; i<cnt; ++i)
    if (channels[i] == enc_channel_c)
      return i;
  return -1;

}

const char* enc_get_channel_name(uint32_t ch)
{
  if (ch < enc_channel_cnt)
    return enc_gs_ia_channel_name[ch];
  return "unknown";
}


int enc_get_new_channels(int base, int target, uint8_t* channels)
{
  int tcnt = enc_get_layout_channel_count(target);
  uint8_t *tchs = NULL;
  if (base == CHANNEL_LAYOUT_INVALID)
    tchs = enc_get_layout_channels(target);
  else
    tchs = enc_get_layout_channels(target);
  int idx = 0;

  if (base == CHANNEL_LAYOUT_INVALID) {
    for (int ti = 0; ti<tcnt; ++ti) {
      channels[ti] = tchs[ti];
    }
    idx = tcnt;
  }
  else {
    int bcnt = enc_get_layout_channel_count(base);
    uint8_t *bchs = enc_get_layout_channels(base);
    int bi;
    for (int ti = 0; ti<tcnt; ++ti) {
      for (bi = 0; bi<bcnt; ++bi) {
        if (enc_convert_12channel(tchs[ti]) == enc_convert_12channel(bchs[bi]))
          break;
      }

      if (bi == bcnt)
        channels[idx++] = tchs[ti];
    }

  }
#if 0
  if (idx > 0) {
    //printf("new channel:\n");
    for (int i = 0; i<idx; ++i) {
      printf("%s\n", enc_get_channel_name(channels[i]));
    }
    //printf("\n");
  }
#endif
  return idx;
}

int get_surround_channels(int lay_out)
{
  int ret;
  switch (lay_out)
  {
  case FORMAT_CHANNEL_LAYOUT_100:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_200:
    ret = 2;
    break;
  case FORMAT_CHANNEL_LAYOUT_510:
    ret = 5;
    break;
  case FORMAT_CHANNEL_LAYOUT_512:
    ret = 5;
    break;
  case FORMAT_CHANNEL_LAYOUT_514:
    ret = 5;
    break;
  case FORMAT_CHANNEL_LAYOUT_710:
    ret = 7;
    break;
  case FORMAT_CHANNEL_LAYOUT_712:
    ret = 7;
    break;
  case FORMAT_CHANNEL_LAYOUT_714:
    ret = 7;
    break;
  case FORMAT_CHANNEL_LAYOUT_312:
    ret = 3;
    break;
  default:
    printf("wrong inputing\n");
    break;
  }
  return ret;
}

int get_height_channels(int lay_out)
{
  int ret;
  switch (lay_out)
  {
  case FORMAT_CHANNEL_LAYOUT_100:
    ret = 0;
    break;
  case FORMAT_CHANNEL_LAYOUT_200:
    ret = 0;
    break;
  case FORMAT_CHANNEL_LAYOUT_510:
    ret = 0;
    break;
  case FORMAT_CHANNEL_LAYOUT_512:
    ret = 2;
    break;
  case FORMAT_CHANNEL_LAYOUT_514:
    ret = 4;
    break;
  case FORMAT_CHANNEL_LAYOUT_710:
    ret = 0;
    break;
  case FORMAT_CHANNEL_LAYOUT_712:
    ret = 2;
    break;
  case FORMAT_CHANNEL_LAYOUT_714:
    ret = 4;
    break;
  case FORMAT_CHANNEL_LAYOUT_312:
    ret = 2;
    break;
  default:
    printf("wrong inputing\n");
    break;
  }
  return ret;
}

int get_lfe_channels(int lay_out)
{
  int ret;
  switch (lay_out)
  {
  case FORMAT_CHANNEL_LAYOUT_100:
    ret = 0;
    break;
  case FORMAT_CHANNEL_LAYOUT_200:
    ret = 0;
    break;
  case FORMAT_CHANNEL_LAYOUT_510:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_512:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_514:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_710:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_712:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_714:
    ret = 1;
    break;
  case FORMAT_CHANNEL_LAYOUT_312:
    ret = 1;
    break;
  default:
    printf("wrong inputing\n");
    break;
  }
  return ret;
}


int enc_get_new_channels2(int last_layout, int current_layout, uint8_t* channels)
{
  int index = 0;
  if (last_layout == CHANNEL_LAYOUT_INVALID)
  {
    uint8_t *tchs = NULL;
    int nch = enc_get_layout_channel_count(current_layout);
    tchs = enc_get_layout_channels2(current_layout);
    for (int i = 0; i < nch; ++i)
    {
      channels[index++] = tchs[i];
    }
    return index;
  }
  int s_set[2] = { 0,0 }, w_set[2] = { 0,0 }, h_set[2] = { 0,0 };

  uint8_t channels_end[12] = { 0 }; // save to end of the new channels
  s_set[0] = get_surround_channels(last_layout);
  s_set[1] = get_surround_channels(current_layout);
  w_set[0] = get_lfe_channels(last_layout);
  w_set[1] = get_lfe_channels(current_layout);
  h_set[0] = get_height_channels(last_layout);
  h_set[1] = get_height_channels(current_layout);


  //surround channels
  for (int j = s_set[0] + 1; j <= s_set[1]; j++)
  {
    /*if (j == 2)
    {
      channels[index++] = enc_channel_l2;
    }
    else */if (j == 5)
    {
      if (s_set[1] == 7)
      {
        channels[index++] = enc_channel_l7;
        channels[index++] = enc_channel_r7;
      }
      else if (s_set[1] == 5)
      {
        channels[index++] = enc_channel_l5;
        channels[index++] = enc_channel_r5;
      }
    }
    else if (j == 7)
    {
      channels[index++] = enc_channel_sl7;
      channels[index++] = enc_channel_sr7;
    }
  }
  //height channels
  if (h_set[1] > h_set[0])
  {
    if (h_set[0] == 0)
    {
      uint8_t height_s3_2[2] = { enc_channel_tl ,enc_channel_tr };
      uint8_t height_s5_2[2] = { enc_channel_hl ,enc_channel_hr };
      uint8_t height_s5_4[4] = { enc_channel_hfl ,enc_channel_hfr, enc_channel_hbl, enc_channel_hbr };
      uint8_t height_s7_2[2] = { enc_channel_hl ,enc_channel_hr };
      uint8_t height_s7_4[4] = { enc_channel_hfl ,enc_channel_hfr, enc_channel_hbl, enc_channel_hbr };
      if (s_set[1] == 3 && h_set[1] == 2)
      {
        for (int j = 0; j < h_set[1]; j++)
        {
          channels[index++] = height_s3_2[j];
        }
      }
      else if (s_set[1] == 5 && h_set[1] == 2)
      {
        for (int j = 0; j < h_set[1]; j++)
        {
          channels[index++] = height_s5_2[j];
        }
      }
      else if (s_set[1] == 7 && h_set[1] == 2)
      {
        for (int j = 0; j < h_set[1]; j++)
        {
          channels[index++] = height_s7_2[j];
        }
      }
      else if (s_set[1] == 5 && h_set[1] == 4)
      {
        for (int j = 0; j < h_set[1]; j++)
        {
          channels[index++] = height_s5_4[j];
        }
      }
      else if (s_set[1] == 7 && h_set[1] == 4)
      {
        for (int j = 0; j < h_set[1]; j++)
        {
          channels[index++] = height_s7_4[j];
        }
      }
    }
    else if (h_set[0] == 2)
    {
      channels[index++] = enc_channel_hfl;
      channels[index++] = enc_channel_hfr;
    }
  }
  // C
  for (int j = s_set[0] + 1; j <= s_set[1]; j++)
  {
    if (j == 3)
    {
      channels[index++] = enc_channel_c;
    }
  }
  // LFE
  if (w_set[1] > w_set[0])
  {
    channels[index++] = enc_channel_lfe;
  }
#if 1
  // L2
  for (int j = s_set[0] + 1; j <= s_set[1]; j++)
  {
    if (j == 2)
    {
      channels[index++] = enc_channel_l2;
    }
  }
#endif
  return index;

}