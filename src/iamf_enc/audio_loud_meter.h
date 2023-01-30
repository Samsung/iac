#ifndef _AUDIO_LOUD_METER_H_
#define _AUDIO_LOUD_METER_H_

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

#include "audio_defines.h"
#include "audio_true_peak_meter.h"
#include "biquad.h"

typedef float DataType;
typedef struct MyVector {
  DataType *Data;
  int size;
  void(*pushBack)(struct MyVector *, DataType);
  DataType(*iterAt)(struct MyVector *, int);
  int(*getSize)(struct MyVector *);
  void(*clearVector)(struct MyVector *);
} MyVector;

void MyVectoryInit(MyVector *thisp);
void MyVectoryDeinit(MyVector *thisp);



#ifndef countof
#define countof(x) (sizeof(x)/sizeof(x[0]))
#endif

typedef struct  {
  float meanSquare;
  float loudness;
}loudnessData_s;

typedef struct  {
  float meanSquare;
  float peaksqr;
  float truepeaksqr;
}resultData_s;

typedef struct AudioLoudMeter {
  MyVector proceedingMeanSquare;
  MyVector proceedingLoudness;
  // Store mean squares of previous sample blocks(for short - term loudness)
  float frameMeanSquareUpdated[20];
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

  void (*initParams)(struct AudioLoudMeter*, float , float , float );
  void (*prepare)(struct AudioLoudMeter*, int , int , channelLayout );
  float (*getEntirePeakSquare)(struct AudioLoudMeter*);
  float (*getEntireTruePeakSquare)(struct AudioLoudMeter*);
  float (*getIntegratedLoudness)(struct AudioLoudMeter*);
  void (*startIntegrated)(struct AudioLoudMeter*);
  void (*stopIntegrated)(struct AudioLoudMeter*);
  void (*resetIntegrated)(struct AudioLoudMeter*);
  resultData_s (*processFrameLoudness)(struct AudioLoudMeter*,float *, int , int );
  void (*processMomentaryLoudness)(struct AudioLoudMeter*,int );

  void (*shiftBlock)(struct AudioLoudMeter*, float *, int );
  float (*getChannelWeight)(struct AudioLoudMeter*, int );
  void (*genarateLoudnessReport)(struct AudioLoudMeter*, char *);
} AudioLoudMeter;

void AudioLoudMeterInit(AudioLoudMeter * thisp);
void AudioLoudMeterDeinit(AudioLoudMeter * thisp);

#endif
