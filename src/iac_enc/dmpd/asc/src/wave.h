#ifndef WAVE_H
#define WAVE_h
#ifdef __cplusplus
extern "C" {
#endif 

#include<stdlib.h>
#include<stdio.h>
#include<stdint.h>
#include<assert.h>

//#include"common.h"

#define FRAME_SIZE   720
#define BAND_WIDTH   2
#define MAX_CHANNELS 12

typedef struct WAV_RIFF {
    char ChunkID[4];   /* "RIFF" */
    uint32_t ChunkSize; /* 36 + Subchunk2Size */
    char Format[4];    /* "WAVE" */
} RIFF_t;

typedef struct WAV_FMT {
    char Subchunk1ID[4];   /* "fmt " */
    uint32_t Subchunk1Size; /* 16 for PCM */
    uint16_t AudioFormat;   /* PCM = 1*/
    uint16_t NumChannels;   /* Mono = 1, Stereo = 2, etc. */
    uint32_t SampleRate;    /* 8000, 44100, etc. */
    uint32_t ByteRate;  /* = SampleRate * NumChannels * BitsPerSample/8 */
    uint16_t BlockAlign;    /* = NumChannels * BitsPerSample/8 */
    uint16_t BitsPerSample; /* 8bits, 16bits, etc. */
} FMT_t;

typedef struct WAV_data {
    char Subchunk2ID[4];   /* "data" */
    uint32_t Subchunk2Size; /* data size */
    /* sub-chunk-data */
} Data_t;

typedef struct WAV_fotmat {
   RIFF_t riff;
   FMT_t fmt;
   Data_t data;
} Wav;



#ifdef __cplusplus
}
#endif 
#endif 