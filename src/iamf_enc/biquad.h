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
 * @file biquad.h
 * @brief Biquad definition
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef _BIQUAD_H
#define _BIQUAD_H

typedef enum {
  bq_type_lowpass = 0,
  bq_type_highpass,
  bq_type_bandpass,
  bq_type_notch,
  bq_type_peak,
  bq_type_lowshelf,
  bq_type_highshelf
} BqType;

typedef struct Biquad {
  double a0, a1, a2, b0, b1, b2;
  double z1, z2;
  void (*resetBiquad)(struct Biquad *);
  void (*setBiquadCoeff)(struct Biquad *, double, double, double, double,
                         double);
  void (*setBiquad)(struct Biquad *, int, double, double, double);
  void (*processBlock)(struct Biquad *, const float *, float *, int);
} Biquad;

void BiquadInit(Biquad *thisp);

#endif
