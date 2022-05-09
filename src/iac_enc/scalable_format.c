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
    tchs = enc_get_layout_channels2(target);
  int idx = 0;

  if (base == CHANNEL_LAYOUT_INVALID) {
    for (int ti = 0; ti<tcnt; ++ti) {
      channels[ti] = tchs[ti];
    }
    idx = tcnt;
  }
  else {
    int bcnt = enc_get_layout_channel_count(base);
    uint8_t *bchs = enc_get_layout_channels2(base);
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