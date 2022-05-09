#include "biquad.h"
#define _USE_MATH_DEFINES
#include <math.h>

void _constructbiquad(Biquad *thisp)
{
  thisp->a1 = thisp->a2 = thisp->b0 = thisp->b1 = thisp->b2 = 0.0;
  thisp->z1 = thisp->z2 = 0.0;
}

void _resetbiquad(Biquad *thisp)
{
  thisp->a0 = 1.0;
  thisp->a1 = thisp->a2 = thisp->b0 = thisp->b1 = thisp->b2 = 0.0;
  thisp->z1 = thisp->z2 = 0.0;
}

void _setbiquadcoeff(Biquad *thisp, double a1, double a2, double b0, double b1, double b2)
{
  _resetbiquad(thisp);
  thisp->a1 = a1;
  thisp->a2 = a2;
  thisp->b0 = b0;
  thisp->b1 = b1;
  thisp->b2 = b2;
}

void _setbiquad(Biquad *thisp, int type, double Fc, double Q, double peakGain)
{
  double norm;
  double V = pow(10, fabs(peakGain) / 20.0);
  double K = tan(M_PI * Fc);
  _resetbiquad(thisp);
  switch (type) {
  case bq_type_lowpass:
    norm = 1 / (1 + K / Q + K * K);
    thisp->a0 = K * K * norm;
    thisp->a1 = 2 * thisp->a0;
    thisp->a2 = thisp->a0;
    thisp->b1 = 2 * (K * K - 1) * norm;
    thisp->b2 = (1 - K / Q + K * K) * norm;
    break;

  case bq_type_highpass:
    norm = 1 / (1 + K / Q + K * K);
    thisp->a0 = 1 * norm;
    thisp->a1 = -2 * thisp->a0;
    thisp->a2 = thisp->a0;
    thisp->b1 = 2 * (K * K - 1) * norm;
    thisp->b2 = (1 - K / Q + K * K) * norm;
    break;

  case bq_type_bandpass:
    norm = 1 / (1 + K / Q + K * K);
    thisp->a0 = K / Q * norm;
    thisp->a1 = 0;
    thisp->a2 = -thisp->a0;
    thisp->b1 = 2 * (K * K - 1) * norm;
    thisp->b2 = (1 - K / Q + K * K) * norm;
    break;

  case bq_type_notch:
    norm = 1 / (1 + K / Q + K * K);
    thisp->a0 = (1 + K * K) * norm;
    thisp->a1 = 2 * (K * K - 1) * norm;
    thisp->a2 = thisp->a0;
    thisp->b1 = thisp->a1;
    thisp->b2 = (1 - K / Q + K * K) * norm;
    break;

  case bq_type_peak:
    if (peakGain >= 0) {    // boost
      norm = 1 / (1 + 1 / Q * K + K * K);
      thisp->a0 = (1 + V / Q * K + K * K) * norm;
      thisp->a1 = 2 * (K * K - 1) * norm;
      thisp->a2 = (1 - V / Q * K + K * K) * norm;
      thisp->b1 = thisp->a1;
      thisp->b2 = (1 - 1 / Q * K + K * K) * norm;
    }
    else {    // cut
      norm = 1 / (1 + V / Q * K + K * K);
      thisp->a0 = (1 + 1 / Q * K + K * K) * norm;
      thisp->a1 = 2 * (K * K - 1) * norm;
      thisp->a2 = (1 - 1 / Q * K + K * K) * norm;
      thisp->b1 = thisp->a1;
      thisp->b2 = (1 - V / Q * K + K * K) * norm;
    }
    break;
  case bq_type_lowshelf:
    if (peakGain >= 0) {    // boost
      norm = 1 / (1 + sqrt(2.0) * K + K * K);
      thisp->a0 = (1 + sqrt(2 * V) * K + V * K * K) * norm;
      thisp->a1 = 2 * (V * K * K - 1) * norm;
      thisp->a2 = (1 - sqrt(2 * V) * K + V * K * K) * norm;
      thisp->b1 = 2 * (K * K - 1) * norm;
      thisp->b2 = (1 - sqrt(2.0) * K + K * K) * norm;
    }
    else {    // cut
      norm = 1 / (1 + sqrt(2 * V) * K + V * K * K);
      thisp->a0 = (1 + sqrt(2.0) * K + K * K) * norm;
      thisp->a1 = 2 * (K * K - 1) * norm;
      thisp->a2 = (1 - sqrt(2.0) * K + K * K) * norm;
      thisp->b1 = 2 * (V * K * K - 1) * norm;
      thisp->b2 = (1 - sqrt(2 * V) * K + V * K * K) * norm;
    }
    break;
  case bq_type_highshelf:
    if (peakGain >= 0) {    // boost
      norm = 1 / (1 + sqrt(2.0) * K + K * K);
      thisp->a0 = (V + sqrt(2 * V) * K + K * K) * norm;
      thisp->a1 = 2 * (K * K - V) * norm;
      thisp->a2 = (V - sqrt(2.0 * V) * K + K * K) * norm;
      thisp->b1 = 2 * (K * K - 1) * norm;
      thisp->b2 = (1 - sqrt(2.0) * K + K * K) * norm;
    }
    else {    // cut
      norm = 1 / (V + sqrt(2 * V) * K + K * K);
      thisp->a0 = (1 + sqrt(2.0) * K + K * K) * norm;
      thisp->a1 = 2 * (K * K - 1) * norm;
      thisp->a2 = (1 - sqrt(2.0) * K + K * K) * norm;
      thisp->b1 = 2 * (K * K - V) * norm;
      thisp->b2 = (V - sqrt(2 * V) * K + K * K) * norm;
    }
    break;
  }

  return;
}


void _processblock(Biquad *thisp, const float *pSrc, float *pDst, int samples_len)
{
  int i;
  for (i = 0; i < samples_len; i++)
  {
    double in = pSrc[i];
    double factorForB0 = in - thisp->a1 * thisp->z1 - thisp->a2 * thisp->z2;
    double out = thisp->b0 * factorForB0 + thisp->b1 * thisp->z1 + thisp->b2 * thisp->z2;
    thisp->z2 = thisp->z1;
    thisp->z1 = factorForB0;
    pDst[i] = out;
  }
}


void BiquadInit(Biquad *thisp)
{
  thisp->resetBiquad = _resetbiquad;
  thisp->setBiquadCoeff = _setbiquadcoeff;
  thisp->setBiquad = _setbiquad;
  thisp->processBlock = _processblock;
  _constructbiquad(thisp);
}

void test_biquad(int argc, char *argv[])
{
  Biquad prefilter;
  Biquad highpassfilter;
  BiquadInit(&prefilter);
  BiquadInit(&highpassfilter);
  prefilter.setBiquadCoeff(&prefilter,
    -1.69065929318241,
    0.73248077421585,
    1.53512485958697,
    -2.69169618940638,
    1.19839281085285);

  highpassfilter.setBiquadCoeff(&highpassfilter,
    -1.99004745483398,
    0.99007225036621,
    1.0,
    -2.0,
    1.0);
}