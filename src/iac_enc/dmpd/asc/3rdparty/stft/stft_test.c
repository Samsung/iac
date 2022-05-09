#include<stdio.h>
#include<stdlib.h>
#include<complex.h>

#include "trans.h"

#define INPUT_SIZE 8640
#define BINS 128
#define PADDED_SIZE  8704

extern struct trans *create_stft(int bins);
extern void slide_stft(struct trans *trans, float *in, int N);
extern void get_stft(struct trans *trans, float *out);
extern float transpose_input[];
extern void dump_data(char* name,float* data, int num);

float input_data[PADDED_SIZE+BINS];
int main()
{
    
   for(int i=0;i<PADDED_SIZE+BINS;i++)
   {
	   if(i<INPUT_SIZE)
	   {
		   input_data[i]= transpose_input[i];
	   }else{
		   input_data[i]=0;
	   }
   }
	struct trans*  stft = create_stft(BINS);
	for(int i=0;i< (PADDED_SIZE/BINS);i++){
		float output[BINS+1]={0};
		slide_stft(stft, input_data+i*BINS, BINS);
		get_stft(stft,output);		
		dump_data("stft_rfft",output,BINS+1);		
	}

    return 0;
}