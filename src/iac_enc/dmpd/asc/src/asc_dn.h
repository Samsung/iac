#ifndef ASC_DN_H
#define ASC_DN_H

float* asc_preprocess(float* input, int size);
float * asc_log_mstft_transform(void * asc_estimator_feature_, float* input, int channel, float * mscale_matrix, int nfft, int nhop);
void inference_asc_estimator(void * asc_estimator_feature_, float * input,float* fc_out, float* softmax_out, int* argmax_out);
void inference_asc_feature(void * asc_estimator_feature_, float* input,float* fout);
int inference_asc_create(void **inference);
int inference_asc_destory(void* inference);


#endif 