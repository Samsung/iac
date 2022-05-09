#include "scalable_factor.h"
#include "scalable_format.h"
#define _USE_MATH_DEFINES
#include "math.h"
#include "fixedp11_5.h"
#include <time.h>

float scalefactor_list_[SF_LEN] = { 1, 0.928577102, 0.857154205, 0.785731307,
0.714308409, 0.642885512, 0.571462614, 0.500039716,
0.428616819, 0.357193921, 0.285771023, 0.214348126,
0.142925228, 0.071502330, 0.000079432, 0 };
static float spl714avg_data[12] = { 0, };
static float spl512avg_data[8] = { 0, };
static float spl312avg_data[6] = { 0, };
static void conv_scalable_factorpcm(unsigned char *pcmbuf, void* dspbuf, int nch, int dtype, int shift)
{
  if (dtype == 0)
  {
    int16_t *buff = (int16_t*)pcmbuf;

    float(*outbuff)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspbuf;
    for (int i = 0; i < nch; i++)
    {
      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        outbuff[i + shift][j] = (float)(buff[i + j*nch]) / 32768.0f; /// why / 32768.0f??
      }
    }
  }
  else if (dtype == 1)
  {
    float *buff = (float*)pcmbuf;

    float(*outbuff)[FRAME_SIZE] = (float(*)[FRAME_SIZE])dspbuf;
    for (int i = 0; i < nch; i++)
    {
      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        outbuff[i + shift][j] = (buff[i + j*nch]);
      }
    }
  }
  else if (dtype == 2)
  {
    float *buff = (float*)pcmbuf;
    float *outbuff = dspbuf;
    memcpy(outbuff, buff, sizeof(float)*FRAME_SIZE*nch);
  }

}
float sum_(float *arr, int size)
{
  float ret = 0.0;
  for (int i = 0; i < size; i++)
  {
    ret += arr[i];
  }
  return ret;
}
void calc_rms(float mBuf[][FRAME_SIZE], int channels, float* sum_sig,float * rms_sig)
{
  float mBuf_sq[FRAME_SIZE];
  
  for (int i = 0; i < channels; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      mBuf_sq[j] = mBuf[i][j] * mBuf[i][j];
    }
    sum_sig[i] = sum_(mBuf_sq, FRAME_SIZE);
    rms_sig[i] = sqrt((sum_sig[i]) / FRAME_SIZE);
  }

}
void rms_test(InScalableBuffer inbuffer, ChannelLayerMdhr clayer, RmsStruct * rms)
{
  float mBuf[12][CHUNK_SIZE];
  float rBuf[12][CHUNK_SIZE];
  float diffmrBuff[12][CHUNK_SIZE];
  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m, inbuffer.dtype_m, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r, inbuffer.dtype_r, 0);

  for (int i = 0; i < inbuffer.channels_m; i++)
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      diffmrBuff[i][j] = mBuf[i][j] - rBuf[i][j];
    }
  calc_rms(mBuf, inbuffer.channels_m, rms->sum_sig, rms->rms_sig);
  calc_rms(diffmrBuff, inbuffer.channels_m, rms->sum_nse, rms->rms_nse);
}

float rms_(float *input, int size)
{
  float average = 0.0;
  for (int i = 0; i < size; i++)
  {
    average += input[i] * input[i];
  }
  average /= (float)(size);
  return (sqrt(average) + pow(10.0,-20));
}

float calc_snr(float *origsig, float * reconsig, int size)
{
  float sig_rms = rms_(origsig, size) + pow(10.0, -20);
  float diff[FRAME_SIZE];
  for (int i = 0; i < size; i++)
    diff[i] = reconsig[i] - origsig[i];

  float nse_rms = rms_(diff, size) + pow(10.0, -20);
  float snr_db = 20 * log10(nse_rms / sig_rms);
  return snr_db;
}

float calc_smr(float *origsig, float * mixedsig, int size)
{
  float sig_rms = rms_(origsig, size) + pow(10.0, -20);
  float mixed_rms = rms_(mixedsig, size) + pow(10.0, -20);
  float snr_db = 20 * log10(sig_rms / mixed_rms);
  return snr_db;
}

int get_min_index(float *in, int size)
{
  int  index = 0;
  float value = in[0];
  for (int i = 0; i < size; i++)
  {
    if (in[i] < value)
    {
      index = i;
      value = in[i];
    }
  }
  return index;
}
void calc_scalefactor1(float origsig[][FRAME_SIZE], float reconsig[][FRAME_SIZE], int channels, float thres, int* scalefactor_index, float * scalefactor_data)
{
  float scaled_sig[FRAME_SIZE];
  float diff_sig[FRAME_SIZE];
  float diff_list[SF_LEN];
  float origsig_sq[FRAME_SIZE];
  float reconsig_sq[FRAME_SIZE];
  for (int i = 0; i < channels; i++)
  {
    float snr_ = calc_snr(origsig[i], reconsig[i], FRAME_SIZE);
    if (calc_snr(origsig[i], reconsig[i], FRAME_SIZE) > thres)
    {
      for (int j = 0; j < SF_LEN; j++)
      {
        for (int k = 0; k < FRAME_SIZE; k++)
        {
          scaled_sig[k] = reconsig[i][k] * scalefactor_list_[j];
          diff_sig[k] = origsig[i][k] - scaled_sig[k];
          diff_sig[k] = diff_sig[k] * diff_sig[k];
        }
        diff_list[j] = sum_(diff_sig, FRAME_SIZE);
      }
      for (int k = 0; k < FRAME_SIZE; k++) 
      {
        origsig_sq[k] = origsig[i][k] * origsig[i][k];
        reconsig_sq[k] = reconsig[i][k] * reconsig[i][k];
      }
      scalefactor_index[i] = get_min_index(diff_list, SF_LEN);
      scalefactor_data[i] = sqrt((sum_(origsig_sq, FRAME_SIZE) + pow(10.0, -20)) / (sum_(reconsig_sq, FRAME_SIZE) + pow(10.0, -20)));
      //printf("scalefactor_index %d %f\n", scalefactor_index[i], scalefactor_data[i]);
      if (scalefactor_index[i] == 0xf)
      {
        if (sum_(origsig_sq, FRAME_SIZE) != 0)
        {
          scalefactor_index[i] = 0x0e;
        }
        else
          scalefactor_data[i] = 0;
      }
    }
    else
    {
      scalefactor_index[i] = 0;
      scalefactor_data[i] = 1.0;
    }
    if (scalefactor_data[i] > 1.0)
    {
      scalefactor_data[i] = 1.0;
    }
  }
}

void calc_scalefactor2(float origsig[FRAME_SIZE], float mixedsig[FRAME_SIZE], float reconsig[FRAME_SIZE], float thres, float *splavg, int *scalefactor_index, float * scalefactor_data)
{


  float N = 3.0;
  float scaled_sig[FRAME_SIZE];
  float diff_sig[FRAME_SIZE];
  float diff_list[SF_LEN];
  float origsig_sq[FRAME_SIZE];
  float reconsig_sq[FRAME_SIZE];
  float smr = calc_smr(origsig, mixedsig, FRAME_SIZE);
  float origsig_db = 20 * log10(rms_(origsig, FRAME_SIZE));
  *splavg = (2.0 / (N + 1.0))*origsig_db + (1.0 - 2.0 / (N + 1.0))*(*splavg);

  if (origsig_db < -80.0 && (*splavg) < -80.0)
  {
    *scalefactor_index = 0xf;
    *scalefactor_data = 0;
  }
  else if (smr < thres)
  {
    for (int j = 0; j < SF_LEN; j++)
    {
      for (int k = 0; k < FRAME_SIZE; k++)
      {
        scaled_sig[k] = reconsig[k] * scalefactor_list_[j];
        diff_sig[k] = origsig[k] - scaled_sig[k];
        diff_sig[k] = diff_sig[k]* diff_sig[k];
      }
      diff_list[j] = sum_(diff_sig, FRAME_SIZE);
    }

    for (int k = 0; k < FRAME_SIZE; k++)
    {
      origsig_sq[k] = origsig[k] * origsig[k];
      reconsig_sq[k] = reconsig[k] * reconsig[k];
    }
    *scalefactor_index = get_min_index(diff_list, SF_LEN);
    *scalefactor_data = sqrt((sum_(origsig_sq, FRAME_SIZE) + +pow(10.0, -20)) / (sum_(reconsig_sq, FRAME_SIZE) + pow(10.0, -20)));
    if (*scalefactor_index == 0xf)
    {
      if (sum_(origsig_sq, FRAME_SIZE) != 0)
      {
        *scalefactor_index = 0x0e;
      }
      else
        scalefactor_data = 0;
    }
  }
  else
  {
    *scalefactor_index = 0;
    *scalefactor_data = 1.0;
  }
  if (*scalefactor_data > 1.0)
  {
    *scalefactor_data = 1.0;
  }
  
}

void scalefactor1_test(InScalableBuffer inbuffer, ChannelLayerMdhr clayer, ScalerFactorStruct *sf)
{
  float mBuf[12][CHUNK_SIZE];
  float rBuf[12][CHUNK_SIZE];
  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m, inbuffer.dtype_m, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r, inbuffer.dtype_r, 0);
  calc_scalefactor1(mBuf, rBuf, inbuffer.channels_m,-9.0, sf->scalefactor_index,sf->scalefactor_data);
}

void scalefactor2_714test(InScalableBuffer inbuffer, ChannelLayerMdhr clayer, ScalerFactorStruct *sf)
{
  float mBuf[12][CHUNK_SIZE];
  float rBuf[12][CHUNK_SIZE];
  float abBuf[12][CHUNK_SIZE];
  float tpqBuf[12][CHUNK_SIZE];
  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m, inbuffer.dtype_m, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r, inbuffer.dtype_r, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_ab, abBuf, inbuffer.channels_ab, inbuffer.dtype_ab, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_tpq, tpqBuf, inbuffer.channels_tpq, inbuffer.dtype_tpq, 0);
  float thres = -6.0;
  for (int i = 0; i < 12; i++)
  {
    sf->scalefactor_index[i] = 0;
    sf->scalefactor_data[i] = 1;
  }
  calc_scalefactor2(mBuf[gBL], abBuf[0],rBuf[gBL], thres, (spl714avg_data+ gBL), (sf->scalefactor_index+gBL), (sf->scalefactor_data+gBL));
  calc_scalefactor2(mBuf[gBR], abBuf[1], rBuf[gBR], thres, (spl714avg_data + gBR), (sf->scalefactor_index + gBR), (sf->scalefactor_data + gBR));
  calc_scalefactor2(mBuf[gHBL], tpqBuf[2], rBuf[gHBL], thres, (spl714avg_data + gHBL), (sf->scalefactor_index + gHBL), (sf->scalefactor_data + gHBL));
  calc_scalefactor2(mBuf[gHBR], tpqBuf[3], rBuf[gHBR], thres, (spl714avg_data + gHBR), (sf->scalefactor_index + gHBR), (sf->scalefactor_data + gHBR));
}

void scalefactor2_512test(InScalableBuffer inbuffer, ChannelLayerMdhr clayer, ScalerFactorStruct *sf)
{
  float mBuf[12][CHUNK_SIZE];
  float rBuf[12][CHUNK_SIZE];
  float abBuf[12][CHUNK_SIZE];
  float tpqBuf[12][CHUNK_SIZE];
  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m, inbuffer.dtype_m, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r, inbuffer.dtype_r, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_ab, abBuf, inbuffer.channels_ab, inbuffer.dtype_ab, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_tpq, tpqBuf, inbuffer.channels_tpq, inbuffer.dtype_tpq, 0);
  float thres = -6.0;

  calc_scalefactor2(mBuf[gMSL5], abBuf[0], rBuf[gMSL5], thres, (spl512avg_data + gMSL5), (sf->scalefactor_index + gMSL5), (sf->scalefactor_data + gMSL5));
  calc_scalefactor2(mBuf[gMSR5], abBuf[1], rBuf[gMSR5], thres, (spl512avg_data + gMSR5), (sf->scalefactor_index + gMSR5), (sf->scalefactor_data + gMSR5));
  calc_scalefactor2(mBuf[gMHL5], tpqBuf[2], rBuf[gMHL5], thres, (spl512avg_data + gMHL5), (sf->scalefactor_index + gMHL5), (sf->scalefactor_data + gMHL5));
  calc_scalefactor2(mBuf[gMHR5], tpqBuf[3], rBuf[gMHR5], thres, (spl512avg_data + gMHR5), (sf->scalefactor_index + gMHR5), (sf->scalefactor_data + gMHR5));
}

void scalefactor2_312test(InScalableBuffer inbuffer, ChannelLayerMdhr clayer, ScalerFactorStruct *sf)
{
  float mBuf[12][CHUNK_SIZE];
  float rBuf[12][CHUNK_SIZE];
  float abBuf[12][CHUNK_SIZE];

  conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m, inbuffer.dtype_m, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r, inbuffer.dtype_r, 0);
  conv_scalable_factorpcm(inbuffer.inbuffer_ab, abBuf, inbuffer.channels_ab, inbuffer.dtype_ab, 0);
  float thres = -6.0;

  calc_scalefactor2(mBuf[gML3], abBuf[0], rBuf[gML3], thres, (spl312avg_data + gML3), (sf->scalefactor_index + gML3), (sf->scalefactor_data + gML3));
  calc_scalefactor2(mBuf[gMR3], abBuf[1], rBuf[gMR3], thres, (spl312avg_data + gMR3), (sf->scalefactor_index + gMR3), (sf->scalefactor_data + gMR3));
}

int scalablefactor_init()
{
  for (int i = 0; i < 12; i++)
  {
    spl714avg_data[i] = 0.0;
  }
  for (int i = 0; i < 8; i++)
  {
    spl512avg_data[i] = 0.0;
  }
  for (int i = 0; i < 6; i++)
  {
    spl312avg_data[i] = 0.0;
  }
  return 0;
}

ScalableFactor * scalablefactor_create(const unsigned char *channel_layout_map)
{
  ScalableFactor *sf = (ScalableFactor *)malloc(sizeof(ScalableFactor));
  memset(sf, 0x00, sizeof(ScalableFactor));
  memcpy(sf->channel_layout_map, channel_layout_map, CHANNEL_LAYER_MDHR_MAX);
  return sf;
}

void scalablefactor_destroy(ScalableFactor *sf)
{
  if (sf)
    free(sf);
}

void cal_scalablefactor2(ScalableFactor *sf, Mdhr *mdhr, InScalableBuffer inbuffer, ChannelLayerMdhr clayer)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  int channels = channel_map714[clayer];
  ScalerFactorStruct sfs;
  int chx[12] = { 0, };
  float mBuf[12][CHUNK_SIZE];
  float rBuf[12][CHUNK_SIZE];
  float sBuf[12][CHUNK_SIZE];
  if (sf->scalefactor_mode == 0)
  {
    RmsStruct rms;
    rms_test(inbuffer, clayer, &rms);
    for (int i = 0; i < channels; i++)
    {
      if (rms.sum_sig[i] == 0)
        chx[i] = 0xf;
    }
  }
  else if (sf->scalefactor_mode == 1 || sf->scalefactor_mode == 2)
  {
    if (sf->scalefactor_mode == 1)
    {
      for (int i = 0; i < 12; i++)
      {
        sfs.scalefactor_index[i] = 0;
        sfs.scalefactor_data[i] = 1;
      }
      scalefactor1_test(inbuffer, clayer, &sfs);

      uint8_t* cl_lay = enc_get_layout_channels(clayer);
      int cl = 0;
      int last_cl = 0;
      for (int i = 0; i < channels; i++)
      {
        cl = cl_lay[i];
        if (inbuffer.scalable_map[cl] == 1)
        {
          chx[i] = float_to_qf(sfs.scalefactor_data[i], 8);
        }
      }
    }
    else if (sf->scalefactor_mode == 2)
    {
      conv_scalable_factorpcm(inbuffer.inbuffer_m, mBuf, inbuffer.channels_m, inbuffer.dtype_m, 0);
      conv_scalable_factorpcm(inbuffer.inbuffer_r, rBuf, inbuffer.channels_r, inbuffer.dtype_r, 0);
      conv_scalable_factorpcm(inbuffer.inbuffer_s, sBuf, inbuffer.channels_s, inbuffer.dtype_s, 0);
      int cl_index = 0;
      float thres = -6.0;
      for (int i = 0; i < 12; i++)
      {
        sfs.scalefactor_index[i] = 0;
        sfs.scalefactor_data[i] = 1;
      }
      uint8_t* cl_lay =  enc_get_layout_channels(clayer);
      int cl = 0;
      int last_cl = 0;
      for (int i = 0; i < channels; i++)
      {
        cl = cl_lay[i];
        if (inbuffer.scalable_map[cl] == 1)
        {
          for (int j = last_cl; j < channels; j++)
          {
            if (inbuffer.gaindown_map[j] == 1)
            {
              calc_scalefactor2(mBuf[i], sBuf[j], rBuf[i], thres, (sf->spl_avg_data[clayer] + i), (sfs.scalefactor_index + i), (sfs.scalefactor_data + i));
              chx[i] = float_to_qf(sfs.scalefactor_data[i], 8);
              last_cl = j + 1;
              break;
            }
          }
        }
      }
    }
  }

  for (int i = 0; i < channels; i++)
  {
    mdhr->scalablefactor[clayer][i] = chx[i];
  }
#if 0
  if (clayer == CHANNEL_LAYER_MDHR_312)
  {
    mdhr->chsilence[CHANNEL_LAYER_MDHR_312] = 0;
    mdhr->chsilence[CHANNEL_LAYER_MDHR_312] = mdhr->chsilence[CHANNEL_LAYER_MDHR_312] | chx[gML3];
    mdhr->chsilence[CHANNEL_LAYER_MDHR_312] = mdhr->chsilence[CHANNEL_LAYER_MDHR_312] | (chx[gMR3] << 8);

  }
  else if (clayer == CHANNEL_LAYER_MDHR_512)
  {
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = 0;
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | chx[gMSL5];
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | (chx[gMSR5] << 8);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | (chx[gMHL5] << 16);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | (chx[gMHR5] << 24);
  }
  else if (clayer == CHANNEL_LAYER_MDHR_714)
  {
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = 0;
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | chx[gBL];
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | (chx[gBR] << 8);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | (chx[gHBL] << 16);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | (chx[gHBR] << 24);
  }
#endif
}
//calculate scaler factor separately for diffrente channle layer(7.1.4/5.1.2/3.1.2)
void cal_scalablefactor(Mdhr *mdhr, InScalableBuffer inbuffer, int scalefactor_mode, ChannelLayerMdhr clayer)
{
  int channels;
  int chx[12] = {0,};
  ScalerFactorStruct sf;
  if (clayer == CHANNEL_LAYER_MDHR_312)
    channels = 6;
  else if (clayer == CHANNEL_LAYER_MDHR_512)
    channels = 8;
  else if (clayer == CHANNEL_LAYER_MDHR_714)
    channels = 12;

  if (scalefactor_mode == 0)
  {
    RmsStruct rms;
    rms_test(inbuffer, clayer, &rms);
    for (int i = 0; i < channels; i++)
    {
      if (rms.sum_sig[i] == 0)
        chx[i] = 0xf;
    }
    //if(inbuffer. )
  }
  else if (scalefactor_mode == 1 || scalefactor_mode == 2)
  {
    if (scalefactor_mode == 1)
    {
      scalefactor1_test(inbuffer, clayer,&sf);
    }
    else if (scalefactor_mode == 2)
    {
      if (clayer == CHANNEL_LAYER_MDHR_312)
      {
        scalefactor2_312test(inbuffer, clayer, &sf);
      }
      else if (clayer == CHANNEL_LAYER_MDHR_512)
      {
        scalefactor2_512test(inbuffer, clayer, &sf);
      }
      else if (clayer == CHANNEL_LAYER_MDHR_714)
      {
        scalefactor2_714test(inbuffer, clayer, &sf);
      }
    }

    if (clayer == CHANNEL_LAYER_MDHR_312)
    {
      chx[gML3] = float_to_qf(sf.scalefactor_data[gML3], 8);
      chx[gMR3] = float_to_qf(sf.scalefactor_data[gMR3], 8);
    }
    else if (clayer == CHANNEL_LAYER_MDHR_512)
    {
      chx[gMSL5] = float_to_qf(sf.scalefactor_data[gMSL5], 8);
      chx[gMSR5] = float_to_qf(sf.scalefactor_data[gMSR5], 8);
      chx[gMHL5] = float_to_qf(sf.scalefactor_data[gMHL5], 8);
      chx[gMHR5] = float_to_qf(sf.scalefactor_data[gMHR5], 8);
    }
    else if (clayer == CHANNEL_LAYER_MDHR_714)
    {
      chx[gBL] = float_to_qf(sf.scalefactor_data[gBL], 8);
      chx[gBR] = float_to_qf(sf.scalefactor_data[gBR], 8);
      chx[gHBL] = float_to_qf(sf.scalefactor_data[gHBL], 8);
      chx[gHBR] = float_to_qf(sf.scalefactor_data[gHBR], 8);
    }
  }


  if (clayer == CHANNEL_LAYER_MDHR_312)
  {
    mdhr->chsilence[CHANNEL_LAYER_MDHR_312] = 0;
    mdhr->chsilence[CHANNEL_LAYER_MDHR_312] = mdhr->chsilence[CHANNEL_LAYER_MDHR_312] | chx[gML3];
    mdhr->chsilence[CHANNEL_LAYER_MDHR_312] = mdhr->chsilence[CHANNEL_LAYER_MDHR_312] | (chx[gMR3] << 8);

  }
  else if (clayer == CHANNEL_LAYER_MDHR_512)
  {
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = 0;
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | chx[gMSL5];
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | (chx[gMSR5] << 8);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | (chx[gMHL5] << 16);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_512] = mdhr->chsilence[CHANNEL_LAYER_MDHR_512] | (chx[gMHR5] << 24);
  }
  else if (clayer == CHANNEL_LAYER_MDHR_714)
  {
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = 0;
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | chx[gBL];
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | (chx[gBR] << 8);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | (chx[gHBL] << 16);
    mdhr->chsilence[CHANNEL_LAYER_MDHR_714] = mdhr->chsilence[CHANNEL_LAYER_MDHR_714] | (chx[gHBR] << 24);
  }
}
