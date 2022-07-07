#include <math.h>
#include "wave.h"
#include "common.h"
#include "asc_dn.h"
#include "asc_common_bs.h"
#include "ia_asc.h"
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

void save_result(char* filename,int* result, int frame_num,int ori_frame_num,int tile_factor)
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

void save_result2(FILE*  fp, int* result, int frame_num, int ori_frame_num, int tile_factor)
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

void save_result3(QueuePlus *pq, int* result, int frame_num, int ori_frame_num, int tile_factor)
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

IA_ASC * ia_asc_start(int layout, QueuePlus *pq)
{
  IA_ASC *ia_asc = (IA_ASC*)malloc(sizeof(IA_ASC));
  memset(ia_asc, 0x00, sizeof(IA_ASC));
  ia_asc->layout = layout;
  ia_asc->max_ents = 3000;// about 1min duration

  if (inference_asc_create(&(ia_asc->asc_estimator_feature)) < 0)
  {
    printf("inference_asc_create is failed \n");
    goto failure;
  }
  if (layout == ASC_CHANNEL_LAYOUT_100 || layout == ASC_CHANNEL_LAYOUT_200 || layout == ASC_CHANNEL_LAYOUT_312)
  {
    printf("ASC, by pass\n");
    return ia_asc;
  }
#ifdef USE_QUEUE_METHOD
  ia_asc->pq = pq;
#else
  char *filename = "audio_dmix.txt";
  ia_asc->fp = (FILE*)fopen(filename, "w+");
#endif
  uint32_t chunk_size = 960;

  uint32_t nch = 0;
  uint32_t unitNum = 0;

  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };

  nch = channel_map714[layout];
  unitNum = chunk_size * ia_asc->max_ents;
  ia_asc->data_fs = (float*)malloc(unitNum*DOWN_CHANNELS*sizeof(float));
  return ia_asc;
failure:
  inference_asc_destory(ia_asc->asc_estimator_feature);
  ia_asc->asc_estimator_feature = NULL;
  return ia_asc;
}

int frame_based_process(IA_ASC *asc)
{
  int ret = 0;
  void *asc_estimator_feature = asc->asc_estimator_feature;
  float * data_fs = asc->data_fs;

  uint32_t frame_size = 8640;
  uint32_t data_unit_size = 8640;
  uint32_t ds_factor = 4;
  uint32_t chunk_size = 960;
  uint32_t tile_factor = (frame_size / chunk_size) * ds_factor;

  float*  sample_e = NULL;
  int nsamples_e = 0;
  audio_resizing_714ch(data_fs, asc->ents*chunk_size, data_unit_size, ds_factor, &sample_e, &nsamples_e); //sample_e need to free

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
  if (!asc->start)
  {
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

    //asc->start = 1;
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
#ifdef USE_QUEUE_METHOD
  if(asc->pq)
    save_result3(asc->pq, dout_asc_result, frame_num, asc->ents, tile_factor);
#else
  if(asc->fp)
    save_result2(asc->fp, dout_asc_result, frame_num, asc->ents, tile_factor);
#endif
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


int ia_asc_process(IA_ASC *asc, int16_t *input, int size)
{
  int ret = 0;
  if (!asc->asc_estimator_feature)
    return 0;
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };

  int nch = channel_map714[asc->layout];

  int chunk_size = 960;
  int step = 0;
  int16_t down_sample[12 * 960];
  if (size > chunk_size) //down sample
  {
    step = size /(size - chunk_size);
    int index = 0;
    for (int i = 0; i < size; i++)
    {
      if (i%step == 0)
        continue;
      if (index >= chunk_size)
        break;
      for (int j = 0; j < nch; j++)
      {
        down_sample[index*nch + j] = input[i*nch + j];
      }
      index++;
    }
  }
  else
  {
    for (int i = 0; i < size; i++)
    {
      for (int j = 0; j < nch; j++)
      {
        down_sample[i*nch + j] = input[i*nch + j];
      }
    }
  }


  if (asc->layout == ASC_CHANNEL_LAYOUT_710 || asc->layout == ASC_CHANNEL_LAYOUT_712 || asc->layout == ASC_CHANNEL_LAYOUT_714)
  {

    int shift = asc->ents * chunk_size;
#if 0
    for (int i = 0; i<chunk_size; i++) {
      asc->data_fs[(i + shift)*DOWN_CHANNELS] = (int16_t)(input[2 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 1] = (int16_t)(input[0 + i*nch] + 0.7071 * input[8 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 2] = (int16_t)(input[1 + i*nch] + 0.7071 * input[9 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 3] = (int16_t)(input[4 + i*nch] + input[6 + i*nch] + 0.7071 * input[10 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 4] = (int16_t)(input[5 + i*nch] + input[7 + i*nch] + 0.7071 * input[11 + i*nch]);
    }
#else
    for (int i = 0; i<chunk_size; i++) {
      asc->data_fs[(i + shift)*DOWN_CHANNELS] = (int16_t)(down_sample[2 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 1] = (int16_t)(down_sample[0 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 2] = (int16_t)(down_sample[1 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 3] = (int16_t)(down_sample[4 + i*nch] + input[6 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 4] = (int16_t)(down_sample[5 + i*nch] + input[7 + i*nch]);
    }
#endif
    asc->ents++;
    if (asc->ents < asc->max_ents)
      return ret;
    else
    {
      frame_based_process(asc);
      asc->ents = 0;
    }

  }
  else if (asc->layout == ASC_CHANNEL_LAYOUT_514 || asc->layout == ASC_CHANNEL_LAYOUT_512 || asc->layout == ASC_CHANNEL_LAYOUT_510)
  {

    int shift = asc->ents * chunk_size;
    for (int i = 0; i<chunk_size; i++) {
      asc->data_fs[(i + shift)*DOWN_CHANNELS] = (int16_t)(down_sample[2 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 1] = (int16_t)(down_sample[0 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 2] = (int16_t)(down_sample[1 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 3] = (int16_t)(down_sample[4 + i*nch]);
      asc->data_fs[(i + shift)*DOWN_CHANNELS + 4] = (int16_t)(down_sample[5 + i*nch]);
    }
    asc->ents++;
    if (asc->ents < asc->max_ents)
      return ret;
    else
    {
      frame_based_process(asc);
      asc->ents = 0;
    }
  }

  return ret;
}

int ia_asc_stop(IA_ASC *asc)
{
  if(asc->ents > 0)
    frame_based_process(asc);
  if (asc && asc->asc_estimator_feature)
  {
    inference_asc_destory(asc->asc_estimator_feature);
  }
  if (asc)
  {
    if(asc->data_fs)
      free(asc->data_fs);
    if (asc->fp)
      fclose(asc->fp);
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
    
    inference_asc_destory(asc_estimator_feature);
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