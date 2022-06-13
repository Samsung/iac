#include "common.h"

extern int decimate_dialog(float* input, int num,float* output);
extern int decimate_effect(float* input, int num,float* output);


int kernel_s = 5;


void audio_resizing_1ch(float * data0, int frame_size, int data_unit_size, int ds_factor, float** output, int* size)
{
    //todo:scipy.signal.decimate downsample    
    int nsamples_ds = ceil((float)frame_size/(float)ds_factor);
    int moduler = nsamples_ds % data_unit_size;
    int padded_nsamples_ds = nsamples_ds;

    if(moduler != 0){
        int tail_add = data_unit_size -moduler;
        padded_nsamples_ds = nsamples_ds + tail_add;
    }
    float* data_ds = malloc(padded_nsamples_ds*sizeof(float));
    if(!data_ds)return;
    memset(data_ds,0,padded_nsamples_ds*sizeof(float));
    decimate_dialog(data0,frame_size,data_ds);

    for(int i=0;i<padded_nsamples_ds;i++)
    {
        if(i < nsamples_ds)
        {
            //data_ds[i] = data0[ds_factor*i];  
        }
        else
        {
            data_ds[i] = data_ds[i-1];
        }
    }
    *output = data_ds;
    *size = padded_nsamples_ds;
}


void audio_resizing_714ch(float* data0, int frame_size, int data_unit_size, int ds_factor, float** output, int* size)
{
    int nsamples_ds = ceil((float)frame_size/(float)ds_factor);
    int moduler = nsamples_ds % data_unit_size;
    int padded_nsamples_ds = nsamples_ds;

    if(moduler != 0){
        int tail_add = data_unit_size - moduler;
        padded_nsamples_ds = nsamples_ds + tail_add;
    }

    
    //float* data_ds = malloc(padded_nsamples_ds*DOWN_CHANNELS*sizeof(float));
    float *ret_data = malloc( (padded_nsamples_ds + data_unit_size*kernel_s)*DOWN_CHANNELS*sizeof(float)); 
    if(!ret_data)return;
    float *data_ds =  ret_data  + data_unit_size*kernel_s*DOWN_CHANNELS;
    //float* data_ds = malloc(padded_nsamples_ds*DOWN_CHANNELS*sizeof(float));
    memset(data_ds,0,padded_nsamples_ds*DOWN_CHANNELS*sizeof(float));
    decimate_effect(data0,frame_size*DOWN_CHANNELS,data_ds);

    for(int i=0;i<padded_nsamples_ds;i++)
    {
        if(i < nsamples_ds)
        {
            // data_ds[i*4]     = data0[ds_factor*i*4];
            // data_ds[i*4 + 1] = data0[ds_factor*i*4 + 1];
            // data_ds[i*4 + 2] = data0[ds_factor*i*4 + 2];
            // data_ds[i*4 + 3] = data0[ds_factor*i*4 + 3];
        }
        else
        {
            data_ds[DOWN_CHANNELS*i]     = data_ds[DOWN_CHANNELS*(i-1)];
            data_ds[DOWN_CHANNELS*i + 1] = data_ds[DOWN_CHANNELS*(i-1) + 1];
            data_ds[DOWN_CHANNELS*i + 2] = data_ds[DOWN_CHANNELS*(i-1) + 2];
            data_ds[DOWN_CHANNELS*i + 3] = data_ds[DOWN_CHANNELS*(i-1) + 3];
            data_ds[DOWN_CHANNELS*i + 4] = data_ds[DOWN_CHANNELS*(i-1) + 4];
        }
    }

    for(int i=0;i<data_unit_size*kernel_s;i++)
    {
        ret_data[DOWN_CHANNELS*i] =   data_ds[0];
        ret_data[DOWN_CHANNELS*i+1] = data_ds[1];
        ret_data[DOWN_CHANNELS*i+2] = data_ds[2];
        ret_data[DOWN_CHANNELS*i+3] = data_ds[3];
        ret_data[DOWN_CHANNELS*i+4] = data_ds[4];
    }
    
    //*output = data_ds;
    //*size = padded_nsamples_ds;
    *output = ret_data;
    //todo: the return should be padded_nsamples_ds
    *size = padded_nsamples_ds + data_unit_size*kernel_s;
}




int get_decision_part(float prob_d, float prob_e, int d, int e, float thd, float the)
{
    int dout_asc_result = 0;

    if((d == 1) && (prob_d> thd)){
        dout_asc_result = 2;
    }else if((e == 1) && (prob_e > the)){
        dout_asc_result = 3;
    }else{
        dout_asc_result = 1;
    }

    return dout_asc_result;
}