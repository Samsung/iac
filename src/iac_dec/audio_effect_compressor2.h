#ifndef _AUDIO_EFFECT_COMPRESSOR2_H
#define _AUDIO_EFFECT_COMPRESSOR2_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "audio_defines.h"

typedef struct compressionCurve {
    float marginDB;
    float offsetDB;
    float kneeDB[5];
    float compressionRatio[5];
} compressionCurve;

// ---------------------------------------------------------------------------

typedef struct AudioEffectCompressor2
{
    float delayData[MAX_DELAYSIZE + 1];   // The circular delay line for the signal
    int in_index;  // Pointer to next block update entry
    int delaySize;
    float sampleRateHz;
    float gainAttackSec;
    float gainReleaseSec;
    float attack_const, release_const;
    float alpha;
    float beta;

    compressionCurve curve0;
    float outKneeDB[5];
    float slope[5];
    float lastPeak;
    float lastInputDB;
    float lastTargetScale;

#ifndef OLD_CODE
    int   peak_pos;
#endif
} AudioEffectCompressor2;

void audio_effect_compressor2_set_compression_curve (AudioEffectCompressor2* , compressionCurve* );
//convert time constants from seconds to unitless parameters
void audio_effect_compressor2_set_attack_release_sec(AudioEffectCompressor2*,
    const float level_atk_sec, const float level_rel_sec,
    const float gain_atk_sec, const float gain_rel_sec);

void audio_effect_compressor2_init(AudioEffectCompressor2*, int sample_rate_hz, int delay_size);
int audio_effect_compressor2_process_block(AudioEffectCompressor2*, float *inblock, float *outblock, int block_len);
#endif
