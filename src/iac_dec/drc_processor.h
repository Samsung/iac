#ifndef _DRCPROCESSOR_H_
#define _DRCPROCESSOR_H_

#include "audio_effect_peak_limiter.h"
#include "audio_effect_compressor.h"

void drc_init_compressor(AudioEffectCompressor *compressor,
                         int input_sample_rate, int input_nchannels,
                         int frame_size);
void drc_init_limiter(AudioEffectPeakLimiter *limiter, int input_sample_rate,
                      int input_nchannels);
void drc_process_block(int output_mode, float input_loudness, float *inblock,
                       float *outblock, int frame_size, int nchannels,
                       AudioEffectCompressor *compressor,
                       AudioEffectPeakLimiter *truepeaklimiter);

#endif /* _DRCPROCESSOR_H_ */
