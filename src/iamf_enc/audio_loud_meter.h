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
 * @file audio_loud_meter.c.h
 * @brief oudness measurement
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef _AUDIO_LOUD_METER_H_
#define _AUDIO_LOUD_METER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_defines.h"
#include "audio_true_peak_meter.h"
#include "biquad.h"

typedef float DataType;
typedef struct MyVector {
  DataType *Data;
  int size;
  void (*pushBack)(struct MyVector *, DataType);
  DataType (*iterAt)(struct MyVector *, int);
  int (*getSize)(struct MyVector *);
  void (*clearVector)(struct MyVector *);
} MyVector;

void MyVectoryInit(MyVector *thisp);
void MyVectoryDeinit(MyVector *thisp);

#ifndef countof
#define countof(x) (sizeof(x) / sizeof(x[0]))
#endif

typedef struct {
  float meanSquare;
  float loudness;
} loudnessData_s;

typedef struct {
  float meanSquare;
  float peaksqr;
  float truepeaksqr;
} resultData_s;

typedef struct AudioLoudMeter {
  MyVector proceedingMeanSquare;
  MyVector proceedingLoudness;
  // Store mean squares of previous sample blocks(for short - term loudness)
  float frameMeanSquareUpdated[128];
  int frameMeanSquareLength;
  MyVector stepMeanSquareUpdated;
  MyVector momentaryMeanSquareUpdated;
  MyVector shortTermMeanSquareUpdated;
  float shortTermMeanSquare[30];
  int shortTermMeanSquareLength;
  float entirePeakSqr;
  float entireTruePeakSqr;

  int sampleRate;
  int numChannels;
  channelLayout layoutChannel;

  int blockSampleCount;
  int stepSampleCount;
  int blockStepCount;
  float blockDuration;
  float overLap;
  float stepDuration;
  float shortTermDuration;

  int momentaryIntegrated;
  int shortTermExecuted;
  int IsIntegrating;

  Biquad preFilter[MAX_CHANNELS];
  Biquad highpassFilter[MAX_CHANNELS];
  AudioTruePeakMeter truePeakMeter[MAX_CHANNELS];

  void (*initParams)(struct AudioLoudMeter *, float, float, float);
  void (*prepare)(struct AudioLoudMeter *, int, int, channelLayout);
  float (*getEntirePeakSquare)(struct AudioLoudMeter *);
  float (*getEntireTruePeakSquare)(struct AudioLoudMeter *);
  float (*getIntegratedLoudness)(struct AudioLoudMeter *);
  void (*startIntegrated)(struct AudioLoudMeter *);
  void (*stopIntegrated)(struct AudioLoudMeter *);
  void (*resetIntegrated)(struct AudioLoudMeter *);
  resultData_s (*processFrameLoudness)(struct AudioLoudMeter *, float *, int,
                                       int);
  void (*processMomentaryLoudness)(struct AudioLoudMeter *, int);

  void (*shiftBlock)(struct AudioLoudMeter *, float *, int);
  float (*getChannelWeight)(struct AudioLoudMeter *, int);
  void (*genarateLoudnessReport)(struct AudioLoudMeter *, char *);
} AudioLoudMeter;

void AudioLoudMeterInit(AudioLoudMeter *thisp);
void AudioLoudMeterDeinit(AudioLoudMeter *thisp);

#endif
