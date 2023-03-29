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
 * @file utils.c
 * @brief height energy quantification
 * @version 0.1
 * @date Created 3/3/2023
**/

#include"utils.h"
#include"string.h"

void dumpChannel(char* buffer,char* name)
{
    
    char temp[20] = {0};
    snprintf(temp,20,"%s.txt",name);
    
    FILE* fp = fopen(temp,"w+");

    int16_t * p = (int16_t *)buffer;
    for(int i=0;i<CHUNK_LEN;i++)
    {
        snprintf(temp,20,"%d\n",*p++);
        fwrite(temp,strlen(temp),1,fp);
    }

    fclose(fp);


}


// void dump_dspInBuf(int channels, int chunkLen)
// {
//     FILE *fp = fopen("c_dspInBuf.txt","w+");

//     for(int i=0;i<channels;i++){
//         int16_t * p = dspInBuf[i];
//         char temp[20] = {0};
        
//         snprintf(temp,20,"\n channel %d\n",i);
//         printf("%s",temp);
//         fwrite(temp,strlen(temp),1,fp);

//         for(int j=0; j<chunkLen;j++){             
//              snprintf(temp,20,"%d\n",*p);
//              fwrite(temp,strlen(temp),1,fp);
//              p++;
//         }
//     }
        
//     if(fp){
//         fclose(fp);
//         fp = NULL;
//     }
// }

void dumpFrame(char *buffer)
{
    int16_t* p = (int16_t*)buffer;
    FILE *fp = fopen("frame.txt","w+");
    for(int i=0;i< CHUNK_LEN*MAX_CHANNELS;i++){
        char temp[20] = {0};
        snprintf(temp,20,"%d\n",*p++);
        fwrite(temp,strlen(temp),1,fp);
        

    }
    fclose(fp);



}



void dumpWave(Wav* wave)
{
    RIFF_t riff = wave->riff;
    FMT_t fmt   = wave->fmt;
    Data_t data = wave->data;
    
    printf("ChunkID \t%c%c%c%c\n", riff.ChunkID[0], riff.ChunkID[1], riff.ChunkID[2], riff.ChunkID[3]);
    printf("ChunkSize \t%d\n", riff.ChunkSize);
    printf("Format \t\t%c%c%c%c\n", riff.Format[0], riff.Format[1], riff.Format[2], riff.Format[3]);

    printf("\n");

    printf("Subchunk1ID \t%c%c%c%c\n", fmt.Subchunk1ID[0], fmt.Subchunk1ID[1], fmt.Subchunk1ID[2], fmt.Subchunk1ID[3]);
    printf("Subchunk1Size \t%d\n", fmt.Subchunk1Size);
    printf("AudioFormat \t%d\n", fmt.AudioFormat);
    printf("NumChannels \t%d\n", fmt.NumChannels);
    printf("SampleRate \t%d\n", fmt.SampleRate);
    printf("ByteRate \t%d\n", fmt.ByteRate);
    printf("BlockAlign \t%d\n", fmt.BlockAlign);
    printf("BitsPerSample \t%d\n", fmt.BitsPerSample);

    printf("\n");

    printf("blockID \t%c%c%c%c\n", data.Subchunk2ID[0], data.Subchunk2ID[1], data.Subchunk2ID[2], data.Subchunk2ID[3]);
    printf("blockSize \t%d\n", data.Subchunk2Size);

    printf("\n");

    printf("duration \t%d\n", data.Subchunk2Size / fmt.ByteRate);

}