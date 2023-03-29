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
 * @file DHE.c
 * @brief height energy quantification
 * @version 0.1
 * @date Created 3/3/2023
**/

#include<math.h>
#include<string.h>

#include"DHE.h"
#include"common.h"




double TypePara[MAX_CHANNELS] = {1, 1, 0.707, 0.707, 0.707, 0.707, 0.707, 0.707, 1, 0.866, 0.866, 0.866};
int g_L=0, g_ML3=0, g_ML5=0, g_A = 0;
int g_R=1, g_MR3=1, g_MR5=1, g_B = 1;
int g_C=2, g_MC=2, g_T = 2;
int g_LFE=3, g_MLFE=3, g_P = 3;
int g_SL=4, g_MHL3=4, g_MSL5=4, g_Q1 = 4;
int g_SR=5, g_MHR3=5, g_MSR5=5, g_Q2 = 5;
int g_BL=6, g_MHL5=6, g_S1 = 6;
int g_BR=7, g_MHR5=7, g_S2 = 7;
int g_HL=8, g_U1 = 8;
int g_HR=9, g_U2 = 9;
int g_HBL=10, g_V1 = 10;
int g_HBR=11, g_V2 = 11;

double lin2db_my(double value)
{
    if (value <= 1){
        value = 1;
    }

    return 10 * log10(value);
}

double AbsSum(double* channel)
{
    double sum = 0;
    for(int i=0;i<CHUNK_LEN;i++)
    {
        sum += abs(channel[i]);
    }

    return sum;
}

int downmix_714toM312_Det(double* dspInBuf[],int chunkLen,int TypeNum)
{
    static int fcnt = 0;
    static double Wlevel = 1;
    double dspOutBuf[6][CHUNK_LEN]={0};
    double mixedHL[CHUNK_LEN]={0};
    double mixedHR[CHUNK_LEN]={0};

    double ThreA = 27;
    double ThreR = 12;
    double ThreC = 12;

    memset(dspOutBuf,0,sizeof(dspOutBuf));
    memset(mixedHL,0,sizeof(mixedHL));
    memset(mixedHR,0,sizeof(mixedHR));




    for(int i=0;i<CHUNK_LEN;i++){
        dspOutBuf[g_MSL5][i] = TypePara[(TypeNum-1)*4] * dspInBuf[g_SL][i] + TypePara[(TypeNum-1)*4+1] * dspInBuf[g_BL][i];
        dspOutBuf[g_MSR5][i] = TypePara[(TypeNum-1)*4] * dspInBuf[g_SR][i] + TypePara[(TypeNum-1)*4+1] * dspInBuf[g_BR][i];
        dspOutBuf[g_ML3][i] = dspInBuf[g_L][i] + TypePara[(TypeNum-1)*4+2] * dspOutBuf[g_MSL5][i];
        dspOutBuf[g_MR3][i] = dspInBuf[g_R][i] + TypePara[(TypeNum-1)*4+2] * dspOutBuf[g_MSR5][i];
        dspOutBuf[g_MC][i] = dspInBuf[g_C][i];
        dspOutBuf[g_MLFE][i] = dspInBuf[g_LFE][i];

        mixedHL[i] = dspInBuf[g_HL][i] + TypePara[(TypeNum-1)*4+3] * dspInBuf[g_HBL][i];
        mixedHR[i] = dspInBuf[g_HR][i] + TypePara[(TypeNum-1)*4+3] * dspInBuf[g_HBR][i];
    }

    if((fcnt % 5) == 0){
        if( (lin2db_my( AbsSum(mixedHL)/CHUNK_LEN ) > ThreA  || lin2db_my(AbsSum(mixedHR)/CHUNK_LEN) >ThreA ) && (Wlevel > 0)){
            Wlevel = Wlevel - 0.1;
        }else if ( ( lin2db_my( (AbsSum(dspOutBuf[g_ML3]) + AbsSum(dspOutBuf[g_MR3]))/CHUNK_LEN ) + ThreR <  lin2db_my((AbsSum(mixedHL) + AbsSum(mixedHR))/CHUNK_LEN) ) && Wlevel > 0){
            Wlevel = Wlevel - 0.1;
        }else if ( (lin2db_my( AbsSum(mixedHL)/CHUNK_LEN) < ThreA && lin2db_my(AbsSum(mixedHR)/CHUNK_LEN) < ThreA) &&  (Wlevel < 1)){
            Wlevel = Wlevel + 0.1;
        }else if ( (lin2db_my(AbsSum(mixedHL)/CHUNK_LEN) < ThreC && lin2db_my(AbsSum(mixedHR)/CHUNK_LEN) < ThreC )  && (Wlevel >=1) && (Wlevel < 2)){
            Wlevel = Wlevel + 0.1;
        }else if ( (lin2db_my(AbsSum(mixedHL)/CHUNK_LEN) > ThreC || lin2db_my(AbsSum(mixedHR)/CHUNK_LEN)  > ThreC) && (Wlevel>1) && (Wlevel <=2)){
            Wlevel = Wlevel - 0.1;
        }
    }


    if(Wlevel <1){
        for(int i=0;i<CHUNK_LEN;i++){
            dspOutBuf[g_MHL3][i] = -16384;
            dspOutBuf[g_MHR3][i] = -16384;}
        
    }else if(Wlevel ==1){
        for(int i=0;i<CHUNK_LEN;i++){
            dspOutBuf[g_MHL3][i] = 0;
            dspOutBuf[g_MHR3][i] = 0;
        }
    }else{
        for(int i=0;i<CHUNK_LEN;i++){
            dspOutBuf[g_MHL3][i] = 16384;
            dspOutBuf[g_MHR3][i] = 16384;
        }
    }

    fcnt+=1;

    int WIndex = (int) (Wlevel * 10);

    return WIndex;
}

double rmse(double * input,int len)
{   
    double sum  = 0.0;
    double mean = 0.0;
    double rms  = 0.0;

    for(int i=0;i<len;i++){
        sum += input[i]*input[i];
    }

    mean = sum / len;

    rms = sqrt(mean) + 10e-20;

    return rms;
}

double hgtArray[4*CHUNK_LEN]={0};
void createHgtArray(double * dspInBuf[])
{
    for(int i=0;i<4;i++)
    {
        for(int j=0;j<CHUNK_LEN;j++){
            hgtArray[i*CHUNK_LEN + j] = dspInBuf[i+8][j];
        }
    }
}

double totalArray[10*CHUNK_LEN]={0};
void createTotalArray(double * dspInBuf[])
{
    int i=0;
    double * cursor = totalArray;

    while(i<MAX_CHANNELS){
        if(2 == i || 3 == i){
            i++;
            continue;
        }

        for(int j=0;j<CHUNK_LEN;j++){
            *cursor++ = dspInBuf[i][j];
        }
        i++;
    }
}


double srdArray[6*CHUNK_LEN]={0};
void createSrdArray(double * dspInBuf[])
{
    int i=0;
    double * cursor = srdArray;

    while(i<MAX_CHANNELS){
        if( ( i >=0 && i < 2) || ( i >=4 && i<8 )){
            for(int j=0;j<CHUNK_LEN;j++){
                *cursor++ = dspInBuf[i][j];
            }
        }
        i++;
    }

}

double rmse_ema_t2(double* input, int len,double prev, double win)
{
    double smoothing = 2.0;
    
    double curr = (1-smoothing/(1.0+win))*prev + (smoothing/(1.0+win))*rmse(input,len);
    return curr;
}


int downmix_714toM312_Det3_v2(double * dspInBuf[], double short_win, double long_win, DHE *dhe,Threshold threshold)
{

    static int fcnt = 0;
    static double Wlevel = 1;

    //hgt 
    createHgtArray(dspInBuf);
    dhe->dspOutBuf_rmse_hgt_short = rmse_ema_t2(hgtArray,sizeof(hgtArray)/sizeof(double), dhe->dspOutBuf_rmse_hgt_short, short_win);

    createTotalArray(dspInBuf);
    dhe->dspOutBuf_rmse_total_long = rmse_ema_t2(totalArray,sizeof(totalArray)/sizeof(double), dhe->dspOutBuf_rmse_total_long, long_win);
    dhe->dspOutBuf_rmse_total_short = rmse_ema_t2(totalArray,sizeof(totalArray)/sizeof(double), dhe->dspOutBuf_rmse_total_short, short_win);

    double dspOutBuf_rmse_total_short_in_dbunit = 20 * log(dhe->dspOutBuf_rmse_total_short);

    createSrdArray(dspInBuf);
    dhe->dspOutBuf_rmse_srd_long = rmse_ema_t2(srdArray,sizeof(srdArray)/sizeof(double),dhe->dspOutBuf_rmse_srd_long, long_win);

    if ((fcnt % (int)short_win) == 0){
        if (dspOutBuf_rmse_total_short_in_dbunit < threshold.ThreM){
            //WLevel = WLevel;
        }else if (dhe->dspOutBuf_rmse_hgt_short/dhe->dspOutBuf_rmse_total_long > threshold.ThreT){
            Wlevel = 0.0;
        }else if (dhe->dspOutBuf_rmse_hgt_short/dhe->dspOutBuf_rmse_srd_long > threshold.ThreS){
            Wlevel = 0.0;
        }else {
            Wlevel = 1.0;
        }
    }

    fcnt+=1;

    return (int)Wlevel;
}   