#include "downmixer.h"
#include <math.h>

union trans2char
{
  float f;
  unsigned char c[4];
};

static float DmixTypeMat[][4] = {
  { 1.0f, 1.0f, 0.707f, 0.707f },     //type1
  { 0.707f, 0.707f, 0.707f, 0.707f },  //type2
  { 1.0f, 0.866f, 0.866f, 0.866f } };	// type3

static float calc_w(int weighttypenum)
{
  // weighttypenum == 0: 0,  weighttypenum != 0: 1.0 ???
  float w_x;
  if (weighttypenum)
    w_x = fminf((float)weighttypenum + 0.1, 1.0);
  else
    w_x = fmaxf((float)weighttypenum - 0.1, 0.0);

  float w_y, w_z;
  float factor = (float)(1) / 3;

  if (w_x <= 1.0)
  {
    if (w_x - 0.5 < 0)
      w_y = pow(((0.5 - w_x) / 4), factor) * (-1) + 0.5;
    else
      w_y = pow(((w_x - 0.5) / 4), factor) + 0.5;
  }
  else
  {
    if (w_x - 0.5 -1 < 0)
      w_y = pow(((0.5 + 1 - w_x) / 4), factor) * (-1) + 0.5;
    else
      w_y = pow(((w_x - 0.5 - 1) / 4), factor) + 0.5 +1;
  }
  w_z = w_y * 0.5;
  return (w_z);
}

static float calc_w_v2(int weighttypenum, float w_x_prev, float *w_x)
{
  // weighttypenum == 0: 0,  weighttypenum != 0: 1.0 ???
  if (weighttypenum)
    *w_x = fminf(w_x_prev + 0.1, 1.0);
  else
    *w_x = fmaxf(w_x_prev - 0.1, 0.0);

  float w_y, w_z;
  float factor = (float)(1) / 3;

  if (*w_x <= 1.0)
  {
    if (*w_x - 0.5 < 0)
      w_y = pow(((0.5 - *w_x) / 4), factor) * (-1) + 0.5;
    else
      w_y = pow(((*w_x - 0.5) / 4), factor) + 0.5;
  }
  else
  {
    if (*w_x - 0.5 - 1 < 0)
      w_y = pow(((0.5 + 1 - *w_x) / 4), factor) * (-1) + 0.5;
    else
      w_y = pow(((*w_x - 0.5 - 1) / 4), factor) + 0.5 + 1;
  }
  w_z = w_y * 0.5;
  return (w_z);
}

void conv_downmixpcm(unsigned char *pcmbuf, void* dspbuf, int nch, int size, int frame_size)
{
  int16_t *buff = (int16_t*)pcmbuf;

  float(*outbuff)[IA_FRAME_MAXSIZE] = (float(*)[IA_FRAME_MAXSIZE])dspbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < frame_size; j++)
    {
      if (j < size)
        outbuff[i][j] = (float)(buff[i + j*nch]) / 32768.0f;
      else
        outbuff[i][j] = 0.0f;
    }
  }
}

// pcmbuf:
// a1a2a3    wavbuf:
// b1b2b3 -> a1b1c1 a2b2c2 a3b3c3 
// c1c2c3 

//
void convert_preskip_pcm(float * outbuffer, void * inbuffer, int ch, int frame_size, int preskip_size)
{
  float(*dspin)[IA_FRAME_MAXSIZE] = (float(*)[IA_FRAME_MAXSIZE])inbuffer;
  for (int i = 0; i < ch; i++)
  {
    for (int j = 0; j < preskip_size; j++)
    {
      outbuffer[i*frame_size + j] = outbuffer[ch * frame_size + i*preskip_size + j];
    }
  }
  for (int i = 0; i < ch; i++)
  {
    for (int j = preskip_size; j < frame_size; j++)
    {
      outbuffer[i*frame_size + j] = dspin[i][j - preskip_size];
    }
  }

  for (int i = 0; i < ch; i++)
  {
    for (int j = 0; j < preskip_size; j++)
    {
      outbuffer[ch * frame_size + i*preskip_size + j] = dspin[i][j + frame_size - preskip_size];
    }
  }
}

void convert_out_pcm(float * outbuffer, void * inbuffer, int ch, int frame_size)
{
  float(*dspin)[IA_FRAME_MAXSIZE] = (float(*)[IA_FRAME_MAXSIZE])inbuffer;
  for (int i = 0; i < ch; i++)
  {
    for (int j = 0; j < frame_size; j++)
    {
      outbuffer[i*frame_size + j] = dspin[i][j];
    }
  }
}

typedef struct
{
  int opcode;
  void *data;
} creator_t;

static int downmix_h4to2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < dm->frame_size; i++)
  {
    dm->buffer[enc_channel_mixed_h_l][i] = dm->ch_data[enc_channel_hfl][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_hbl][i];
    dm->buffer[enc_channel_mixed_h_r][i] = dm->ch_data[enc_channel_hfr][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_hbr][i];
  }
  dm->ch_data[enc_channel_hl] = dm->buffer[enc_channel_mixed_h_l];
  dm->ch_data[enc_channel_hr] = dm->buffer[enc_channel_mixed_h_r];

  dm->gaindown_map[CHANNEL_LAYOUT_512][enc_channel_hl] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_512][enc_channel_hr] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_712][enc_channel_hl] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_712][enc_channel_hr] = 1;
  return 0;
}

static int downmix_h2tofh2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  float w_z = calc_w_v2(weight_type, dm->weight_state_value_x_prev, w_x);

  for (int i = 0; i < dm->frame_size; i++)
  {
    dm->buffer[enc_channel_mixed_t_l][i] = dm->ch_data[enc_channel_hl][i] + DmixTypeMat[type_id][3] * w_z* dm->ch_data[enc_channel_sl5][i];
    dm->buffer[enc_channel_mixed_t_r][i] = dm->ch_data[enc_channel_hr][i] + DmixTypeMat[type_id][3] * w_z* dm->ch_data[enc_channel_sr5][i];
  }
  dm->ch_data[enc_channel_tl] = dm->buffer[enc_channel_mixed_t_l];
  dm->ch_data[enc_channel_tr] = dm->buffer[enc_channel_mixed_t_r];

  dm->gaindown_map[CHANNEL_LAYOUT_312][enc_channel_tl] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_312][enc_channel_tr] = 1;

  return 0;
}

static int downmix_h2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (dm->ch_data[enc_channel_hfl])
  return downmix_h4to2(dm, dmix_type, weight_type, w_x);
  return 0;
}

static int downmix_fh2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (!dm->ch_data[enc_channel_hl])
    downmix_h2(dm, dmix_type, weight_type, w_x);
  if(dm->ch_data[enc_channel_hl])
    return downmix_h2tofh2(dm, dmix_type, weight_type, w_x);
  return 0;
}

static int downmix_s7to5(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < dm->frame_size; i++)
  {
    dm->buffer[enc_channel_mixed_s5_l][i] = DmixTypeMat[type_id][0] * dm->ch_data[enc_channel_sl7][i] + DmixTypeMat[type_id][1] * dm->ch_data[enc_channel_bl7][i];
    dm->buffer[enc_channel_mixed_s5_r][i] = DmixTypeMat[type_id][0] * dm->ch_data[enc_channel_sr7][i] + DmixTypeMat[type_id][1] * dm->ch_data[enc_channel_br7][i];
  }
  dm->ch_data[enc_channel_sl5] = dm->buffer[enc_channel_mixed_s5_l];
  dm->ch_data[enc_channel_sr5] = dm->buffer[enc_channel_mixed_s5_r];

  dm->gaindown_map[CHANNEL_LAYOUT_510][enc_channel_sl5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_510][enc_channel_sr5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_512][enc_channel_sl5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_512][enc_channel_sr5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_514][enc_channel_sl5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_514][enc_channel_sr5] = 1;

  return 0;
}

static int downmix_s5(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if(dm->ch_data[enc_channel_sl7])
    return downmix_s7to5(dm, dmix_type, weight_type, w_x);
  return 0;
}

static int downmix_s5to3(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < dm->frame_size; i++)
  {
    dm->buffer[enc_channel_mixed_s3_l][i] = dm->ch_data[enc_channel_l5][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_sl5][i];
    dm->buffer[enc_channel_mixed_s3_r][i] = dm->ch_data[enc_channel_r5][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_sr5][i];
  }
  dm->ch_data[enc_channel_l3] = dm->buffer[enc_channel_mixed_s3_l];
  dm->ch_data[enc_channel_r3] = dm->buffer[enc_channel_mixed_s3_r];

  dm->gaindown_map[CHANNEL_LAYOUT_312][enc_channel_l3] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_312][enc_channel_r3] = 1;

  return 0;
}

static int downmix_s3to2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < dm->frame_size; i++)
  {
    dm->buffer[enc_channel_mixed_s2_l][i] = dm->ch_data[enc_channel_l3][i] + 0.707 * dm->ch_data[enc_channel_c][i];
    dm->buffer[enc_channel_mixed_s2_r][i] = dm->ch_data[enc_channel_r3][i] + 0.707 * dm->ch_data[enc_channel_c][i];
  }
  dm->ch_data[enc_channel_l2] = dm->buffer[enc_channel_mixed_s2_l];
  dm->ch_data[enc_channel_r2] = dm->buffer[enc_channel_mixed_s2_r];
  dm->gaindown_map[CHANNEL_LAYOUT_200][enc_channel_l2] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_200][enc_channel_r2] = 1;

  return 0;
}

static int downmix_s2to1(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < dm->frame_size; i++)
  {
    dm->buffer[enc_channel_mixed_s1_m][i] = 0.5 * dm->ch_data[enc_channel_l2][i] + 0.5 * dm->ch_data[enc_channel_r2][i];
  }
  dm->ch_data[enc_channel_mono] = dm->buffer[enc_channel_mixed_s1_m];
  dm->gaindown_map[CHANNEL_LAYOUT_100][enc_channel_mono] = 1;
  return 0;
}

static int downmix_s3(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if(!dm->ch_data[enc_channel_sl5])
    downmix_s5(dm, dmix_type, weight_type, w_x);
  if(dm->ch_data[enc_channel_sl5])
    downmix_s5to3(dm, dmix_type, weight_type, w_x);
  return 0;
}

static int downmix_s2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (!dm->ch_data[enc_channel_l3])
    downmix_s3(dm, dmix_type, weight_type, w_x);
  if(dm->ch_data[enc_channel_l3])
    downmix_s3to2(dm, dmix_type, weight_type, w_x);
  return 0;
}

static int downmix_s1(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (!dm->ch_data[enc_channel_l2])
    downmix_s2(dm, dmix_type, weight_type, w_x);
  if (dm->ch_data[enc_channel_l2])
    downmix_s2to1(dm, dmix_type, weight_type, w_x);

  return 0;
}

void downmix_to714(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  return;
}

void downmix_to712(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_h2(dm, dmix_type, weight_type, w_x);
  return;
}

void downmix_to710(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  return;
}

void downmix_to514(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_s5(dm, dmix_type, weight_type, w_x);
  return;
}

void downmix_to512(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_s5(dm, dmix_type, weight_type, w_x);
  downmix_h2(dm, dmix_type, weight_type, w_x);
  return;
}

void downmix_to510(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_s5(dm, dmix_type, weight_type, w_x);
  return;
}

void downmix_to312(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_s3(dm, dmix_type, weight_type, w_x);
  downmix_fh2(dm, dmix_type, weight_type, w_x);
  return;
}

void downmix_to200(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_s2(dm, dmix_type, weight_type, w_x);
  return;
}

void downmix_to100(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  downmix_s1(dm, dmix_type, weight_type, w_x);
  return;
}

static creator_t g_downmix[] = {
  { CHANNEL_LAYOUT_100, downmix_to100 },
  { CHANNEL_LAYOUT_200, downmix_to200 },
  { CHANNEL_LAYOUT_510, downmix_to510 },
  { CHANNEL_LAYOUT_512, downmix_to512 },
  { CHANNEL_LAYOUT_514, downmix_to514 },
  { CHANNEL_LAYOUT_710, downmix_to710 },
  { CHANNEL_LAYOUT_712, downmix_to712 },
  { CHANNEL_LAYOUT_714, downmix_to714 },
  { CHANNEL_LAYOUT_312, downmix_to312 },
  { -1 }
};

int downmix2(DownMixer *dm, unsigned char* inbuffer, int size, int dmix_type, int weight_type)
{

  int ret = 0;
  float dspInBuf[12][IA_FRAME_MAXSIZE];
  float tmp[12][IA_FRAME_MAXSIZE];
  float weight_state_value_x_curr = 0.0;
  uint8_t *playout;
  int channel;
  conv_downmixpcm(inbuffer, dspInBuf, dm->channels, size, dm->frame_size);

  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char pre_ch = 0;



  ///////////////

  int last_layout = 0;
  int last_index = 0;
  int ts_index = 0;
  int ts_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = dm->channel_layout_map[i];

    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    last_layout = lay_out;
    last_index = i;
  }

  for (int i = 0; i < enc_channel_cnt; i++)
  {
    dm->ch_data[i] = NULL;
  }

  int chs = enc_get_layout_channel_count(last_layout);
  for (int i = 0; i < chs; i++)
  {
    uint8_t *tchs = enc_get_layout_channels(last_layout);
    int ch = tchs[i];
    dm->ch_data[ch] = dspInBuf[i];
    //printf("%s (%d) \n", up_get_channel_name(channel), channel);
  }
  for (int i = last_index; i >= 0; i--)
  {
    int lay_out = dm->channel_layout_map[i];
    if(g_downmix[lay_out].data)
      ((void(*)(DownMixer *dm, int dmix_type, int weight_type, float *w_x))g_downmix[lay_out].data)
      (dm, dmix_type, weight_type, &weight_state_value_x_curr);
  }


  for (int i = 0; i <= last_index; i++)
  {
    int layout = dm->channel_layout_map[i];
    ts_ch = 0;
    playout = enc_get_layout_channels(layout);
    //downmix_m
    for (int ch = 0; ch < enc_get_layout_channel_count(layout); ++ch)
    {
      channel = playout[ch];
      if (!dm->ch_data[channel]) {
        printf("channel %d doesn't has data.\n", playout[ch]);
        continue;
      }
      memcpy(tmp[ch], dm->ch_data[channel], sizeof(float) * dm->frame_size);
    }
    //convert_preskip_pcm(dm->downmix_m[layout], tmp, enc_get_layout_channel_count(layout), dm->frame_size, dm->preskip_size);
    convert_out_pcm(dm->downmix_m[layout], tmp, enc_get_layout_channel_count(layout), dm->frame_size);
    //downmix_s
    for (int ch = pre_ch; ch < enc_get_layout_channel_count(layout); ++ch)
    {
      int s_index = 0;
      for (s_index = 0; s_index < enc_get_layout_channel_count(layout); s_index++)
      {
        if (dm->channel_order[ch] == playout[s_index])
        {
          memcpy(&(dm->downmix_s[layout][(ch - pre_ch) * dm->frame_size]), tmp[s_index], sizeof(float) * dm->frame_size);
          break;
        }
      }
      if (s_index == enc_get_layout_channel_count(layout))// mono case
      {
        memcpy(&(dm->downmix_s[layout][(ch - pre_ch) * dm->frame_size]), dm->ch_data[enc_channel_l2], sizeof(float) * dm->frame_size);
      }
    }
    pre_ch = enc_get_layout_channel_count(layout);
  }


  dm->weight_state_value_x_prev = weight_state_value_x_curr;
  return 0;
}

DownMixer * downmix_create(const unsigned char *channel_layout_map, int frame_size)
{
  DownMixer *dm = (DownMixer*)malloc(sizeof(DownMixer));
  if(!dm)return NULL;
  memset(dm, 0x00, sizeof(DownMixer));

  memcpy(dm->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_MAX);
  dm->frame_size = frame_size;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = dm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    dm->downmix_m[layout] = (float *)malloc((frame_size) * MAX_CHANNELS * sizeof(float));
    if(!dm->downmix_m[layout])goto FAILED;
    memset(dm->downmix_m[layout], 0x00, (frame_size) * MAX_CHANNELS * sizeof(float));
    dm->downmix_s[layout] = (float *)malloc(frame_size * MAX_CHANNELS * sizeof(float));
    if(!dm->downmix_s[layout])goto FAILED;
    memset(dm->downmix_s[layout], 0x00, frame_size * MAX_CHANNELS * sizeof(float));
  }

  for (int i = 0; i < enc_channel_mixed_cnt; i++)
  {
    dm->buffer[i] = (float *)malloc(frame_size * sizeof(float));
    if(!dm->buffer[i])goto FAILED;
    memset(dm->buffer[i], 0x00, frame_size * sizeof(float));
  }


  int idx = 0, ret = 0;;
  int last_cl_layout = CHANNEL_LAYOUT_INVALID;
  uint8_t new_channels[256];

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = dm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    ret = enc_get_new_channels2(last_cl_layout, layout, new_channels);

    for (int i = idx, j = 0; j<ret; ++i, ++j) {
      dm->channel_order[i] = new_channels[j];
    }
    idx += ret;

    last_cl_layout = layout;
  }

  dm->channels = enc_get_layout_channel_count(last_cl_layout);
  dm->weight_state_value_x_prev = 0.0;
  return dm;
FAILED:
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = dm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    if(dm->downmix_m[layout])
      free(dm->downmix_m[layout]);
    if(dm->downmix_s[layout])
      free(dm->downmix_s[layout]);
  }

  for (int i = 0; i < enc_channel_mixed_cnt; i++)
  {
    if(dm->buffer[i])
    free(dm->buffer[i]);
  }
  if(dm)
    free(dm);
  return NULL;
}

void downmix_clear(DownMixer *dm)
{
  dm->weight_state_value_x_prev = 0.0;
}

void downmix_destroy(DownMixer *dm)
{
  if (dm)
  {
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (dm->downmix_m[i])
        free(dm->downmix_m[i]);
      if(dm->downmix_s[i])
        free(dm->downmix_s[i]);
    }
    for (int i = 0; i < enc_channel_mixed_cnt; i++)
    {
      if (dm->buffer[i])
        free(dm->buffer[i]);
    }

    free(dm);
  }
}
