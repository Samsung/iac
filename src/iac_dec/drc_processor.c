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

void drc_init_limiter(AudioEffectPeakLimiter *limiter, int input_sample_rate, int input_nchannels)
{
    audio_effect_peak_limiter_init(limiter, LIMITER_MaximumTruePeak,
            input_sample_rate, input_nchannels,
            LIMITER_AttackSec, LIMITER_ReleaseSec,
            FRAME_SIZE,
#ifndef NODELAY_PROCESS
            LIMITER_LookAhead
#else
            0
#endif
            );
}

void drc_process_block(int output_mode, float input_loudness, float *inblock,
        float *outblock, int block_len, int nchannels,
        AudioEffectPeakLimiter *truepeaklimiter)
{
    float gain;

    switch (output_mode)
    {
    case 0:
        audio_effect_peak_limiter_process_block(truepeaklimiter, inblock, outblock, block_len);
        break;
    case 1:
        gain = -24.0 - input_loudness;
        apply_gain(inblock, block_len, nchannels, gain);
        audio_effect_peak_limiter_process_block(truepeaklimiter, inblock, outblock, block_len);
        break;
    case 2:
        gain = -24.0 - input_loudness;
        apply_gain(inblock, block_len, nchannels, gain);
        apply_gain(outblock, block_len, nchannels, 8.0); // -24 --> -16
        audio_effect_peak_limiter_process_block(truepeaklimiter, outblock, outblock, block_len);
        break;
    }
}
