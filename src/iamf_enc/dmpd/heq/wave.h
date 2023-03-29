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
 * @file wav.h
 * @brief height energy quantification
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef WAVE_H
#define WAVE_H

#include<stdint.h>

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
    //Data_block_t block;
} Data_t;

//typedef struct WAV_data_block {
//} Data_block_t;

typedef struct WAV_fotmat {
   RIFF_t riff;
   FMT_t fmt;
   Data_t data;
} Wav;


#endif 