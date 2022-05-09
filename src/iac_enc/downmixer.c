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

void downmix_714toM312(float dspInBuf[][FRAME_SIZE], void * dspOutBuf, int dmix_type, int weight_type)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  int type_id = dmix_type - 1;

  float w_z = calc_w(weight_type);

  for (int i = 0; i < FRAME_SIZE; i++)
  {
    float MSL = DmixTypeMat[type_id][0] * dspInBuf[gSL][i] + DmixTypeMat[type_id][1] * dspInBuf[gBL][i];
    float MSR = DmixTypeMat[type_id][0] * dspInBuf[gSR][i] + DmixTypeMat[type_id][1] * dspInBuf[gBR][i];
    float MHL = dspInBuf[gHL][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBL][i];
    float MHR = dspInBuf[gHR][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBR][i];

    dspOut[gML3][i] = dspInBuf[gL][i] + DmixTypeMat[type_id][3] * MSL;
    dspOut[gMR3][i] = dspInBuf[gR][i] + DmixTypeMat[type_id][3] * MSR;
    dspOut[gMC][i] = dspInBuf[gC][i];
    dspOut[gMLFE][i] = dspInBuf[gLFE][i];
    dspOut[gMHL3][i] = DmixTypeMat[type_id][3] * w_z * MSL + MHL;
    dspOut[gMHR3][i] = DmixTypeMat[type_id][3] * w_z * MSR + MHR;
  }
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

void downmix_714toM312_v2(float dspInBuf[][FRAME_SIZE], void * dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  int type_id = dmix_type - 1;

  float w_z = calc_w_v2(weight_type, w_x_prev, w_x);

  for (int i = 0; i < FRAME_SIZE; i++)
  {
    float MSL = DmixTypeMat[type_id][0] * dspInBuf[gSL][i] + DmixTypeMat[type_id][1] * dspInBuf[gBL][i];
    float MSR = DmixTypeMat[type_id][0] * dspInBuf[gSR][i] + DmixTypeMat[type_id][1] * dspInBuf[gBR][i];
    float MHL = dspInBuf[gHL][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBL][i];
    float MHR = dspInBuf[gHR][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBR][i];

    dspOut[gML3][i] = dspInBuf[gL][i] + DmixTypeMat[type_id][3] * MSL;
    dspOut[gMR3][i] = dspInBuf[gR][i] + DmixTypeMat[type_id][3] * MSR;
    dspOut[gMC][i] = dspInBuf[gC][i];
    dspOut[gMLFE][i] = dspInBuf[gLFE][i];
    dspOut[gMHL3][i] = DmixTypeMat[type_id][3] * w_z * MSL + MHL;
    dspOut[gMHR3][i] = DmixTypeMat[type_id][3] * w_z * MSR + MHR;
  }
}

void downmix_714toM512(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    float MSL = DmixTypeMat[type_id][0] * dspInBuf[gSL][i] + DmixTypeMat[type_id][1] * dspInBuf[gBL][i];
    float MSR = DmixTypeMat[type_id][0] * dspInBuf[gSR][i] + DmixTypeMat[type_id][1] * dspInBuf[gBR][i];
    float MHL = dspInBuf[gHL][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBL][i];
    float MHR = dspInBuf[gHR][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBR][i];

    dspOut[gML5][i] = dspInBuf[gL][i];
    dspOut[gMR5][i] = dspInBuf[gR][i];
    dspOut[gMC][i] = dspInBuf[gC][i];
    dspOut[gMLFE][i] = dspInBuf[gLFE][i];

    dspOut[gMSL5][i] = MSL;
    dspOut[gMSR5][i] = MSR;
    dspOut[gMHL5][i] = MHL;
    dspOut[gMHR5][i] = MHR;
  }
}

void downmix_714toM510(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    float MSL = DmixTypeMat[type_id][0] * dspInBuf[gSL][i] + DmixTypeMat[type_id][1] * dspInBuf[gBL][i];
    float MSR = DmixTypeMat[type_id][0] * dspInBuf[gSR][i] + DmixTypeMat[type_id][1] * dspInBuf[gBR][i];

    dspOut[gML5][i] = dspInBuf[gL][i];
    dspOut[gMR5][i] = dspInBuf[gR][i];
    dspOut[gMC][i] = dspInBuf[gC][i];
    dspOut[gMLFE][i] = dspInBuf[gLFE][i];

    dspOut[gMSL5][i] = MSL;
    dspOut[gMSR5][i] = MSR;
  }
}

void downmix_714toM514(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    float MSL = DmixTypeMat[type_id][0] * dspInBuf[gSL][i] + DmixTypeMat[type_id][1] * dspInBuf[gBL][i];
    float MSR = DmixTypeMat[type_id][0] * dspInBuf[gSR][i] + DmixTypeMat[type_id][1] * dspInBuf[gBR][i];

    dspOut[gML5][i] = dspInBuf[gL][i];
    dspOut[gMR5][i] = dspInBuf[gR][i];
    dspOut[gMC][i] = dspInBuf[gC][i];
    dspOut[gMLFE][i] = dspInBuf[gLFE][i];

    dspOut[gMSL5][i] = MSL;
    dspOut[gMSR5][i] = MSR;
    dspOut[gMHFL5][i] = dspInBuf[gHL][i];
    dspOut[gMHFR5][i] = dspInBuf[gHR][i];
    dspOut[gMHBL5][i] = dspInBuf[gHBL][i];
    dspOut[gMHBR5][i] = dspInBuf[gHBR][i];
  }
}

void downmix_714toM710(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      dspOut[j][i] = dspInBuf[j][i];
    }
  }
}

void downmix_714toM712(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      dspOut[j][i] = dspInBuf[j][i];
    }
    float MHL = dspInBuf[gHL][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBL][i];
    float MHR = dspInBuf[gHR][i] + DmixTypeMat[type_id][2] * dspInBuf[gHBR][i];

    dspOut[gHL][i] = MHL;
    dspOut[gHR][i] = MHR;
  }
}

void downmix_714toM714(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    for (int j = 0; j < 12; j++)
    {
      dspOut[j][i] = dspInBuf[j][i];
    }
  }
}

void downmix_714toM312_ko(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type)
{
  downmix_714toM312(dspInBuf, dspOutBuf, dmix_type, weight_type);
}

void downmix_714toM312_ko_v2(float dspInBuf[][FRAME_SIZE], void * dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  downmix_714toM312_v2(dspInBuf, dspOutBuf, dmix_type, weight_type, w_x_prev, w_x);
}

void downmix_714toM200(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  float dspOutBuf312[6][FRAME_SIZE];
  downmix_714toM312_ko_v2(dspInBuf, dspOutBuf312, dmix_type, weight_type, w_x_prev, w_x);
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dspOut[gA][i] = dspOutBuf312[gML3][i] + 0.707*dspOutBuf312[gMC][i];
    dspOut[gB][i] = dspOutBuf312[gMR3][i] + 0.707*dspOutBuf312[gMC][i];
  }
}

void downmix_714toMstereo(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  float dspOutBuf200[2][FRAME_SIZE];
  downmix_714toM200(dspInBuf, dspOutBuf200, dmix_type, weight_type, w_x_prev, w_x);
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dspOut[0][i] = 0.5*dspOutBuf200[gA][i] + 0.5*dspOutBuf200[gB][i];
  }
}

void downmix_714toS312_ko(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  float dspOutBuf312[6][FRAME_SIZE];
  downmix_714toM312_ko(dspInBuf, dspOutBuf312, dmix_type, weight_type);
  for (int i = 0; i < FRAME_SIZE; i++)
  {

    dspOut[gA][i] = dspOutBuf312[gML3][i] + 0.707*dspOutBuf312[gMC][i];
    dspOut[gB][i] = dspOutBuf312[gMR3][i] + 0.707*dspOutBuf312[gMC][i];

    dspOut[gT][i] = dspOutBuf312[gMC][i];
    dspOut[gP][i] = dspOutBuf312[gMLFE][i];

    dspOut[gQ1][i] = dspOutBuf312[gMHL3][i];
    dspOut[gQ2][i] = dspOutBuf312[gMHR3][i];
  }
}

void downmix_714toS312_ko_v2(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  float dspOutBuf312[6][FRAME_SIZE];
  downmix_714toM312_ko_v2(dspInBuf, dspOutBuf312, dmix_type, weight_type, w_x_prev, w_x);
  for (int i = 0; i < FRAME_SIZE; i++)
  {

    dspOut[gA][i] = dspOutBuf312[gML3][i] + 0.707*dspOutBuf312[gMC][i];
    dspOut[gB][i] = dspOutBuf312[gMR3][i] + 0.707*dspOutBuf312[gMC][i];

    dspOut[gT][i] = dspOutBuf312[gMC][i];
    dspOut[gP][i] = dspOutBuf312[gMLFE][i];

    dspOut[gQ1][i] = dspOutBuf312[gMHL3][i];
    dspOut[gQ2][i] = dspOutBuf312[gMHR3][i];
  }
}

void downmix_714toSUV_ko(float dspInBuf[][FRAME_SIZE], void* dspOutBuf)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dspOut[0][i] = dspInBuf[gL][i]; //#S1
    dspOut[1][i] = dspInBuf[gR][i]; //#S2
    dspOut[2][i] = dspInBuf[gSL][i]; //#U1
    dspOut[3][i] = dspInBuf[gSR][i]; //#U2
    dspOut[4][i] = dspInBuf[gHL][i]; //#V1
    dspOut[5][i] = dspInBuf[gHR][i]; //#V2
  }
}

void downmix_714toSxxx(float dspInBuf[][FRAME_SIZE], void* dspOutBuf, int dmix_type, int weight_type, float w_x_prev, float *w_x)
{
  float(*dspOut)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOutBuf;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dspOut[0][i] = dspInBuf[gL][i]; //#S1
    dspOut[1][i] = dspInBuf[gR][i]; //#S2
    dspOut[2][i] = dspInBuf[gSL][i]; //#U1
    dspOut[3][i] = dspInBuf[gSR][i]; //#U2
    dspOut[4][i] = dspInBuf[gHL][i]; //#V1
    dspOut[5][i] = dspInBuf[gHR][i]; //#V2
  }
}
#if 0
void conv_writtenpcm(float pcmbuf[][FRAME_SIZE], void *wavbuf, int nch, int shift)
{
  int16_t *wbuf = (int16_t *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      wbuf[i + j*nch] = (int16_t)(pcmbuf[i + shift][j] * 32767.0);
    }
  }
}

void conv_writtenfloat(float pcmbuf[][FRAME_SIZE], void *wavbuf, int nch, int shift)
{
  unsigned char *wbuf = (unsigned char *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      union trans2char trans;
      trans.f = pcmbuf[i + shift][j];
      wbuf[(i + j*nch) * 4] = trans.c[0];
      wbuf[(i + j*nch) * 4 + 1] = trans.c[1];
      wbuf[(i + j*nch) * 4 + 2] = trans.c[2];
      wbuf[(i + j*nch) * 4 + 3] = trans.c[3];
    }
  }
}
#endif
void conv_downmixpcm(unsigned char *pcmbuf, void* dspbuf, int nch)
{
  int16_t *buff = (int16_t*)pcmbuf;

  float(*outbuff)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      outbuff[i][j] = (float)(buff[i + j*nch]) / 32768.0f; /// why / 32768.0f??
    }
  }
}

// pcmbuf:
// a1a2a3    wavbuf:
// b1b2b3 -> a1b1c1 a2b2c2 a3b3c3 
// c1c2c3 

//
void convert_preskip_pcm(float * outbuffer, void * inbuffer, int ch)
{
  float(*dspin)[FRAME_SIZE] = (float(*)[FRAME_SIZE])inbuffer;
  for (int i = 0; i < ch; i++)
  {
    for (int j = 0; j < PRESKIP_SIZE; j++)
    {
      outbuffer[i*FRAME_SIZE + j] = outbuffer[ch * FRAME_SIZE + i*PRESKIP_SIZE + j];
    }
  }
  for (int i = 0; i < ch; i++)
  {
    for (int j = PRESKIP_SIZE; j < FRAME_SIZE; j++)
    {
      outbuffer[i*FRAME_SIZE + j] = dspin[i][j - PRESKIP_SIZE];
    }
  }

  for (int i = 0; i < ch; i++)
  {
    for (int j = 0; j < PRESKIP_SIZE; j++)
    {
      outbuffer[ch * FRAME_SIZE + i*PRESKIP_SIZE + j] = dspin[i][j + FRAME_SIZE - PRESKIP_SIZE];
    }
  }
}

typedef struct
{
  int opcode;
  void *data;
} creator_t;

static creator_t g_downmixm[] = {
  { CHANNEL_LAYOUT_D100, downmix_714toMstereo },
  { CHANNEL_LAYOUT_D200, downmix_714toM200 },
  { CHANNEL_LAYOUT_D510, downmix_714toM510 },
  { CHANNEL_LAYOUT_D512, downmix_714toM512 },
  { CHANNEL_LAYOUT_D514, downmix_714toM514 },
  { CHANNEL_LAYOUT_D710, downmix_714toM710 },
  { CHANNEL_LAYOUT_D712, downmix_714toM712 },
  { CHANNEL_LAYOUT_D714, downmix_714toM714 },
  { CHANNEL_LAYOUT_D312, downmix_714toM312_v2 },
  { -1 }
};

static int downmix_h4to2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dm->buffer[enc_channel_mixed_h_l][i] = dm->ch_data[enc_channel_hfl][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_hbl][i];
    dm->buffer[enc_channel_mixed_h_r][i] = dm->ch_data[enc_channel_hfr][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_hbr][i];
  }
  dm->ch_data[enc_channel_hl] = dm->buffer[enc_channel_mixed_h_l];
  dm->ch_data[enc_channel_hr] = dm->buffer[enc_channel_mixed_h_r];

  dm->gaindown_map[CHANNEL_LAYOUT_D512][enc_channel_hl] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D512][enc_channel_hr] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D712][enc_channel_hl] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D712][enc_channel_hr] = 1;
  return 0;
}

static int downmix_h2tofh2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  float w_z = calc_w_v2(weight_type, dm->weight_state_value_x_prev, w_x);

  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dm->buffer[enc_channel_mixed_t_l][i] = dm->ch_data[enc_channel_hl][i] + DmixTypeMat[type_id][3] * w_z* dm->ch_data[enc_channel_sl5][i];
    dm->buffer[enc_channel_mixed_t_r][i] = dm->ch_data[enc_channel_hr][i] + DmixTypeMat[type_id][3] * w_z* dm->ch_data[enc_channel_sr5][i];
  }
  dm->ch_data[enc_channel_tl] = dm->buffer[enc_channel_mixed_t_l];
  dm->ch_data[enc_channel_tr] = dm->buffer[enc_channel_mixed_t_r];

  dm->gaindown_map[CHANNEL_LAYOUT_D312][enc_channel_tl] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D312][enc_channel_tr] = 1;

  return 0;
}

static int downmix_h2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (dm->ch_data[enc_channel_hfl])
  return downmix_h4to2(dm, dmix_type, weight_type, w_x);
}

static int downmix_fh2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (!dm->ch_data[enc_channel_hl])
    downmix_h2(dm, dmix_type, weight_type, w_x);
  if(dm->ch_data[enc_channel_hl])
    return downmix_h2tofh2(dm, dmix_type, weight_type, w_x);
}

static int downmix_s7to5(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dm->buffer[enc_channel_mixed_s5_l][i] = DmixTypeMat[type_id][0] * dm->ch_data[enc_channel_sl7][i] + DmixTypeMat[type_id][1] * dm->ch_data[enc_channel_bl7][i];
    dm->buffer[enc_channel_mixed_s5_r][i] = DmixTypeMat[type_id][0] * dm->ch_data[enc_channel_sr7][i] + DmixTypeMat[type_id][1] * dm->ch_data[enc_channel_br7][i];
  }
  dm->ch_data[enc_channel_sl5] = dm->buffer[enc_channel_mixed_s5_l];
  dm->ch_data[enc_channel_sr5] = dm->buffer[enc_channel_mixed_s5_r];

  dm->gaindown_map[CHANNEL_LAYOUT_D510][enc_channel_sl5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D510][enc_channel_sr5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D512][enc_channel_sl5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D512][enc_channel_sr5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D514][enc_channel_sl5] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D514][enc_channel_sr5] = 1;

  return 0;
}

static int downmix_s5(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if(dm->ch_data[enc_channel_sl7])
    return downmix_s7to5(dm, dmix_type, weight_type, w_x);
}

static int downmix_s5to3(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dm->buffer[enc_channel_mixed_s3_l][i] = dm->ch_data[enc_channel_l5][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_sl5][i];
    dm->buffer[enc_channel_mixed_s3_r][i] = dm->ch_data[enc_channel_r5][i] + DmixTypeMat[type_id][2] * dm->ch_data[enc_channel_sr5][i];
  }
  dm->ch_data[enc_channel_l3] = dm->buffer[enc_channel_mixed_s3_l];
  dm->ch_data[enc_channel_r3] = dm->buffer[enc_channel_mixed_s3_r];

  dm->gaindown_map[CHANNEL_LAYOUT_D312][enc_channel_l3] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D312][enc_channel_r3] = 1;

  return 0;
}

static int downmix_s3to2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  int type_id = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dm->buffer[enc_channel_mixed_s2_l][i] = dm->ch_data[enc_channel_l3][i] + 0.707 * dm->ch_data[enc_channel_c][i];
    dm->buffer[enc_channel_mixed_s2_r][i] = dm->ch_data[enc_channel_r3][i] + 0.707 * dm->ch_data[enc_channel_c][i];
  }
  dm->ch_data[enc_channel_l2] = dm->buffer[enc_channel_mixed_s2_l];
  dm->ch_data[enc_channel_r2] = dm->buffer[enc_channel_mixed_s2_r];
  dm->gaindown_map[CHANNEL_LAYOUT_D200][enc_channel_l2] = 1;
  dm->gaindown_map[CHANNEL_LAYOUT_D200][enc_channel_r2] = 1;

  return 0;
}

static int downmix_s3(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if(!dm->ch_data[enc_channel_sl5])
    downmix_s5(dm, dmix_type, weight_type, w_x);
  if(dm->ch_data[enc_channel_sl5])
    downmix_s5to3(dm, dmix_type, weight_type, w_x);
}

static int downmix_s2(DownMixer *dm, int dmix_type, int weight_type, float *w_x)
{
  if (!dm->ch_data[enc_channel_l3])
    downmix_s3(dm, dmix_type, weight_type, w_x);
  if(dm->ch_data[enc_channel_l3])
    downmix_s3to2(dm, dmix_type, weight_type, w_x);
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

static creator_t g_downmix[] = {
  { CHANNEL_LAYOUT_D100, NULL },
  { CHANNEL_LAYOUT_D200, downmix_to200 },
  { CHANNEL_LAYOUT_D510, downmix_to510 },
  { CHANNEL_LAYOUT_D512, downmix_to512 },
  { CHANNEL_LAYOUT_D514, downmix_to514 },
  { CHANNEL_LAYOUT_D710, downmix_to710 },
  { CHANNEL_LAYOUT_D712, downmix_to712 },
  { CHANNEL_LAYOUT_D714, downmix_to714 },
  { CHANNEL_LAYOUT_D312, downmix_to312 },
  { -1 }
};

int downmix2(DownMixer *dm, unsigned char* inbuffer, int dmix_type, int weight_type)
{

  int ret = 0;
  float dspInBuf714[12][CHUNK_SIZE];
  float tmp[12][CHUNK_SIZE];
  float weight_state_value_x_curr = 0.0;
  uint8_t *playout;
  int channel;
  conv_downmixpcm(inbuffer, dspInBuf714, dm->channels);

  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char pre_ch = 0;



  ///////////////

  int last_layout = 0;
  int last_index = 0;
  int ts_index = 0;
  int ts_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int lay_out = dm->channel_layout_map[i];

    if (lay_out == CHANNEL_LAYOUT_DMAX)
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
    dm->ch_data[ch] = dspInBuf714[i];
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
      memcpy(tmp[ch], dm->ch_data[channel], sizeof(float) * FRAME_SIZE);
    }
    convert_preskip_pcm(dm->downmix_m[layout], tmp, enc_get_layout_channel_count(layout));
    //downmix_s
    for (int ch = pre_ch; ch < enc_get_layout_channel_count(layout); ++ch)
    {
      for (int j = 0; j < enc_get_layout_channel_count(layout); j++)
      {
        if (dm->channel_order[ch] == playout[j])
        {
          memcpy(&(dm->downmix_s[layout][(ch - pre_ch)*FRAME_SIZE]), tmp[j], sizeof(float) * FRAME_SIZE);
          break;
        }
      }
    }
    pre_ch = enc_get_layout_channel_count(layout);
  }



  dm->weight_state_value_x_prev = weight_state_value_x_curr;
  return 0;
}

#if 0
/*
channels: input channel
target_channel_type: demixed channel type, AB TPQ SUV...
dmix_type: downmix type from by audio scene classificaiton
weight_type: wight type from height mixing
input: input PCM

*/
int downmix(DownMixer *dm, unsigned char* inbuffer, int dmix_type, int weight_type)
{

 
  int ret = 0;
  float dspInBuf714[12][CHUNK_SIZE];
  float tmp[12][CHUNK_SIZE];
  float weight_state_value_x_curr = 0.0;
  conv_downmixpcm(inbuffer, dspInBuf714, dm->channels);

  unsigned char channel_map714[] = {1,2,6,8,10,8,10,12,6};
  unsigned char pre_ch = 0;
  unsigned char channel_map_s0[][MAX_CHANNELS] = { { 0,1 },{ 2,3,4,5 },{ 0,1 },{ 4,5,8,9 } }; //Transmission Order: 2ch / 3.1.2ch / 5.1.2ch / 7.1.4ch 
  unsigned char channel_map_s1[][MAX_CHANNELS] = { { 0,1 },{ 2,3,0,1 },{ 6,7 },{ 4,5,8,9 } }; //Transmission Order: 2ch / 5.1ch / 5.1.2ch / 7.1.4ch
  unsigned char channel_map_s2[][MAX_CHANNELS] = { { 0,1 },{ 2,3,0,1 },{ 4,5 },{ 8,9,10,11 } }; //Transmission Order: 2ch / 5.1ch / 7.1ch / 7.1.4ch
  unsigned char(*channel_map_s)[MAX_CHANNELS];
  if(dm->scalable_format == 0)
    channel_map_s = channel_map_s0;// TODO..........
  else if (dm->scalable_format == 1)
    channel_map_s = channel_map_s1;// TODO..........
  else if (dm->scalable_format == 2)
    channel_map_s = channel_map_s2;// TODO..........

  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int lay_out = dm->channel_layout_map[i];

    if (lay_out == CHANNEL_LAYOUT_DMAX)
      break;

    ((void(*)(float dspInBuf[][FRAME_SIZE], void* , int , int , float , float *))g_downmixm[lay_out].data)
      (dspInBuf714, tmp, dmix_type, weight_type, dm->weight_state_value_x_prev, &weight_state_value_x_curr);

    for (int j = 0; j < (channel_map714[lay_out] - pre_ch); j++)
    {
      for (int k = 0; k < FRAME_SIZE; k++)
      {
        float *temp1 = dm->downmix_s[lay_out];
        temp1[j*FRAME_SIZE + k] = tmp[(*(channel_map_s+i))[j]][k];
      }
    }
    pre_ch = channel_map714[lay_out];

    convert_preskip_pcm(dm->downmix_m[lay_out], tmp, channel_map714[lay_out]);
  }
  
  dm->weight_state_value_x_prev = weight_state_value_x_curr;
  return 0;
}
#endif

DownMixer * downmix_create(const unsigned char *channel_layout_map)
{
  DownMixer *dm = (DownMixer*)malloc(sizeof(DownMixer));
  memset(dm, 0x00, sizeof(DownMixer));

  memcpy(dm->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_DMAX);
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = dm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    dm->downmix_m[layout] = (float *)malloc((FRAME_SIZE + PRESKIP_SIZE) * MAX_CHANNELS * sizeof(float));
    memset(dm->downmix_m[layout], 0x00, (FRAME_SIZE + PRESKIP_SIZE) * MAX_CHANNELS * sizeof(float));
    dm->downmix_s[layout] = (float *)malloc(FRAME_SIZE * MAX_CHANNELS * sizeof(float));
    memset(dm->downmix_s[layout], 0x00, FRAME_SIZE * MAX_CHANNELS * sizeof(float));
  }

  for (int i = 0; i < enc_channel_mixed_cnt; i++)
  {
    dm->buffer[i] = (float *)malloc(FRAME_SIZE * sizeof(float));
    memset(dm->buffer[i], 0x00, FRAME_SIZE * sizeof(float));
  }


  int idx = 0, ret = 0;;
  int last_cl_layout = CHANNEL_LAYOUT_INVALID;
  uint8_t new_channels[256];

  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = dm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    ret = enc_get_new_channels(last_cl_layout, layout, new_channels);

    for (int i = idx, j = 0; j<ret; ++i, ++j) {
      dm->channel_order[i] = new_channels[j];
    }
    idx += ret;

    last_cl_layout = layout;
  }

  dm->channels = enc_get_layout_channel_count(last_cl_layout);
  dm->weight_state_value_x_prev = 0.0;
  return dm;
}

void downmix_clear(DownMixer *dm)
{
  dm->weight_state_value_x_prev = 0.0;
}

void downmix_destroy(DownMixer *dm)
{
  if (dm)
  {
    for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
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
