#include "gaindown.h"
#include "fixedp11_5.h"
static float DmixTypeMat[][4] = {
  { 1.0f, 1.0f, 0.707f, 0.707f },     //type1
  { 0.707f, 0.707f, 0.707f, 0.707f },  //type2
  { 1.0f, 0.866f, 0.866f, 0.866f } };	// type3
void tpq_downgain(float *inbuf, void *outbuf, int nch, float sdn)
{
  int16_t *wbuf = (int16_t *)outbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      if (i == 2 || i == 3)
      {
        wbuf[i + j*nch] = (int16_t)(inbuf[i + j*nch] * sdn * 32767.0);
      }
      else
      {
        wbuf[i + j*nch] = (int16_t)(inbuf[i + j*nch] * 32767.0);
      }

    }
  }
}

void remix_AB2chtoAB2cht(float dspInBuf[][FRAME_SIZE], float tpqsch_oggInBuf[][FRAME_SIZE], void* dspOut, float sdn, int dmix_type, int weight_type)
{
  float(*dspOutBuf)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspOut;
  int Typeid = dmix_type - 1;
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    float MSL = DmixTypeMat[Typeid][0] * dspInBuf[gSL][i] + DmixTypeMat[Typeid][1] * dspInBuf[gBL][i];
    float MSR = DmixTypeMat[Typeid][0] * dspInBuf[gSR][i] + DmixTypeMat[Typeid][1] * dspInBuf[gBR][i];
    dspOutBuf[gML3][i] = dspInBuf[gL][i] + DmixTypeMat[Typeid][3] * MSL + 0.707*tpqsch_oggInBuf[0][i];
    dspOutBuf[gA][i] = dspOutBuf[gA][i] * sdn;
    dspOutBuf[gMR3][i] = dspInBuf[gR][i] + DmixTypeMat[Typeid][3] * MSR + 0.707*tpqsch_oggInBuf[0][i];
    dspOutBuf[gB][i] = dspOutBuf[gB][i] * sdn;
  }

}

void ab_downgain(int16_t* inbuffer, int inbuffer_ch, int16_t* inbuffer_tpq, int tpq_ch, void *outbuf1, void *outbuf2, float sdn, int dmix_type, int weight_type)
{
  float dspInBuf714[12][FRAME_SIZE];
  float dspInBuftpq[4][FRAME_SIZE];
  float dspOutBuf[12][FRAME_SIZE];
  int16_t *outbuf_ab_t = (int16_t*)outbuf1;
  int16_t *outbuf_ab = (int16_t*)outbuf2;
  for (int i = 0; i < inbuffer_ch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
      dspInBuf714[i][j] = (float)(inbuffer[i + j * inbuffer_ch]);
  }

  for (int i = 0; i < tpq_ch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
      dspInBuftpq[i][j] = (float)(inbuffer_tpq[i + j * tpq_ch]);
  }
  remix_AB2chtoAB2cht(dspInBuf714, dspInBuftpq, dspOutBuf,sdn, dmix_type, weight_type);
  if (outbuf_ab_t)
  {
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < FRAME_SIZE; j++)
      {
        outbuf_ab_t[i + j * 2] = dspOutBuf[i][j];
      }
    }
  }

  for (int i = 0; i < FRAME_SIZE; i++)
  {
    dspInBuftpq[0][i] = dspInBuf714[gC][i];
  }
  remix_AB2chtoAB2cht(dspInBuf714, dspInBuftpq, dspOutBuf, sdn, dmix_type, weight_type);

  if (outbuf_ab)
  {
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < FRAME_SIZE; j++)
      {
        outbuf_ab[i + j * 2] = dspOutBuf[i][j];
      }
    }
  }


}

/*
a
b
t
p
q
*/
void gaindown(float *downmix_s[CHANNEL_LAYOUT_GMAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, uint16_t *dmixgain)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };

  unsigned char pre_ch = 0;
  unsigned char base_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_GMAX; i++)
  {
    int lay_out = channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_GMAX)
      break;
    int channels = channel_map714[lay_out] - pre_ch;
    for (int j = 0; j < channels; j++)
    {
      if (gain_down_map[base_ch + j] == 0)
        continue;
      for (int k = 0; k < FRAME_SIZE; k++)
      {
        downmix_s[lay_out][j*FRAME_SIZE + k] = downmix_s[lay_out][j*FRAME_SIZE + k] * qf_to_float(dmixgain[lay_out], 8);
      }
    }
    base_ch += channels;
    pre_ch = channel_map714[lay_out];
  }
}

/*
abtpq...
*/
void gaindown2(float *downmix_s[CHANNEL_LAYOUT_GMAX],
  const unsigned char *channel_layout_map, const unsigned char *gain_down_map, uint16_t *dmixgain)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };

  unsigned char pre_ch = 0;
  unsigned char base_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_GMAX; i++)
  {
    int lay_out = channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_GMAX)
      break;
    int channels = channel_map714[lay_out] - pre_ch;
    for (int j = 0; j < channels; j++)
    {
      if (gain_down_map[base_ch + j] == 0)
        continue;
      for (int k = 0; k < FRAME_SIZE; k++)
      {
        downmix_s[lay_out][j + k*channels] = downmix_s[lay_out][j + k*channels] * qf_to_float(dmixgain[lay_out], 8);
      }
    }
    base_ch += channels;
    pre_ch = channel_map714[lay_out];
  }
}