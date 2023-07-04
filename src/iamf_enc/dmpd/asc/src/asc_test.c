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
 * @file asc_test.c
 * @brief audio scene classification
 * @version 0.1
 * @date Created 3/3/2023
**/

#include <math.h>
#include "wave.h"
#include "common.h"
#include "asc_dn.h"
#include "asc_common_bs.h"
#include "iamf_asc.h"
#include "asc_resampler.h"

#ifndef MAX_ASC_PROCESS_DURATIONS
#define MAX_ASC_PROCESS_DURATIONS  60 //unit:second
#endif

#ifndef ASC_DEFAULT_SAMPLE_RATE
#define ASC_DEFAULT_SAMPLE_RATE  48000
#endif

extern float  mel_matrix[8772];

void dump_data(char* name,float* data, int num)
{
    FILE* fp = fopen(name,"w+");
    for(int i=0;i<num;i++){
        char temp[256];
        snprintf(temp,sizeof(temp),"%f\n",data[i]);
        fwrite(temp,strlen(temp),1,fp);

    }
    fclose(fp);
}

static void save_result(char* filename,int* result, int frame_num,int ori_frame_num,int tile_factor)
{       
        
        char* sp = strrchr(filename,'.');
        char  ext[] = "_dmix.txt";
        char name[256]={0};
        strncpy(name,filename,sp-filename);
        strncat(name,ext,strlen(ext));
        
        FILE* fp = fopen(name,"w+");
        int count = 0;
        for(int i=0;i<frame_num;i++){
            for(int j=0;j<tile_factor;j++){
                if (count <= ori_frame_num){
                    char temp[256]={0};
                    snprintf(temp,sizeof(temp),"%d\n",result[i]);
                    fwrite(temp,strlen(temp),1,fp);
                }else{
                    fclose(fp);
                    return ;
                }
                count++;
            }
        }
        fclose(fp);
}

static void save_result2(FILE*  fp, int* result, int frame_num, int ori_frame_num, int tile_factor)
{

  int count = 0;
  for (int i = 0; i<frame_num; i++) {
    for (int j = 0; j<tile_factor; j++) {
      if (count < ori_frame_num) {
        char temp[256] = { 0 };
        snprintf(temp, sizeof(temp), "%d\n", result[i]);
        fwrite(temp, strlen(temp), 1, fp);
      }
      else {
        return;
      }
      count++;
    }
  }
}

static void save_result3(QueuePlus *pq, int* result, int frame_num, int ori_frame_num, int tile_factor)
{

  int count = 0;
  for (int i = 0; i<frame_num; i++) {
    for (int j = 0; j<tile_factor; j++) {
      if (count < ori_frame_num) {
        uint8_t dmix_index_t = result[i];
        QueuePush(pq, &dmix_index_t);
      }
      else {
        return;
      }
      count++;
    }
  }
}

static void save_result4(IAMF_ASC *asc, int* result, int frame_num, int tile_factor)
{
  uint32_t chunk_size = 960;
  uint32_t factor = chunk_size * tile_factor;
  for (uint32_t n = 0; n < asc->frames; n++)
  {
    uint32_t resample_size = asc->frame_size * asc->den / asc->num;
    uint32_t index = n * resample_size / factor;
    if (index >= frame_num)
      index = frame_num - 1;
    uint8_t dmix_index_t = result[index];
#ifdef USE_QUEUE_METHOD
    if(asc->pq)
      QueuePush(asc->pq, &dmix_index_t);
#else
    if (asc->fp) {
      char temp[256] = { 0 };
      snprintf(temp, sizeof(temp), "%d\n", dmix_index_t);
      fwrite(temp, strlen(temp), 1, asc->fp);
    }
#endif
  }
}

static const int gASCCLChCount[] = { 1, 2, 6, 8, 10, 8, 10, 12, 6, 2 };

static int asc_get_channels_count(ASC_CHANNEL_LAYOUT layout) {
  return gASCCLChCount[layout];
}

IAMF_ASC * iamf_asc_start(int layout, int frame_size, int sample_rate, QueuePlus *pq, char* out_file)
{
  IAMF_ASC *asc = (IAMF_ASC*)malloc(sizeof(IAMF_ASC));
  memset(asc, 0x00, sizeof(IAMF_ASC));
  asc->layout = layout;
  asc->shift = 0;
  asc->frame_size = frame_size;
  asc->den = asc->num = 1;
  asc->sample_rate = sample_rate;
  AscResamplerState *resampler = NULL;

  if (inference_asc_create(&(asc->asc_estimator_feature)) < 0)
  {
    printf("inference_asc_create is failed \n");
    goto failure;
  }
  if (layout == ASC_CHANNEL_LAYOUT_100 || layout == ASC_CHANNEL_LAYOUT_200 || layout == ASC_CHANNEL_LAYOUT_312)
  {
    printf("ASC, by pass\n");
    return asc;
  }
#ifdef USE_QUEUE_METHOD
  asc->pq = pq;
#else
#if 0
  char *filename = "audio_dmix.txt";
  asc->fp = (FILE*)fopen(filename, "w+");
#else
  if(out_file)
    asc->fp = (FILE*)fopen(out_file, "w+");
#endif
#endif

  //initial resampler
  int err = 0;
  int channels = asc_get_channels_count(layout);
  resampler = asc_resampler_init(channels, sample_rate, ASC_DEFAULT_SAMPLE_RATE, ASC_RESAMPLER_QUALITY_DEFAULT, &err);
  if (err != ASC_RESAMPLER_ERR_SUCCESS) goto failure;
  asc_resampler_skip_zeros(resampler);
  resampler->buffer = (int16_t*)malloc(frame_size * 4 * sizeof(int16_t)*channels);// max sample rate 192,000 Hz,4x48000
  if (!resampler->buffer) goto failure;
  uint32_t num = 1, den = 1;
  asc_resampler_get_ratio(resampler, &num, &den);
  asc->num = num;
  asc->den = den;
  asc->resampler = (void*)resampler;

  uint32_t unitNum = MAX_ASC_PROCESS_DURATIONS * ASC_DEFAULT_SAMPLE_RATE;
  asc->data_fs = (float*)malloc(unitNum*DOWN_CHANNELS*sizeof(float));
  if (asc->frame_size > 0)
    asc->max_frames = unitNum / (asc->frame_size * den / num);
  return asc;
failure:
  inference_asc_destroy(asc->asc_estimator_feature);
  asc->asc_estimator_feature = NULL;
  if (resampler) {
    if (resampler->buffer) free(resampler->buffer);
    asc_resampler_destroy(resampler);
  }
  return asc;
}

int frame_based_process(IAMF_ASC *asc)
{
  int ret = 0;
  int err;
  void *asc_estimator_feature = asc->asc_estimator_feature;
  float * data_fs = asc->data_fs;

  uint32_t frame_size = 8640;
  uint32_t data_unit_size = 8640;
  uint32_t ds_factor = 4;
  uint32_t chunk_size = 960;
  uint32_t tile_factor = (frame_size / chunk_size) * ds_factor;

  float*  sample_e = NULL;
  int nsamples_e = 0;
  audio_resizing_714ch(data_fs, asc->shift, data_unit_size, ds_factor, &sample_e, &nsamples_e); //sample_e need to free

  int frame_num = ((nsamples_e - 1.0) / data_unit_size) + 1;
  int* dout_asc_result = malloc(frame_num*sizeof(int));

  uint32_t f_size = 512;
  uint32_t kernel_s = 5;

  float *featureBuffer = malloc(f_size*(kernel_s + 1)*sizeof(float));
  if(!dout_asc_result||!featureBuffer)
  {
    ret = -1;
    goto FAILED;
  }
  memset(featureBuffer, 0, f_size*(kernel_s + 1)*sizeof(float));


  uint32_t fft_size_asc = 256;
  uint32_t hop_size_asc = 128;
  for (int i = 0; i < kernel_s; i++)
  {
    float * input_e = sample_e + i*DOWN_CHANNELS*data_unit_size;
    float * inp_prep_e = asc_preprocess(input_e, DOWN_CHANNELS*data_unit_size);
    float * inp_asc_e = asc_log_mstft_transform(asc_estimator_feature, inp_prep_e, DOWN_CHANNELS, mel_matrix, fft_size_asc, hop_size_asc);

    float fout[512] = { 0 };
    inference_asc_feature(asc_estimator_feature, inp_asc_e, fout);

    if (inp_asc_e)
    {
      free(inp_asc_e);
      inp_asc_e = NULL;
    }

    for (int j = 0; j < f_size; j++)
    {
      featureBuffer[f_size*(i + 1) + j] = fout[j];
    }

  }
#ifdef DEBUG
  dump_data("featureBuffer", featureBuffer, f_size*(kernel_s + 1));
#endif 

#ifdef DEBUG
  for (int i = kernel_s; i< (frame_num + kernel_s); i++) 
#else 
  for (int i = kernel_s; i< frame_num; i++) 
#endif 
{
    float * input_e = sample_e + i*DOWN_CHANNELS*data_unit_size;
    float * inp_prep_e = asc_preprocess(input_e, DOWN_CHANNELS*data_unit_size);
    float * inp_asc_e = asc_log_mstft_transform(asc_estimator_feature, inp_prep_e, DOWN_CHANNELS, mel_matrix, fft_size_asc, hop_size_asc);


    // float out_asc_e[2]={0};
    // float out_asc_e_softmax[2]={0};
    // int out_asc_result_e=0;

    // #ifdef DEBUG
    // extern float transpose_3_input[];
    // inp_asc_e = transpose_3_input;
    // #endif
    float fout[512] = { 0 };
    inference_asc_feature(asc_estimator_feature, inp_asc_e, fout);

    if (inp_asc_e)
    {
      free(inp_asc_e);
      inp_asc_e = NULL;
    }

    for (int k = 0; k<kernel_s; k++)
    {
      for (int j = 0; j<f_size; j++)
      {
        featureBuffer[f_size*k + j] = featureBuffer[f_size*(k + 1) + j];
      }
    }
    for (int j = 0; j<f_size; j++)
    {
      featureBuffer[f_size*kernel_s + j] = fout[j];
    }

#ifdef DEBUG
    dump_data("featureBuffer_frame0", featureBuffer, f_size*(kernel_s + 1));
#endif 

    float fout_asc[3] = { 0 };
    int fout_asc_result = 0;
    float fout_asc_softmax[3] = { 0 };

    inference_asc_estimator(asc_estimator_feature, featureBuffer, fout_asc, fout_asc_softmax, &fout_asc_result);

    // float thre_dialog = 0.6;
    // float thre_effect = 0.8;
    //dout_asc_result[i] = get_decision_part(out_asc_d_softmax[out_asc_result_d],out_asc_e_softmax[out_asc_result_e],out_asc_result_d,out_asc_result_e,thre_dialog,thre_effect);
    dout_asc_result[i - kernel_s] = fout_asc_result + 1;
#ifndef DISABLE_DEBUG_LOG
    printf("frame %d  result %d total %d\n", i, dout_asc_result[i - kernel_s], frame_num);
#endif
  }
  //save_result2(asc->fp, dout_asc_result, frame_num, asc->frames, tile_factor);
  save_result4(asc, dout_asc_result, frame_num, tile_factor);
FAILED:
  if (NULL != sample_e) {
    free(sample_e);
    sample_e = NULL;
  }
  if (NULL != dout_asc_result) {
    free(dout_asc_result);
    dout_asc_result = NULL;
  }

  if (NULL != featureBuffer) {
    free(featureBuffer);
    featureBuffer = NULL;
  }
  return ret;
}


int iamf_asc_process(IAMF_ASC *asc, int16_t *input, int size)
{
  int ret = 0;
  if (!asc->asc_estimator_feature)
    return 0;
  int nch = asc_get_channels_count(asc->layout);
  uint32_t resample_size = size;
  int16_t *pcm_data = input;

  if (asc->sample_rate != ASC_DEFAULT_SAMPLE_RATE) {
    resample_size = asc->frame_size * asc->den / asc->num;

    AscResamplerState * resampler = (AscResamplerState*)asc->resampler;
    if (resampler) {
      asc_resampler_process_interleaved_int(
        resampler, (const int16_t *)input, (uint32_t *)&size,
        (int16_t *)resampler->buffer, (uint32_t *)&resample_size);
      pcm_data = resampler->buffer;
    }
  }

  if (asc->layout == ASC_CHANNEL_LAYOUT_710 || asc->layout == ASC_CHANNEL_LAYOUT_712 || asc->layout == ASC_CHANNEL_LAYOUT_714)
  {

    int shift = asc->shift;
#if 0
    for (int i = 0; i<chunk_size; i++) {
      asc->data_fs[(i + shift)*DOWN_CHANNELS] = (int16_t)(input[2 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 1] = (int16_t)(input[0 + i*nch] + 0.7071 * input[8 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 2] = (int16_t)(input[1 + i*nch] + 0.7071 * input[9 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 3] = (int16_t)(input[4 + i*nch] + input[6 + i*nch] + 0.7071 * input[10 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 4] = (int16_t)(input[5 + i*nch] + input[7 + i*nch] + 0.7071 * input[11 + i*nch]);
    }
#else
    for (int i = 0; i<resample_size; i++) {
      asc->data_fs[(i + shift)*DOWN_CHANNELS] = (int16_t)(pcm_data[2 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 1] = (int16_t)(pcm_data[0 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 2] = (int16_t)(pcm_data[1 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 3] = (int16_t)(pcm_data[4 + i*nch] + pcm_data[6 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 4] = (int16_t)(pcm_data[5 + i*nch] + pcm_data[7 + i*nch]);
    }
#endif
    asc->frames++;
    asc->shift = shift + resample_size;

    if (asc->frames < asc->max_frames)
      return ret;
    else
    {
      frame_based_process(asc);
      asc->frames = 0;
      asc->shift = 0;
    }

  }
  else if (asc->layout == ASC_CHANNEL_LAYOUT_514 || asc->layout == ASC_CHANNEL_LAYOUT_512 || asc->layout == ASC_CHANNEL_LAYOUT_510)
  {

    int shift = asc->shift;
    for (int i = 0; i<resample_size; i++) {
      asc->data_fs[(i + shift)*DOWN_CHANNELS] = (int16_t)(pcm_data[2 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 1] = (int16_t)(pcm_data[0 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 2] = (int16_t)(pcm_data[1 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 3] = (int16_t)(pcm_data[4 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 4] = (int16_t)(pcm_data[5 + i*nch]);
    }
    asc->frames++;
    asc->shift = shift + resample_size;

    if (asc->frames < asc->max_frames)
      return ret;
    else
    {
      frame_based_process(asc);
      asc->frames = 0;
      asc->shift = 0;
    }
  }

  return ret;
}

int iamf_asc_stop(IAMF_ASC *asc)
{
  if(asc->frames > 0)
    frame_based_process(asc);
  if (asc && asc->asc_estimator_feature)
  {
    inference_asc_destroy(asc->asc_estimator_feature);
  }
  if (asc)
  {
    if(asc->data_fs)
      free(asc->data_fs);
    if (asc->fp)
      fclose(asc->fp);
    if (asc->resampler) {
      AscResamplerState *resampler = (AscResamplerState *)(asc->resampler);
      if (resampler->buffer)
        free(resampler->buffer);
      asc_resampler_destroy(resampler);
    }

    free(asc);
    return 0;
  }
  return -1;
}

#ifndef DISABLE_EXECUTION
int main(int argc, char* argv[])
{
#if 0
    if(argc != 2){
        printf("usage: tflite audio\n");
        return 0;
    }
#endif
    // setenv("MALLOC_TRACE","m.log","1");
    // mtrace();

    //char * filename = "replace_audio.wav";
    char * filename = argv[1];


    uint32_t frame_size     = 8640;
    uint32_t data_unit_size = 8640;
    uint32_t ds_factor      = 4;
    uint32_t ori_framesize = 960;
    uint32_t tile_factor = (frame_size /ori_framesize) * ds_factor;

    uint32_t fft_size_asc = 256;
    uint32_t hop_size_asc = 128;

    uint32_t f_size = 512;
    
    uint32_t kernel_s = 5;
    uint32_t kernel_f = 3;




    uint32_t nch = 0;
    uint32_t dataSize = 0;
    uint32_t frameNum = 0;


    FILE*  fp  = fopen(filename, "rb");;

    //fp = fopen("leaf_10s.wav", "rb");

    //fp = fopen("/home/denghh/AI_3D_Audio/0_encoding/2_HeightMixingParameter/W_param_extractor/input/Pixels_No.1.wav", "rb");
    //fp = fopen("Pixels_No.1.wav", "rb");

    assert(fp != NULL);
    
    Wav wav;
    
    fread(&wav,1,sizeof(Wav),fp);
    nch = wav.fmt.NumChannels;
    dataSize = wav.data.Subchunk2Size;
    frameNum = dataSize/(BAND_WIDTH*nch);

    int ori_frame_num = (int)ceil(frameNum/ori_framesize);


    float * data_fs = NULL;
    data_fs = (float*)malloc(frameNum*DOWN_CHANNELS*sizeof(float));

    printf("nch %d  dataSize %d frameNum %d\n",nch,dataSize,frameNum);

    int16_t input[MAX_CHANNELS]={0};
    //ch_composition
    for(int i= 0;i<frameNum;i++){
        fread(&input,1,sizeof(input),fp);
        data_fs[i*DOWN_CHANNELS]      = (int16_t)(input[2]);
        data_fs[i*DOWN_CHANNELS + 1]  = (int16_t)(input[0] + 0.7071 * input[8]);
        data_fs[i*DOWN_CHANNELS + 2]  = (int16_t)(input[1] + 0.7071 * input[9]);
        data_fs[i*DOWN_CHANNELS + 3]  = (int16_t)(input[4] + input[6] +  0.7071 * input[10]);
        data_fs[i*DOWN_CHANNELS + 4]  = (int16_t)(input[5] + input[7] +  0.7071 * input[11]);
    }
    #ifdef DEBUG    
    dump_data("data_fs.txt",data_fs,frameNum*DOWN_CHANNELS);
    #endif 

    float*  sample_e = NULL;
    int nsamples_e = 0;
    audio_resizing_714ch(data_fs, frameNum,  data_unit_size, ds_factor, &sample_e, &nsamples_e); //sample_e need to free

    #ifdef DEBUG    
    dump_data("sample_e.txt",sample_e,nsamples_e*DOWN_CHANNELS);
    #endif 


    #ifdef DEBUG
    int frame_num = 1;
    #else 
    int frame_num = ((nsamples_e -1.0)/data_unit_size) + 1;
    #endif 

   

    
    int* dout_asc_result  = malloc(frame_num*sizeof(int));

    float * featureBuffer = malloc(f_size*(kernel_s+1)*sizeof(float));
    memset(featureBuffer,0,f_size*(kernel_s+1)*sizeof(float));
    void *asc_estimator_feature = NULL;//create context
    if (inference_asc_create(&asc_estimator_feature) < 0)
      goto failure;

    for(int i=0;i<kernel_s;i++)
    {
        float * input_e = sample_e + i*DOWN_CHANNELS*data_unit_size;
        float * inp_prep_e = asc_preprocess(input_e,DOWN_CHANNELS*data_unit_size);
        float * inp_asc_e  = asc_log_mstft_transform(asc_estimator_feature,inp_prep_e,DOWN_CHANNELS,mel_matrix,fft_size_asc,hop_size_asc);

        float fout[512]={0};
        inference_asc_feature(asc_estimator_feature, inp_asc_e,fout);

        if (inp_asc_e)
        {
          free(inp_asc_e);
          inp_asc_e = NULL;
        }

        for(int j=0;j<f_size;j++)
        {
            featureBuffer[f_size*(i+1) + j] = fout[j];
        }

    }
    #ifdef DEBUG
    dump_data("featureBuffer",featureBuffer,f_size*(kernel_s+1));
    #endif 

    #ifdef DEBUG
    for(int i= kernel_s; i< (frame_num + kernel_s);i++)
    #else 
    for(int i= kernel_s; i< frame_num;i++)
    #endif 
    {
        float * input_e = sample_e + i*DOWN_CHANNELS*data_unit_size;
        float * inp_prep_e = asc_preprocess(input_e,DOWN_CHANNELS*data_unit_size);
        float * inp_asc_e  = asc_log_mstft_transform(asc_estimator_feature,inp_prep_e,DOWN_CHANNELS,mel_matrix,fft_size_asc,hop_size_asc);


        // float out_asc_e[2]={0};
        // float out_asc_e_softmax[2]={0};
        // int out_asc_result_e=0;

        // #ifdef DEBUG
        // extern float transpose_3_input[];
        // inp_asc_e = transpose_3_input;
        // #endif
        float fout[512]={0};
        inference_asc_feature(asc_estimator_feature,inp_asc_e,fout);

        if (inp_asc_e)
        {
          free(inp_asc_e);
          inp_asc_e = NULL;
        }

        for(int k=0;k<kernel_s;k++)
        {
            for(int j=0;j<f_size;j++)
            {
                featureBuffer[f_size*k + j] = featureBuffer[f_size*(k+1) + j];
            }
        }
        for(int j=0;j<f_size;j++)
        {
            featureBuffer[f_size*kernel_s + j] = fout[j];
        }

        #ifdef DEBUG
        dump_data("featureBuffer_frame0",featureBuffer,f_size*(kernel_s+1));
        #endif 

        float fout_asc[3] = {0};
        int fout_asc_result = 0;
        float fout_asc_softmax[3] = {0};

        inference_asc_estimator(asc_estimator_feature, featureBuffer,fout_asc,fout_asc_softmax,&fout_asc_result);

        // float thre_dialog = 0.6;
        // float thre_effect = 0.8;
        //dout_asc_result[i] = get_decision_part(out_asc_d_softmax[out_asc_result_d],out_asc_e_softmax[out_asc_result_e],out_asc_result_d,out_asc_result_e,thre_dialog,thre_effect);
        dout_asc_result[i-kernel_s] = fout_asc_result + 1;
        printf("frame %d  result %d total %d\n",i,dout_asc_result[i-kernel_s],frame_num);
    }


    save_result(filename,dout_asc_result,frame_num,ori_frame_num,tile_factor);



failure:
    
    inference_asc_destroy(asc_estimator_feature);
    //refine
    if(NULL != sample_e){
        free(sample_e);
        sample_e = NULL;
    }
    if(NULL != dout_asc_result){
        free(dout_asc_result);
        dout_asc_result = NULL;
    }

    if(NULL != featureBuffer){
        free(featureBuffer);
        featureBuffer = NULL;
    }
    if(NULL != data_fs){
        free(data_fs);
        data_fs = NULL;
    }

    fclose(fp);

    return 0;
}
#endif