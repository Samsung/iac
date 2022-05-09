#ifndef _FIXEDP11_5_H_
#define _FIXEDP11_5_H_
#include <stdint.h>
#include <math.h>

#define FIXED_POINT_FRACTIONAL_BITS 14

/// Fixed-point Format: 2.14 (16-bit)
typedef int16_t fixed16_t;
typedef int16_t q16_t;
typedef uint8_t qf_t;

/// Converts between double and fixed16_t
fixed16_t float_to_fixed(double input, int frac);
double fixed_to_float(fixed16_t input, int frac);

/// Converts between double and q16_t
float q_to_float(q16_t q, int frac);
q16_t float_to_q(float q, int frac);

/// Converts between double and q8_t
float qf_to_float(qf_t q, int frac);
qf_t float_to_qf(float q, int frac);

float lin2db(float lin);
float db2lin(float db);
#endif
