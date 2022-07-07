#include "audio_loud_meter.h"
#include <math.h>
void MyVectoryInit(MyVector *thisp);
void MyVectoryDeinit(MyVector *thisp);

void _pushback(MyVector *thisp, DataType data);
DataType _iterat(MyVector *thisp, int i);
void _constructor(MyVector *thisp)
{
  thisp->size = 0;
  thisp->Data = NULL;
}

void _denstructor(MyVector *thisp)
{
  if(thisp->Data)
    free(thisp->Data);
  thisp->size = 0;
  thisp->Data = NULL;
  thisp->getSize = NULL;
  thisp->iterAt = NULL;
  thisp->pushBack = NULL;
  thisp->clearVector = NULL;
}

void _pushback(MyVector *thisp, DataType value)
{
  int i;
  DataType *ptr;
  if (thisp->Data == NULL)
    ptr = (DataType *)malloc(sizeof((thisp->size + 1)*sizeof(DataType)));
  else
    ptr = (DataType *)realloc(thisp->Data, (thisp->size + 1)*sizeof(DataType));
  if (ptr == NULL)
    fprintf(stderr, "realloc failed \n");
  *(ptr + thisp->size) = value;
  thisp->size ++;
  thisp->Data = ptr;
}

DataType _iterat(MyVector *thisp, int i)
{
  return*(thisp->Data + i);
}

int _getsize(MyVector *thisp)
{
  return (thisp->size);
}

void _clearvector(MyVector *thisp)
{
  if(thisp->Data)
    free(thisp->Data);
  thisp->size = 0;
  thisp->Data = NULL;
}
void MyVectoryInit(MyVector *thisp)
{
  thisp->pushBack = _pushback;
  thisp->iterAt = _iterat;
  thisp->getSize = _getsize;
  thisp->clearVector = _clearvector;
  _constructor(thisp);
}

void MyVectoryDeinit(MyVector *thisp)
{
  _denstructor(thisp);
}

void _resetintegrated(AudioLoudMeter *thisp);
void _shiftblock(AudioLoudMeter *thisp, float *buffer, int buffer_len);
void _constructaudioloudmeter(AudioLoudMeter *thisp)
{
  MyVectoryInit(&(thisp->proceedingMeanSquare));
  MyVectoryInit(&(thisp->proceedingLoudness));

  MyVectoryInit(&(thisp->stepMeanSquareUpdated));
  MyVectoryInit(&(thisp->momentaryMeanSquareUpdated));
  MyVectoryInit(&(thisp->shortTermMeanSquareUpdated));

  Biquad * preFilter = thisp->preFilter;
  Biquad * highpassFilter = thisp->highpassFilter;
  AudioTruePeakMeter * truePeakMeter = thisp->truePeakMeter;
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    BiquadInit(&preFilter[i]);
    BiquadInit(&highpassFilter[i]);
    audio_true_peak_meter_init(&truePeakMeter[i]);
  }

  thisp->shortTermMeanSquareLength = 0;
  for (int i = 0; i < countof(thisp->shortTermMeanSquare); i++)
    thisp->shortTermMeanSquare[i] = 0;

  thisp->numChannels = 0;
  thisp->sampleRate = 0;
  thisp->blockSampleCount = 0;
  thisp->stepSampleCount = 0;
  thisp->blockStepCount = 0;

  thisp->blockDuration = 0.4; //400ms
  thisp->overLap = 0.75;
  thisp->stepDuration = thisp->blockDuration * (1 - thisp->overLap);
  thisp->shortTermDuration = 3; //3s

  thisp->IsIntegrating = 0;
  thisp->momentaryIntegrated = 0;
  thisp->shortTermExecuted = 0;
}


void _initparams(AudioLoudMeter *thisp, float block_duration, float over_lap, float shortterm_duration)
{
  thisp->blockDuration = block_duration;
  thisp->overLap = over_lap;
  thisp->stepDuration = thisp->blockDuration * (1 - thisp->overLap);
  thisp->shortTermDuration = shortterm_duration;
}

void _prepare(AudioLoudMeter *thisp, int sample_rate, int num_channels, channelLayout layout_ch)
{
  thisp->sampleRate = sample_rate;
  thisp->numChannels = num_channels;
  thisp->layoutChannel = layout_ch;

  for (int i = 0; i < countof(thisp->frameMeanSquareUpdated); i++)
  {
    thisp->frameMeanSquareUpdated[i] = 0;
  }
  thisp->frameMeanSquareLength = 0;

  thisp->stepMeanSquareUpdated.clearVector(&thisp->stepMeanSquareUpdated);
  thisp->momentaryMeanSquareUpdated.clearVector(&thisp->momentaryMeanSquareUpdated);
  thisp->shortTermMeanSquareUpdated.clearVector(&thisp->shortTermMeanSquareUpdated);

  //init momentary loudness
  thisp->blockSampleCount = (int)round(thisp->blockDuration * thisp->sampleRate);
  thisp->stepSampleCount = (int)round(thisp->blockSampleCount * (1 - thisp->overLap)); //BlockSampleCount == 400ms, StepSampleCount == 100ms
  thisp->blockStepCount = thisp->blockSampleCount / thisp->stepSampleCount;
  thisp->numChannels = num_channels;

  // init short - term loudness
  thisp->shortTermMeanSquareLength = (int)round(thisp->shortTermDuration / thisp->stepDuration);
  for (int i = 0; i < countof(thisp->shortTermMeanSquare); i++)
  {
    thisp->shortTermMeanSquare[i] = 0;
  }

  // init integrated loudness
  thisp->IsIntegrating = 0;

  for (int i = 0; i < num_channels; i++)
  {
    thisp->preFilter[i].setBiquadCoeff(&(thisp->preFilter[i]),
      -1.69065929318241,
      0.73248077421585,
      1.53512485958697,
      -2.69169618940638,
      1.19839281085285);

    thisp->highpassFilter[i].setBiquadCoeff(&(thisp->highpassFilter[i]),
      -1.99004745483398,
      0.99007225036621,
      1.0,
      -2.0,
      1.0);
  }
}

float _getentirepeaksquare(AudioLoudMeter *thisp)
{
  return (thisp->entirePeakSqr);
}

float _getentiretruepeaksquare(AudioLoudMeter *thisp)
{
  return (thisp->entireTruePeakSqr);
}


float _getintegratedloudness(AudioLoudMeter *thisp)
{
  if (thisp->proceedingMeanSquare.getSize(&(thisp->proceedingMeanSquare)) == 0 ||
    thisp->proceedingLoudness.getSize(&(thisp->proceedingLoudness)) == 0)
  {
    return 0;
  }

  // Gating of 400 ms blocks(overlapping by 75 % ), where two thresholds are used :
  MyVector absoluteGatedLoudness1;
  MyVector absoluteGatedLoudness2;

  MyVectoryInit(&(absoluteGatedLoudness1));
  MyVectoryInit(&(absoluteGatedLoudness2));

  float absoluteGateGamma = -70.0;
  loudnessData_s louddata;
  for (int i = 0; i < thisp->proceedingMeanSquare.getSize(&(thisp->proceedingMeanSquare)); i++)
  {
    louddata.meanSquare = thisp->proceedingMeanSquare.iterAt(&(thisp->proceedingMeanSquare),i);
    louddata.loudness = thisp->proceedingLoudness.iterAt(&(thisp->proceedingLoudness), i);
    if (louddata.loudness > absoluteGateGamma)
      absoluteGatedLoudness1.pushBack(&absoluteGatedLoudness1, louddata.meanSquare);
      absoluteGatedLoudness2.pushBack(&absoluteGatedLoudness2, louddata.loudness);

  }

  // Calc relativeGateGamma
  float absoluteGatedMeanSquareSum = 0;
  for (int i = 0; i < absoluteGatedLoudness1.getSize(&absoluteGatedLoudness1); i++)
  {
    louddata.meanSquare = absoluteGatedLoudness1.iterAt(&(absoluteGatedLoudness1), i);
    louddata.loudness = absoluteGatedLoudness2.iterAt(&(absoluteGatedLoudness2), i);
    absoluteGatedMeanSquareSum += louddata.meanSquare;
  }

  float absoluteGatedMeanSquare = 0;
  float relativeGateGamma = 0;
  if (absoluteGatedLoudness1.getSize(&absoluteGatedLoudness1) > 0)
  {
    absoluteGatedMeanSquare = absoluteGatedMeanSquareSum / (float)absoluteGatedLoudness1.getSize(&absoluteGatedLoudness1);
    if (absoluteGatedMeanSquare == 0)
    {
      printf("WARNING!!!: absoluteGatedMeanSquare == 0\n");
      absoluteGatedMeanSquare = 10e-20;
    }
    relativeGateGamma = -0.691 + 10 * log10(absoluteGatedMeanSquare) - 10;
  }
  else
  {
    relativeGateGamma = -0.691 - 10;
  }

  // The second at ?10 dB relative to the level measured after application of the first threshold.

  MyVector relativeGatedLoudness1;
  MyVector relativeGatedLoudness2;

  MyVectoryInit(&(relativeGatedLoudness1));
  MyVectoryInit(&(relativeGatedLoudness2));

  for (int i = 0; i < absoluteGatedLoudness1.getSize(&absoluteGatedLoudness1); i++)
  {
    louddata.meanSquare = absoluteGatedLoudness1.iterAt(&(absoluteGatedLoudness1), i);
    louddata.loudness = absoluteGatedLoudness2.iterAt(&(absoluteGatedLoudness2), i);
    if (louddata.loudness > relativeGateGamma)
    {
      relativeGatedLoudness1.pushBack(&relativeGatedLoudness1, louddata.meanSquare);
      relativeGatedLoudness2.pushBack(&relativeGatedLoudness2, louddata.loudness);
    }
  }

  float integratedLoudness = 0;
  float relativeGatedMeanSquare = 0;
  float relativeGatedMeanSquareSum = 0;
  if (relativeGatedLoudness1.getSize(&relativeGatedLoudness1) > 0)
  {
    for (int i = 0; i < relativeGatedLoudness1.getSize(&relativeGatedLoudness1); i++)
    {
      louddata.meanSquare = relativeGatedLoudness1.iterAt(&relativeGatedLoudness1,i);
      louddata.loudness = relativeGatedLoudness2.iterAt(&relativeGatedLoudness2, i);
      relativeGatedMeanSquareSum += louddata.meanSquare;
    }

    relativeGatedMeanSquare = relativeGatedMeanSquareSum / (float)relativeGatedLoudness1.getSize(&relativeGatedLoudness1);
    if (relativeGatedMeanSquare == 0)
    {
      printf("WARNING!!!: relativeGatedMeanSquare == 0\n");
      relativeGatedMeanSquare = 10e-20;
    }
    integratedLoudness = -0.691 + 10 * log10(relativeGatedMeanSquare);
  }

  MyVectoryDeinit(&(absoluteGatedLoudness1));
  MyVectoryDeinit(&(absoluteGatedLoudness2));

  MyVectoryDeinit(&(relativeGatedLoudness1));
  MyVectoryDeinit(&(relativeGatedLoudness2));

  return integratedLoudness;
}

void _startintegrated(AudioLoudMeter *thisp)
{
  _resetintegrated(thisp);
  thisp->IsIntegrating = 1;
}

void _stopintegrated(AudioLoudMeter *thisp)
{
  thisp->IsIntegrating = 0;
}

void _resetintegrated(AudioLoudMeter *thisp)
{
  for (int i = 0; i < countof(thisp->frameMeanSquareUpdated); i++)
    thisp->frameMeanSquareUpdated[i] = 0; //20ms? mean square
  thisp->frameMeanSquareLength = 0;


  thisp->stepMeanSquareUpdated.clearVector(&(thisp->stepMeanSquareUpdated));
  thisp->proceedingMeanSquare.clearVector(&(thisp->proceedingMeanSquare));
  thisp->proceedingLoudness.clearVector(&(thisp->proceedingLoudness));

  thisp->momentaryMeanSquareUpdated.clearVector(&(thisp->momentaryMeanSquareUpdated));
  thisp->shortTermMeanSquareUpdated.clearVector(&(thisp->shortTermMeanSquareUpdated));

  for (int i = 0; i < countof(thisp->shortTermMeanSquare); i++)
    thisp->shortTermMeanSquare[i] = 0;
  thisp->momentaryIntegrated = 0;
  thisp->shortTermExecuted = 0;
  thisp->entirePeakSqr = 0;
  thisp->entireTruePeakSqr = 0;
  // true peak measurement reset
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    audio_true_peak_meter_reset_states(&(thisp->truePeakMeter[i]));
  }
}

resultData_s _processframeloudness(AudioLoudMeter *thisp, float *in, int msize25pct, int in_frlen)
{
  float *clone = (float*)malloc(sizeof(float) *in_frlen * thisp->numChannels);
  float *buffer = clone;
  float frameMeanSquare = 0;
  float framePeakSqr = 0;
  float frameTruePeakSqr = 0;
  resultData_s processResult;  
  if(!clone)goto FAILED;
  if (thisp->IsIntegrating == 1)
  {
    for (int ch = 0; ch < thisp->numChannels; ch++)
    {
      memcpy(buffer + ch*in_frlen, in + ch*in_frlen, in_frlen * sizeof(float));
      thisp->preFilter[ch].processBlock(&(thisp->preFilter[ch]), buffer + ch*in_frlen, buffer + ch*in_frlen, in_frlen);
      thisp->highpassFilter[ch].processBlock(&(thisp->highpassFilter[ch]), buffer + ch*in_frlen, buffer + ch*in_frlen, in_frlen);
    }

    //Calc Frame loudness
    if (in_frlen > 0)
    {
      float *squared = (float*)malloc(sizeof(float) * in_frlen); 
      float *squared_bs1770 = (float*)malloc(sizeof(float) * in_frlen);
      if(!squared||!squared_bs1770){
	  	if(clone)
			free(clone);
		if(squared)
			free(squared);
		if(squared_bs1770)
			free(squared_bs1770);
		goto FAILED;
	  }
      for (int channel = 0; channel < thisp->numChannels; channel++)
      {
        float channelWeight = thisp->getChannelWeight(thisp,channel);
        if (channelWeight > 0)
        {
          float ch_maxsqr = 0;
          float ch_max_tpsqr = 0;
          float tp = 0;
          for (int i = 0; i < in_frlen; i++)
          {
            squared_bs1770[i] = buffer[channel * in_frlen + i] * buffer[channel * in_frlen + i];
            squared[i] = in[channel * in_frlen + i] * in[channel * in_frlen + i];
            if (ch_maxsqr < squared[i])
              ch_maxsqr = squared[i];
            tp = audio_true_peak_meter_next_true_peak(&(thisp->truePeakMeter[channel]),in[channel * in_frlen + i]);
            float tp_squared = tp * tp;


            if (ch_max_tpsqr < tp_squared)
              ch_max_tpsqr = tp_squared;
          }


          if (framePeakSqr < ch_maxsqr)
            framePeakSqr = ch_maxsqr;
          if (thisp->entirePeakSqr < framePeakSqr)
            thisp->entirePeakSqr = framePeakSqr;


          if (frameTruePeakSqr < ch_max_tpsqr)
            frameTruePeakSqr = ch_max_tpsqr;
          if (thisp->entireTruePeakSqr < frameTruePeakSqr)
            thisp->entireTruePeakSqr = frameTruePeakSqr;

          float channelSquareSum = 0;
          for (int i = 0; i < in_frlen; i++)
          {
            channelSquareSum += squared_bs1770[i];
          }

          frameMeanSquare += channelWeight * (channelSquareSum / (float)in_frlen);
        }
      }
      thisp->frameMeanSquareUpdated[thisp->frameMeanSquareLength] = frameMeanSquare;
      thisp->frameMeanSquareLength++;
      if (thisp->frameMeanSquareLength == msize25pct)
      {

        float stepSquareSum = 0;
        for (int i = 0; i < thisp->frameMeanSquareLength; i++)
        {
          stepSquareSum += thisp->frameMeanSquareUpdated[i];
          thisp->frameMeanSquareUpdated[i] = 0;
        }
        thisp->stepMeanSquareUpdated.pushBack(&(thisp->stepMeanSquareUpdated),stepSquareSum / (float)msize25pct);
        thisp->frameMeanSquareLength = 0;
      }
	  if(squared)
	  	free (squared);
	  if(squared_bs1770)
	  	free (squared_bs1770);
    }
  }
  if (clone != NULL)
	  free(clone);
FAILED:

  processResult.meanSquare = frameMeanSquare;
  processResult.peaksqr = framePeakSqr;
  processResult.truepeaksqr = frameTruePeakSqr;
  
  return (processResult);
}


void _processmomentaryloudness(AudioLoudMeter *thisp, int msize25pct)
{
  if (thisp->frameMeanSquareLength != 0 && thisp->frameMeanSquareLength < msize25pct)
  {

    float stepSquareSum = 0;
    for (int i = 0; i < thisp->frameMeanSquareLength; i++)
    {
      stepSquareSum += thisp->frameMeanSquareUpdated[i];
      thisp->frameMeanSquareUpdated[i] = 0;
    }
    thisp->stepMeanSquareUpdated.pushBack(&(thisp->stepMeanSquareUpdated),stepSquareSum / (float)msize25pct);
    thisp->frameMeanSquareLength = 0;
  }

  float blockIntegrationCount = 0;
  float *blockLoudness = NULL;
  blockLoudness = (float*)malloc(thisp->blockStepCount * sizeof(float));//
  if(!blockLoudness)return; 
  for (int i = 0; i < thisp->blockStepCount; i++)
    blockLoudness[i] = 0;

  for (int i = 0; i < thisp->stepMeanSquareUpdated.getSize(&(thisp->stepMeanSquareUpdated)); i++)
  {

    _shiftblock(thisp, blockLoudness, thisp->blockStepCount);
    blockLoudness[thisp->blockStepCount - 1] = thisp->stepMeanSquareUpdated.iterAt(&(thisp->stepMeanSquareUpdated),i);

    //Calc momentory loudness
    loudnessData_s louddata;
    float momentaryMeanSquare = 0;
    float momentaryLoudness = 0;
    float blockSquardSum = 0;
    for (int j = 0; j < thisp->blockStepCount; j++)
    {
      blockSquardSum += blockLoudness[j];
    }
    momentaryMeanSquare = blockSquardSum / (float)(thisp->blockStepCount);
    if (momentaryMeanSquare == 0)
    {
      printf("WARNING!!!: momentaryMeanSquare == 0\n");
      momentaryMeanSquare = 10e-20;
    }
    if (momentaryMeanSquare > 1)
    {
      //printf("momentaryMeanSquare>1\n");
    }
    thisp->momentaryMeanSquareUpdated.pushBack(&(thisp->momentaryMeanSquareUpdated),momentaryMeanSquare);
    momentaryLoudness = -0.691 + 10 * log10(momentaryMeanSquare);


    if (thisp->shortTermExecuted == 1)
    {
      // Calc short - term loudness, step Block Loudness
      // Short - term Loudness, Loudness Range
      _shiftblock(thisp, thisp->shortTermMeanSquare, thisp->shortTermMeanSquareLength);
      thisp->shortTermMeanSquare[thisp->shortTermMeanSquareLength - 1] = thisp->stepMeanSquareUpdated.iterAt(&(thisp->stepMeanSquareUpdated),i);
      float shortTermMeanSquaresSum = 0;
      float shortTermMeanSquareMean = 0;
      for (int j = 0; j < thisp->shortTermMeanSquareLength; j++)
        shortTermMeanSquaresSum += thisp->shortTermMeanSquare[j];
      shortTermMeanSquareMean = shortTermMeanSquaresSum / (float)(thisp->shortTermMeanSquareLength);
      float shortTermLoudness = 0;
      if (shortTermMeanSquareMean == 0)
      {
        printf("WARNING!!!: shortTermMeanSquareMean == 0\n");
        shortTermMeanSquareMean = 10e-20;
      }
      thisp->shortTermMeanSquareUpdated.pushBack(&(thisp->shortTermMeanSquareUpdated),shortTermMeanSquareMean);
      shortTermLoudness = -0.691 + 10 * log10(shortTermMeanSquareMean);
    }

    louddata.loudness = momentaryLoudness;
    louddata.meanSquare = momentaryMeanSquare;
    thisp->proceedingMeanSquare.pushBack(&(thisp->proceedingMeanSquare),louddata.meanSquare);
    thisp->proceedingLoudness.pushBack(&(thisp->proceedingLoudness),louddata.loudness);
  }
  if (blockLoudness)
  {
    free(blockLoudness);
    blockLoudness = NULL;
  }
}


void _shiftblock(AudioLoudMeter *thisp, float *buffer, int buffer_len)
{
  for (int i = 1; i < buffer_len; i++)
  {
    buffer[i - 1] = buffer[i];
  }
}

float _getchannelweight(AudioLoudMeter *thisp, int channel)
{
  //5.1, 7.1, 3.1.2, 5.1.2, 7.1.4
  float weight = 1.0f;
  switch (thisp->layoutChannel)
  {
  case CHANNEL51:
  case CHANNEL71:
  case CHANNEL512:
  case CHANNEL514:
  case CHANNEL712:
  case CHANNEL714:
    if (channel == 4 || channel == 5) // #Ls, Rs
      weight = 1.41f;
    break;
  }
  if (channel == 3)
    weight = 0.0f; //Lfe is not used in Loudness Calculations

  return weight;
}

void _genarateloudnessreport(AudioLoudMeter *thisp, char *filename)
{
  float momentaryMeanSquare;
  float momentaryLoundness;
  float shortTermMeanSquare;
  float shortTermLoundness;
  printf("Index, Momentary, ShortTerm\n");
  for (int i = 0; i < thisp->momentaryMeanSquareUpdated.getSize(&(thisp->momentaryMeanSquareUpdated)); i++)
  {
    momentaryMeanSquare = thisp->momentaryMeanSquareUpdated.iterAt(&(thisp->momentaryMeanSquareUpdated),i);
    momentaryLoundness = -0.691 + 10 * log10(momentaryMeanSquare);
    if (momentaryLoundness < -100)
      momentaryLoundness = -100;
    if (thisp->shortTermMeanSquareUpdated.getSize(&(thisp->shortTermMeanSquareUpdated)) > 0)
    {
      shortTermMeanSquare = thisp->shortTermMeanSquareUpdated.iterAt(&(thisp->shortTermMeanSquareUpdated),i);
      shortTermLoundness = -0.691 + 10 * log10(shortTermMeanSquare);
      if (shortTermLoundness > -100)
        shortTermLoundness = -100;
    }
    else
    {
      shortTermLoundness = 0;
    }
    printf("%d, %2.2f, %2.2f\n", i, momentaryLoundness, shortTermLoundness);
  }
}


void AudioLoudMeterInit(AudioLoudMeter * thisp)
{
  thisp->initParams = _initparams;
  thisp->prepare = _prepare;
  thisp->getEntirePeakSquare = _getentirepeaksquare;
  thisp->getEntireTruePeakSquare = _getentiretruepeaksquare;
  thisp->getIntegratedLoudness = _getintegratedloudness;
  thisp->startIntegrated = _startintegrated;
  thisp->stopIntegrated = _stopintegrated;
  thisp->resetIntegrated = _resetintegrated;

  thisp->processFrameLoudness = _processframeloudness;
  thisp->processMomentaryLoudness = _processmomentaryloudness;
  thisp->shiftBlock = _shiftblock;
  thisp->getChannelWeight = _getchannelweight;
  thisp->genarateLoudnessReport = _genarateloudnessreport;
  _constructaudioloudmeter(thisp);
}

void AudioLoudMeterDeinit(AudioLoudMeter * thisp)
{
  MyVectoryDeinit(&(thisp->proceedingMeanSquare));
  MyVectoryDeinit(&(thisp->proceedingLoudness));

  MyVectoryDeinit(&(thisp->stepMeanSquareUpdated));
  MyVectoryDeinit(&(thisp->momentaryMeanSquareUpdated));
  MyVectoryDeinit(&(thisp->shortTermMeanSquareUpdated));

  AudioTruePeakMeter * truePeakMeter = thisp->truePeakMeter;
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    audio_true_peak_meter_deinit(&truePeakMeter[i]);
  }
}
