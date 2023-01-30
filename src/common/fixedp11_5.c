#include "fixedp11_5.h"

fixed16_t float_to_fixed(double input, int frac)
{
  return (fixed16_t)(round(input * (1 << frac)));
}

double fixed_to_float(fixed16_t input, int frac)
{
  return ((double)input / (double)(1 << frac));
}

float q_to_float(q16_t q, int frac)
{
  return ((float)q) * powf(2.0f, (float)-frac);
}

q16_t float_to_q(float q, int frac)
{
  return (q16_t)(q * powf(2.0f, (float)frac));
}

float qf_to_float(qf_t qf, int frac)
{
  return ((float)qf / (pow(2.0f, (float)frac) - 1.0)); //f = q / 255
}

qf_t float_to_qf(float f, int frac)
{
  if (f > 1)
    f = 1.0;
  else if (f < 0)
    f = 0;
  return (int)(f * (pow(2.0f, (float)frac) - 1)); // fq = f * 255
}

// linear to dB
float lin2db(float lin)
{
  if (lin == 0)
    lin = 1.0f / 32768.0f;
  return 20.0f * log10f(lin);
}

// dB to linear
float db2lin(float db)
{
  return powf(10.0f, 0.05f * db);
}

// Mapping of wIdx(k) to w(k)
#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif
static float widx2w_table[11] = { 0.0, 0.0179, 0.0391, 0.0658, 0.1038, 0.25, 0.3962, 0.4342, 0.4609, 0.4821, 0.5 };
float calc_w(int w_idx_offset, int w_idx_prev, int *w_idx)
{
  if (w_idx_offset > 0)
    *w_idx = MIN(w_idx_prev + 1, 10);
  else
    *w_idx = MAX(w_idx_prev - 1, 0);

  return widx2w_table[*w_idx];
}

float get_w(int w_idx)
{
    if (w_idx < 0)
        return widx2w_table[0];
    else if (w_idx > 10)
        return widx2w_table[10];

    return widx2w_table[w_idx];
}
