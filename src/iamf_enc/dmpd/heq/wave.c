/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file wave.c
 * @brief height energy quantification
 * @version 0.1
 * @date Created 3/3/2023
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>


#include"utils.h"
#include"common.h"
#include"DHE.h"
#include"wave.h"

#include "iamf_heq.h"

FILE *fp = NULL;
double * dspInBuf[MAX_CHANNELS];
int count  = 0;


int frame_rate = 48000;
double short_duration = 0.360;
double long_duration = 3.60;


void init_dspInBuf(int channels,int chunkLen)
{
    assert(channels <= MAX_CHANNELS);

    for(int i=0;i<channels;i++){

        if(NULL == dspInBuf[i]){
            dspInBuf[i] = (double*)malloc(chunkLen*sizeof(double));
        }
        memset(dspInBuf[i],0,chunkLen*sizeof(double));
    }
}

void destroy_dspInBuf()
{
    for(int i=0;i<MAX_CHANNELS;i++){
        if(NULL != dspInBuf[i]){
            free(dspInBuf[i]);
            dspInBuf[i] = NULL;
        }
    }
}

void assign_dspInBuf(char* buffer, int channels,int chunkLen)
{
    int16_t * p = (int16_t*)buffer;
    for(int i = 0; i < channels; i++){
        for(int j = 0; j<chunkLen;j++){
            dspInBuf[i][j] = (double)(p[i+j*channels]);
        }
    }
}

int readWave(char* filename,Wav* wav)
{
    fp = fopen(filename, "rb");
    if (!fp) {
        printf("can't open audio file %s \n",filename);
        return FAILED;
    }

    fread(wav, 1, sizeof(Wav), fp);
    return SUCCESS;
}

static int get_height_channels(HEQ_CHANNEL_LAYOUT lay_out)
{
  int ret;
  switch (lay_out)
  {
  case HEQ_CHANNEL_LAYOUT_100:
    ret = 0;
    break;
  case HEQ_CHANNEL_LAYOUT_200:
    ret = 0;
    break;
  case HEQ_CHANNEL_LAYOUT_510:
    ret = 0;
    break;
  case HEQ_CHANNEL_LAYOUT_512:
    ret = 2;
    break;
  case HEQ_CHANNEL_LAYOUT_514:
    ret = 4;
    break;
  case HEQ_CHANNEL_LAYOUT_710:
    ret = 0;
    break;
  case HEQ_CHANNEL_LAYOUT_712:
    ret = 2;
    break;
  case HEQ_CHANNEL_LAYOUT_714:
    ret = 4;
    break;
  case HEQ_CHANNEL_LAYOUT_312:
    ret = 2;
    break;
  default:
    printf("wrong inputing\n");
    break;
  }
  return ret;
}

IAMF_HEQ *iamf_heq_start(int layout,int frame_rate, QueuePlus *pq, char* out_file)
{
  IAMF_HEQ *heq = (IAMF_HEQ*)malloc(sizeof(IAMF_HEQ));
  memset(heq, 0x00, sizeof(IAMF_HEQ));
  heq->layout = layout;
  heq->dhe.dspOutBuf_rmse_hgt_short = 0.0;
  heq->dhe.dspOutBuf_rmse_srd_long = 0.0;
  heq->dhe.dspOutBuf_rmse_total_long = 0.0;
  heq->dhe.dspOutBuf_rmse_total_short = 0.0;

  heq->threshold.ThreT = 2.0;
  heq->threshold.ThreS = 1.0;
  heq->threshold.ThreM = -80.0;
  heq->fcnt = 0;
  heq->Wlevel = 1;
  heq->frame_rate = frame_rate;
  if (layout == HEQ_CHANNEL_LAYOUT_714 || layout == HEQ_CHANNEL_LAYOUT_712
    || layout == HEQ_CHANNEL_LAYOUT_514 || layout == HEQ_CHANNEL_LAYOUT_512)
  {
#ifdef USE_QUEUE_METHOD
    heq->pq = pq;
#else
#if 0
    char *filename = "audio_w.txt";
    heq->fp = (FILE*)fopen(filename, "w+");
#else
    if (out_file)
      asc->fp = (FILE*)fopen(out_file, "w+");
#endif
#endif
  }
  else
  {
    printf("HEQ, by pass\n");
  }
  return heq;
}

int iamf_heq_process(IAMF_HEQ *heq, int16_t *input, int size)
{
  int ret = 0;

  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  int den_factor = size;

  if (size > 0)
    den_factor = size;
  double short_win = (short_duration * heq->frame_rate) / den_factor;
  double long_win = (long_duration *  heq->frame_rate) / den_factor;
#ifdef USE_QUEUE_METHOD
  if (heq->pq == NULL)
    return 0;
#else
  if (heq->fp == NULL)
    return 0;
#endif
  double *dspInBuf_[MAX_CHANNELS];
  for (int i = 0; i < MAX_CHANNELS; i++) {
    dspInBuf_[i] = (double*)malloc(size * sizeof(double));
    memset(dspInBuf_[i], 0x00, size * sizeof(double));
  }
  double *hgtArray_ = (double*)malloc(4 * size * sizeof(double));
  memset(hgtArray_, 0x00, 4 * size * sizeof(double));
  double *totalArray_ = (double*)malloc(10 * size * sizeof(double));
  memset(totalArray_, 0x00, 10 * size * sizeof(double));
  double *srdArray_ = (double*)malloc(6 * size * sizeof(double));
  memset(srdArray_, 0x00, 6 * size * sizeof(double));
  int nch = channel_map714[heq->layout];
  int h_length = get_height_channels(heq->layout);


  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < size; j++)
    {
      dspInBuf_[i][j] = (double)(input[i + j*nch]);
    }
  }
  //createHgtArray
  for (int i = 0; i<h_length; i++)
  {
    for (int j = 0; j<size; j++) {
      hgtArray_[i*size + j] = dspInBuf_[i + nch - h_length][j];
    }
  }
  heq->dhe.dspOutBuf_rmse_hgt_short = rmse_ema_t2(hgtArray_, h_length*size, heq->dhe.dspOutBuf_rmse_hgt_short, short_win);
  //createTotalArray
  int loop = 0;
  for (int i = 0; i<nch; i++)
  {
    if (2 == i || 3 == i)
      continue;
    for (int j = 0; j<size; j++) {
      totalArray_[loop*size + j] = dspInBuf_[i][j];
    }
    loop++;
  }
  heq->dhe.dspOutBuf_rmse_total_long = rmse_ema_t2(totalArray_, (nch - 2)*size, heq->dhe.dspOutBuf_rmse_total_long, long_win);
  heq->dhe.dspOutBuf_rmse_total_short = rmse_ema_t2(totalArray_, (nch - 2)*size, heq->dhe.dspOutBuf_rmse_total_short, short_win);

  double dspOutBuf_rmse_total_short_in_dbunit = 20 * log(heq->dhe.dspOutBuf_rmse_total_short);

  //createSrdArray
  loop = 0;
  for (int i = 0; i<nch - h_length; i++)
  {
    if (2 == i || 3 == i)
      continue;
    for (int j = 0; j<size; j++) {
      srdArray_[loop*size + j] = dspInBuf_[i][j];
    }
    loop++;
  }
  heq->dhe.dspOutBuf_rmse_srd_long = rmse_ema_t2(srdArray_, (nch - 2 - h_length)*size, heq->dhe.dspOutBuf_rmse_srd_long, long_win);

  if ((heq->fcnt % (int)short_win) == 0) {
    if (dspOutBuf_rmse_total_short_in_dbunit < heq->threshold.ThreM) {
      //WLevel = WLevel;
    }
    else if (heq->dhe.dspOutBuf_rmse_hgt_short / heq->dhe.dspOutBuf_rmse_total_long > heq->threshold.ThreT) {
      heq->Wlevel = 0.0;
    }
    else if (heq->dhe.dspOutBuf_rmse_hgt_short / heq->dhe.dspOutBuf_rmse_srd_long > heq->threshold.ThreS) {
      heq->Wlevel = 0.0;
    }
    else {
      heq->Wlevel = 1.0;
    }
  }

  heq->fcnt += 1;
#ifdef USE_QUEUE_METHOD
  if(heq->pq)
  {
    int index = (int)heq->Wlevel;
    uint8_t w_index_t = index;
    QueuePush(heq->pq, &w_index_t);
  }
#else
  if(heq->fp)
  {
    char temp[20] = { 0 };
    int index = (int)heq->Wlevel;
    snprintf(temp, 20, "%d\r", index);
    fwrite(temp, strlen(temp), 1, heq->fp);
  }
#endif

  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (dspInBuf_[i]) free(dspInBuf_[i]);
  }
  if (hgtArray_) free(hgtArray_);
  if (totalArray_) free(totalArray_);
  if (srdArray_) free(srdArray_);
  return 0;
}

int iamf_heq_stop(IAMF_HEQ *heq)
{
  if(heq->fp)
    fclose(heq->fp);
  if (heq)
    free(heq);
  return 0;
}

#ifndef DISABLE_EXECUTION
int main(int argc, char* argv[])
 {
	if(argc != 2){
        printf("usage: w_extractor audio\n");
        return 0;
    }

	Wav wav     = { 0 };
    FMT_t fmt   = { 0 };
    Data_t data = { 0 };

	char * filename = argv[1];
    
    readWave(filename,&wav);

    fmt = wav.fmt;
    data = wav.data;

    dumpWave(&wav);

    int chunkSize = CHUNK_LEN*BAND_WIDTH*fmt.NumChannels;
    assert(chunkSize != 0);
    
    int chunkNum = data.Subchunk2Size/chunkSize;
    char* buffer =(char* )malloc(chunkSize*sizeof(char));

    double short_win = (short_duration * frame_rate) / CHUNK_LEN;
    double long_win = (long_duration * frame_rate) / CHUNK_LEN;

    

	char* sp = strrchr(filename,'.');
    char  ext[] = "_w.txt";
    char name[256]={0};
    strncpy(name,filename,sp-filename);
    strcat(name,ext);
	
    FILE* fpIndex = fopen(name,"w+");
	char temp[20]={0};
    
    DHE dhe = {0};
    
    
    dhe.dspOutBuf_rmse_hgt_short = 0.0;
    dhe.dspOutBuf_rmse_srd_long = 0.0;
    dhe.dspOutBuf_rmse_total_long = 0.0;
    dhe.dspOutBuf_rmse_total_short = 0.0;

    Threshold threshold={0};

    threshold.ThreT = 2.0;
    threshold.ThreS = 1.0;
    threshold.ThreM = -80.0;

    for(int i= 0;i<chunkNum;i++){
        init_dspInBuf(fmt.NumChannels,CHUNK_LEN);
        fread(buffer,chunkSize*sizeof(char),1,fp);        
        assign_dspInBuf(buffer,fmt.NumChannels,CHUNK_LEN);
 
        //int index = downmix_714toM312_Det(dspInBuf,CHUNK_LEN,3);
        int index = downmix_714toM312_Det3_v2(dspInBuf,short_win,long_win,&dhe,threshold);

        snprintf(temp,20,"%d\r",index);
        fwrite(temp,strlen(temp),1,fpIndex);
        printf("frame %d  result %d total %d\n",i,index,chunkNum);
    }
	if(data.Subchunk2Size % chunkSize)
	{
		fwrite(temp,strlen(temp),1,fpIndex);;
	}

    fclose(fpIndex);




    printf("\n");
            
    printf("pos %d\n",(int)ftell(fp));
    free(buffer);
    fclose(fp);

    destroy_dspInBuf();
        

        
 }
#endif