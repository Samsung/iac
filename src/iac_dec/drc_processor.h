#ifndef _DRCPROCESSOR_H_
#define _DRCPROCESSOR_H_

#include "audio_effect_peak_limiter.h"

void drc_init_limiter(AudioEffectPeakLimiter *limiter, int input_sample_rate, int input_nchannels);
void drc_process_block(int output_mode, float input_loudness, float *inblock,
        float *outblock, int block_len, int nchannels,
        AudioEffectPeakLimiter *truepeaklimiter);

#endif /* _DRCPROCESSOR_H_ */
