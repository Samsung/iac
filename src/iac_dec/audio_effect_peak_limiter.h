#ifndef __AUDIO_PEAK_LIMITER_H_
#define __AUDIO_PEAK_LIMITER_H_

#define USE_TRUEPEAK 0

#if USE_TRUEPEAK
#include "audio_true_peak_meter.h"
#endif

#include <stdint.h>
#include "audio_defines.h"

void apply_gain(float *inblock, int in_len, int num_channels, float gain_db);
void apply_gain_from_to(float *inblock, int in_len, int num_channels,
        float from_db, float to_db);

typedef struct AudioEffectPeakLimiter
{
    float currentGain;
    float targetStartGain;
    float targetEndGain;
    float attackSec;
    float releaseSec;
    float linearThreashold;
    float currentTC;
    float incTC;
    int numChannels;

    float delayData[MAX_CHANNELS][MAX_DELAYSIZE + 1];
    float peakData[MAX_DELAYSIZE + 1];
    int entryIndex;
    int delaySize;
    int delayBufferSize;

#ifndef OLD_CODE
    int peak_pos;
#endif

    float *audioBlock;

#if USE_TRUEPEAK
    AudioTruePeakMeter truePeakMeters[MAX_CHANNELS];
#endif
} AudioEffectPeakLimiter;


AudioEffectPeakLimiter* audio_effect_peak_limiter_create (void);
void audio_effect_peak_limiter_init (AudioEffectPeakLimiter* ,
        float threashold_db, int sample_rate, int num_channels,
        float atk_sec, float rel_sec, int block_size, int delay_size);
int audio_effect_peak_limiter_process_block (AudioEffectPeakLimiter* ,
        float *inblock, float *outblock, int block_len);
void audio_effect_peak_limiter_uninit (AudioEffectPeakLimiter* );
void audio_effect_peak_limiter_destroy (AudioEffectPeakLimiter* );

#endif /* __AUDIO_PEAK_LIMITER_H_ */
