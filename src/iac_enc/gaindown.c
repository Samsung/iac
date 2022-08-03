#include "gaindown.h"
#include "fixedp11_5.h"
static float DmixTypeMat[][4] = {
  { 1.0f, 1.0f, 0.707f, 0.707f },     //type1
  { 0.707f, 0.707f, 0.707f, 0.707f },  //type2
  { 1.0f, 0.866f, 0.866f, 0.866f } };	// type3

/*
a
b
t
p
q
*/
void gaindown(float *downmix_s[CHANNEL_LAYOUT_MAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, float *dmixgain_f, int frame_size)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };

  unsigned char pre_ch = 0;
  unsigned char base_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    int channels = channel_map714[lay_out] - pre_ch;
    for (int j = 0; j < channels; j++)
    {
      if (gain_down_map[base_ch + j] == 0)
        continue;
      for (int k = 0; k < frame_size; k++)
      {
        downmix_s[lay_out][j*frame_size + k] = downmix_s[lay_out][j*frame_size + k] * dmixgain_f[lay_out];
      }
    }
    base_ch += channels;
    pre_ch = channel_map714[lay_out];
  }
}

/*
abtpq...
*/
void gaindown2(float *downmix_s[CHANNEL_LAYOUT_MAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, float *dmixgain_f, int frame_size)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };

  unsigned char pre_ch = 0;
  unsigned char base_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    int channels = channel_map714[lay_out] - pre_ch;
    for (int j = 0; j < channels; j++)
    {
      if (gain_down_map[base_ch + j] == 0)
        continue;
      for (int k = 0; k < frame_size; k++)
      {
        downmix_s[lay_out][j + k*channels] = downmix_s[lay_out][j + k*channels] * dmixgain_f[lay_out];
      }
    }
    base_ch += channels;
    pre_ch = channel_map714[lay_out];
  }
}