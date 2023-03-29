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
 * @file asc_dn.c
 * @brief audio scene classification
 * @version 0.1
 * @date Created 3/3/2023
**/

#define FRAME_SIZE  8640

#define _INT16      32767.0
#define _LOG_EPS    1e-6


#define INPUT_SIZE 68

#include <math.h>
#include "common.h"
#include "trans.h"


typedef struct {
  void *estimator;
  void *feature;
  struct trans*  stft;
}asc_estimator_feature;

extern int load_model(char* filename,float* input,float* fc_out, float* softmax_out,float* argmax_out);


float* asc_preprocess(float* input,int size)
{
    for(int i=0; i< size;i++)
    {
        int value = ceil(input[i]);
        input[i] = (float)(value) / _INT16;
    }
    return input;    
}

// float* tf_signal_linear_to_mel_weight_matrix()
// {

// }

float * tf_signal_stft(void * asc_estimator_feature_,float* x,int channel)
{
    #define STFT_INPUT_SIZE 8640
    #define BINS 128
    #define PADDED_SIZE  8704
    static float input_data[PADDED_SIZE+BINS];

    int window_num = PADDED_SIZE/BINS; 
    float * ret = malloc(window_num*(BINS+1)*channel*sizeof(float));
    
    asc_estimator_feature *p = (asc_estimator_feature*)(asc_estimator_feature_);
    for(int j=0;j<channel;j++){
        for(int i=0;i<PADDED_SIZE+BINS;i++){
            if(i<STFT_INPUT_SIZE)
            {
                input_data[i]= x[j*STFT_INPUT_SIZE + i];
            }else{
                input_data[i]=0;
            }
        }           
        for(int i=0;i< window_num;i++){
            struct trans*  stft = p->stft;
            float* output = ret +window_num*(BINS+1)*j +i*(BINS+1);
            slide_stft(stft, input_data+i*BINS, BINS);
            get_stft(stft,output);
        }

    }
    return ret;
}


float* tf_abs(float* input, int size)
{
    for(int i=0; i< size;i++)
    {
        input[i] = (input[i] > 0)?input[i]:-input[i];
    }
    return input;
}

float* t_to_f(void * asc_estimator_feature_, float* x, int channel,int nfft, int nhop)
{

    float * stft = NULL;

 
        
    float * trans = malloc(channel*STFT_INPUT_SIZE*sizeof(float));
    if(!trans)return NULL;
    for(int i=0;i<STFT_INPUT_SIZE;i++)
    {                  
        trans[i]                     = x[i*channel];
        trans[i + STFT_INPUT_SIZE]   = x[i*channel + 1];
        trans[i + 2*STFT_INPUT_SIZE] = x[i*channel + 2];
        trans[i + 3*STFT_INPUT_SIZE] = x[i*channel + 3];
        trans[i + 4*STFT_INPUT_SIZE] = x[i*channel + 4];
    }
    stft = tf_signal_stft(asc_estimator_feature_,trans,channel);
    #ifdef DEBUG
    dump_data("feature/stft_rfft",stft,68*129*channel);
    #endif 
	if (trans)
	{
    	free(trans);
		trans = NULL;
	}	
  

    //stft

    return stft;
    
}

int matrix_offset(int height,int width,int row, int col)
{
    int offset = row * width + col;

    return offset;
}

float* tf_matmul(float* input_,int channel,float * mscale_matrix)
{
    int in_h = 68;
    int in_w = 129;
    int mel_h = 129;
    int mel_w = 68;
    int out_w = mel_w;

    float* out = malloc(in_h*out_w*channel*sizeof(float));
    float * ret = out;
    for(int c=0;c < channel;c++){
        for(int x=0;x<in_h;x++){
            for(int y=0;y<out_w;y++){
                float sum = 0;
                for(int z=0;z<in_w;z++){
                    int offset_input = matrix_offset(in_h,in_w,x,z);
                    int offset_mel = matrix_offset(mel_h,mel_w,z,y);
                    sum += input_[offset_input] * mscale_matrix[offset_mel];
                }
                int offset_out = matrix_offset(mel_h,mel_w,x,y);
                out[offset_out] = sum;
            }
        }
        //next channel
        input_ += in_h*in_w;
        out    += in_h*out_w;

    }

    return ret;
    
}

float* _tf_log10(float * x, int size, float eps)
{
    for(int i=0;i<size;i++)
    {
        x[i] = log(x[i] + eps)/log(10);
    }
    return x;
}


float* tf_transpose(float* x,int channel)
{
    if(channel == 1){
        return x;
    }
    
    float * ret = malloc(INPUT_SIZE*INPUT_SIZE*channel*sizeof(float));
    if(!ret)return NULL;
    for(int i=0;i<INPUT_SIZE*INPUT_SIZE;i++)
    {                  
          ret[i*channel]=  x[i];
          ret[i*channel + 1]=  x[i+ INPUT_SIZE*INPUT_SIZE];
          ret[i*channel + 2]=  x[i+2*INPUT_SIZE*INPUT_SIZE];
          ret[i*channel + 3]=  x[i+3*INPUT_SIZE*INPUT_SIZE];
          ret[i*channel + 4]=  x[i+4*INPUT_SIZE*INPUT_SIZE];
        
    }    
    return ret;
}

float * asc_log_mstft_transform(void * asc_estimator_feature_, float* input,int channel,float * mscale_matrix,int nfft,int nhop)
{
    float* stft_abs = t_to_f(asc_estimator_feature_,input,channel,nfft,nhop);
    //float* stft_abs = tf_abs(stft,68*129*channel);
    int mstft_size = INPUT_SIZE*INPUT_SIZE*channel;
    
    // #ifdef DEBUG
    // if(channel == 1){
    //      extern  float stft_abs_input[8772];
    //      stft_abs = stft_abs_input;        
    // }else{
    //     extern  float effect_stft_abs_input[];
    //     stft_abs = effect_stft_abs_input;
    // }
    // #endif 
    
    float* mstft = tf_matmul(stft_abs,channel,mscale_matrix);
    
    if (stft_abs)
      free(stft_abs);
    #ifdef DEBUG
    extern void dump_data(char* name,float* data, int num);
        dump_data("feature/MatMul_1",mstft,68*68*channel);
    #endif 
    
    float* log_melgrams = _tf_log10(mstft,mstft_size,_LOG_EPS);

    #ifdef DEBUG
    extern void dump_data(char* name,float* data, int num);
        dump_data("feature/truediv_3",log_melgrams,INPUT_SIZE*INPUT_SIZE*channel);
    #endif 



    float *log_melgrams2 = tf_transpose(log_melgrams,channel);
    
    if (log_melgrams)
    {
      free(log_melgrams);
      log_melgrams = NULL;
    }

    return log_melgrams2;
}


int inference_asc_create(void ** inference)
{
  extern int load_asc_estimator_model(void ** tflite);
  extern int load_asc_feature_model(void ** tflite);
  asc_estimator_feature *asc_estimator_feature_ = (asc_estimator_feature*)malloc(sizeof(asc_estimator_feature));
  if (asc_estimator_feature_)
    *inference = asc_estimator_feature_;
  else
    return -1;
  memset(asc_estimator_feature_, 0x00, sizeof(asc_estimator_feature_));
  asc_estimator_feature_->stft = create_stft(BINS);
  int ret1 = load_asc_estimator_model(&asc_estimator_feature_->estimator);
  int ret2 = load_asc_feature_model(&asc_estimator_feature_->feature);
  if (ret1 < 0 || ret2 < 0)
  {
    return -1;
  }
  return 0;
}

int inference_asc_destroy(void* asc_estimator_feature_)
{
  extern int unload_asc_estimator_model(void *estimator_tflite_);
  extern int unload_asc_feature_model(void *feature_tflite_);
  asc_estimator_feature *p = (asc_estimator_feature*)(asc_estimator_feature_);
  if (p)
  {
    free_stft(p->stft);
    if (p->estimator)
    {
      unload_asc_estimator_model(p->estimator);
      p->estimator = NULL;
    }

    if (p->feature)
    {
      unload_asc_feature_model(p->feature);
      p->feature = NULL;
    }
    free(p);
  }
  return 0;
}

void inference_asc_estimator(void * asc_estimator_feature_, float * input,float* fc_out, float* softmax_out, int* argmax_out)
{
    asc_estimator_feature *p = (asc_estimator_feature*)asc_estimator_feature_;
    //extern void load_asc_estimator_model();
    extern void asc_estimator_forward(void * estimator_tflite_, float* input,float* fc_out, float* softmax_out,int* argmax_out);
    //static int estimator_model_init = 0;
    //if(0 == estimator_model_init){
    //    load_asc_estimator_model();
    //    estimator_model_init = 1;
    //}
    asc_estimator_forward(p->estimator,input,fc_out,softmax_out,argmax_out);
    //load_model("model1.tflite",input,fc_out,softmax_out,argmax_out);
}

void inference_asc_feature(void * asc_estimator_feature_, float* input,float* fout)
{
    asc_estimator_feature *p = (asc_estimator_feature*)asc_estimator_feature_;
    //extern void load_asc_feature_model();
    extern void asc_feature_model_forward(void *feature_tflite_, float* input,float* fout);
    
    //static int feature_model_init = 0;
    //if(0 == feature_model_init){
    //    load_asc_feature_model();
    //    feature_model_init = 1;
    //}
    asc_feature_model_forward(p->feature, input,fout);

    //load_model("model2.tflite",input[0],fc_out,softmax_out,argmax_out);
}


