#ifndef DHE_H
#define DHE_H

#include<stdlib.h>

typedef struct dhe{
    int index;
    double dspOutBuf_rmse_hgt_short;
    double dspOutBuf_rmse_total_long;
    double dspOutBuf_rmse_srd_long;
    double dspOutBuf_rmse_total_short;
}DHE;

typedef struct threshold{
    double ThreT;
    double ThreS; 
    double ThreM;
}Threshold;


int downmix_714toM312_Det(double* dspInBuf[],int chunkLen,int TypeNum);
int downmix_714toM312_Det3_v2(double * dspInBuf[], double short_win, double long_win, DHE *dhe,Threshold threshold);
double rmse_ema_t2(double* input, int len, double prev, double win);


#endif 