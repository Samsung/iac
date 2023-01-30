
#ifndef _BIQUAD_H
#define _BIQUAD_H

typedef enum{
  bq_type_lowpass = 0,
  bq_type_highpass,
  bq_type_bandpass,
  bq_type_notch,
  bq_type_peak,
  bq_type_lowshelf,
  bq_type_highshelf
}BqType;


typedef struct Biquad {
  double a0, a1, a2, b0, b1, b2;
  double z1, z2;
  void(*resetBiquad)(struct Biquad*);
  void(*setBiquadCoeff)(struct Biquad*, double , double , double , double , double );
  void(*setBiquad)(struct Biquad*, int , double , double , double );
  void(*processBlock)(struct Biquad*, const float *, float *, int );
} Biquad;

void BiquadInit(Biquad *thisp);

#endif
