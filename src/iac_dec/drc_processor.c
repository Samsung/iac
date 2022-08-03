/******************************************************************************
*                       Samsung Electronics Co., Ltd.                        *
*                                                                            *
*                           Copyright (C) 2021                               *
*                          All rights reserved.                              *
*                                                                            *
* This software is the confidential and proprietary information of Samsung   *
* Electronics Co., Ltd. ("Confidential Information"). You shall not disclose *
* such Confidential Information and shall use it only in accordance with the *
* terms of the license agreement you entered into with Samsung Electronics   *
* Co., Ltd.                                                                  *
*                                                                            *
* Removing or modifying of the above copyright notice or the following       *
* descriptions will terminate the right of using this software.              *
*                                                                            *
* As a matter of courtesy, the authors request to be informed about uses of  *
* this software and about bugs in this software.                             *
******************************************************************************/

#include "drc_processor.h"
#include "fixedp11_5.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_decoder.h"

#define DRC_LevelAttackSec      0.001f
#define DRC_LevelReleaseSec     0.30f
#define DRC_GainAttackSec       0.005f
#define DRC_GainReleaseSec      0.05f
#define DRC_LookAhead           240

static void apply_gain(float *inblock, int in_len, int num_channels, float gain_db)
{
    float k = db2lin(gain_db);
    float *buffer;
    ia_logd ("gain db %f, gain : %f", gain_db, k);
    for (int channel = 0; channel < num_channels; channel++) {
        buffer = inblock + channel * in_len;
        for (int i = 0; i < in_len; i++)
            buffer[i] = buffer[i] * k;
    }
}


void drc_init_compressor(AudioEffectCompressor *compressor,
                         int input_sample_rate, int input_nchannels,
                         int frame_size)
{
    audio_effect_compressor_set_default_compression_curve(compressor);
    audio_effect_compressor_init (compressor, input_sample_rate, input_nchannels,
            DRC_LevelAttackSec, DRC_LevelReleaseSec,
            DRC_GainAttackSec, DRC_GainReleaseSec,
            DRC_LookAhead);
}

void drc_init_limiter(AudioEffectPeakLimiter *limiter, int input_sample_rate,
                      int input_nchannels)
{
    audio_effect_peak_limiter_init(limiter, LIMITER_MaximumTruePeak,
            input_sample_rate, input_nchannels,
            LIMITER_AttackSec, LIMITER_ReleaseSec, LIMITER_LookAhead);
}

void drc_process_block(int output_mode, float input_loudness, float *inblock,
                       float *outblock, int frame_size, int nchannels,
                       AudioEffectCompressor *compressor,
                       AudioEffectPeakLimiter *truepeaklimiter)
{
    float gain;

    switch (output_mode) {
        case IA_DRC_TV_MODE:
            gain = -24.0 - input_loudness;
            apply_gain(inblock, frame_size, nchannels, gain);
            audio_effect_peak_limiter_process_block(truepeaklimiter, inblock, outblock, frame_size);
            break;
        case IA_DRC_MODILE_MODE:
            gain = -24.0 - input_loudness;
            apply_gain(inblock, frame_size, nchannels, gain);
            audio_effect_compressor_process_block(compressor, inblock, outblock, frame_size);
            apply_gain(outblock, frame_size, nchannels, 8.0); // -24 --> -16
            audio_effect_peak_limiter_process_block(truepeaklimiter, outblock, outblock, frame_size);
            break;
        default:
            audio_effect_peak_limiter_process_block(truepeaklimiter, inblock, outblock, frame_size);
            break;
    }
}
