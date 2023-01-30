#ifndef _AUDIO_TRUEPEAK_METER_H_
#define _AUDIO_TRUEPEAK_METER_H_

#include "audio_defines.h"

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(x[0]))
#endif

#define MAX_PHASES 12

typedef struct SamplePhaseFilters
{
  int numofphases;
  int numofcoeffs;
  float *in_buffer;

  float phase_filters[4][MAX_PHASES];
  float *phase_filters_p[4];

} SamplePhaseFilters;

SamplePhaseFilters* sample_phase_filters_create (void);
int sample_phase_filters_init (SamplePhaseFilters* );
void sample_phase_filters_destroy (SamplePhaseFilters* );
void sample_phase_filters_reset_states (SamplePhaseFilters* );
void sample_phase_filters_next_over_sample (SamplePhaseFilters* , float sample, float *results);

typedef struct AudioTruePeakMeter
{
  SamplePhaseFilters phasefilters;
} AudioTruePeakMeter;

AudioTruePeakMeter* audio_true_peak_meter_create (void);
int audio_true_peak_meter_init (AudioTruePeakMeter* );
int audio_true_peak_meter_deinit(AudioTruePeakMeter*);
void audio_true_peak_meter_destroy (AudioTruePeakMeter* );
void audio_true_peak_meter_reset_states (AudioTruePeakMeter* );
float audio_true_peak_meter_next_true_peak (AudioTruePeakMeter* , float sample);

#endif
