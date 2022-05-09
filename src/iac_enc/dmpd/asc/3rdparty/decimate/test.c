#include<stdio.h>
#include<stdlib.h>



#define INPUT_SIZE 200

extern int decimate_dialog(float* input, int num,float* output);
extern int decimate_effect(float* input, int num,float* output);

int main()
{
    float input[INPUT_SIZE]={0};
    float output[INPUT_SIZE/4]={0};

    for(int i=0; i<INPUT_SIZE;i++){
        input[i] = i;
    }
    decimate_dialog(input,INPUT_SIZE,output);

    for(int i=0;i<INPUT_SIZE/4;i++)
    {
         printf("%f\n",output[i]);
    }

    decimate_effect(input,INPUT_SIZE,output);
    for(int i=0;i<INPUT_SIZE/4;i++)
    {
        printf("%f\n",output[i]);
    }
	

    return 0;
}