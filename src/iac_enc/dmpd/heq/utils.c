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