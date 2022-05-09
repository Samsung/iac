#ifdef WIN32
#else
#include <complex.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <fftw3.h>
#include "window.h"
#include "trans.h"

//int init = 0;
#ifdef DEBUG
void stft_dump_data(char* name,float* data, int num)
{
    FILE* fp = fopen(name,"w+");
    for(int i=0;i<num;i++){
        char temp[256];
        snprintf(temp,sizeof(temp),"%f\n",data[i]);
        fwrite(temp,strlen(temp),1,fp);

    }
    fclose(fp);
}
#endif 


struct stft {
	struct trans base;
	float *in, *tmp, *win;
#ifdef WIN32
  fftwf_complex *out;
#else
  complex float *out;
#endif
	fftwf_plan plan;
	int bins, samples;
};

void slide_stft(struct trans *trans, float *in, int N)
{
	struct stft *stft = (struct stft *)trans->data;
	
	memcpy(stft->in,in,sizeof(float)*stft->samples);
	
	for (int i = 0; i < stft->samples; i++)
		stft->tmp[i] = stft->win[i] * stft->in[i];
	
	#ifdef DEBUG
	stft_dump_data("framed_signals",stft->tmp,stft->samples);
	#endif 
	
	fftwf_execute(stft->plan);
}

void get_stft(struct trans *trans, float *out)
{
	struct stft *stft = (struct stft *)trans->data;
  for (int i = 0; i < stft->bins + 1; i++)
  {
#ifdef WIN32
    out[i] = sqrtf(stft->out[i][0] * stft->out[i][0] + stft->out[i][1] * stft->out[i][1]);
#else
    out[i] = cabsf(stft->out[i]);
#endif
  }

}

void free_stft(struct trans *trans)
{
	struct stft *stft = (struct stft *)trans->data;
	fftwf_destroy_plan(stft->plan);
	fftwf_free(stft->out);
	free(stft->tmp);
	free(stft->in);
	free(stft->win);
	free(stft);
	fftwf_cleanup();
}

struct trans *create_stft(int bins)
{
	struct stft *stft = (struct stft *)malloc(sizeof(struct stft));
	stft->base.free = free_stft;
	stft->base.slide = slide_stft;
	stft->base.get = get_stft;
	stft->base.data = (void *)stft;
	stft->bins = bins;
	int samples = bins * 2;
	stft->samples = samples;
	stft->in = (float *)malloc(sizeof(float) * samples);
	stft->tmp = (float *)malloc(sizeof(float) * samples);
	memset(stft->in, 0, sizeof(float) * samples);
#ifdef WIN32
	stft->out = (fftwf_complex *)malloc(sizeof(fftwf_complex) * (bins + 1));
#else
	stft->out = (complex float *)malloc(sizeof(complex float) * (bins + 1));
#endif
	stft->plan = fftwf_plan_dft_r2c_1d(samples, stft->tmp, stft->out, FFTW_ESTIMATE | FFTW_PRESERVE_INPUT);
	//stft->plan = fftwf_plan_dft_r2c_1d(samples, stft->tmp, stft->out, FFTW_ESTIMATE | FFTW_UNALIGNED);
	stft->win = (float *)malloc(sizeof(float) * samples);
	float sum = 0.0f;
	extern float hann_matrix[];
	for (int i = 0; i < samples; i++) {
		//stft->win[i] = gauss(i, samples|1, 0.2f);
		//stft->win[i] = hann(i,samples,0.0f);
		stft->win[i] = hann_matrix[i];
		//sum += stft->win[i];
	}
	//init = 0;
	//stft_dump_data("hann",stft->win,samples);
	//for (int i = 0; i < samples; i++)
	//	stft->win[i] /= sum;
	return (struct trans *)&(stft->base);
}

