#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include "audio_effect_peak_limiter.h"
#include "immersive_audio_debug.h"


static int init_default(AudioEffectPeakLimiter* );
static float compute_target_gain(AudioEffectPeakLimiter* , float);
inline static float curve_accel(float x);

void apply_gain(float *inblock, int in_len, int num_channels, float gain_db)
{
    float k = powf(10, gain_db / 20);
    float *buffer;
    for (int channel = 0; channel < num_channels; channel++)
    {
        buffer = inblock + channel * in_len;
        for (int i = 0; i < in_len; i++)
            buffer[i] = buffer[i] * k;
    }
}

void apply_gain_from_to(float *inblock, int in_len, int num_channels,
        float from_db, float to_db)
{
    float gain_db = to_db - from_db;
    apply_gain(inblock, in_len, num_channels, gain_db);
}


AudioEffectPeakLimiter*
audio_effect_peak_limiter_create (void)
{
    AudioEffectPeakLimiter* ths = NULL;
    ths =
        (AudioEffectPeakLimiter*) malloc (sizeof (AudioEffectPeakLimiter));
    return ths;
}

void audio_effect_peak_limiter_uninit (AudioEffectPeakLimiter* ths)
{
    if (ths && ths->audioBlock)
        free(ths->audioBlock);

#if USE_TRUEPEAK
        for (int c=0; c<MAX_CHANNELS; ++c) {
            audio_true_peak_meter_deinit(&ths->truePeakMeters[c]);
        }
#endif
}

void audio_effect_peak_limiter_destroy (AudioEffectPeakLimiter* ths)
{
    audio_effect_peak_limiter_uninit (ths);
    if (ths)
        free(ths);
}

//threashold_db: Peak threshold in dB
//sample_rate : Sample rate of the samples
//num_channels: number of channels in frame
//atk_sec : attack duration in seconds
//rel_sec : release duration in seconds
//block_size: number of samples in frame
//delay_size: number of samples in delay buffer
void audio_effect_peak_limiter_init(AudioEffectPeakLimiter* ths,
        float threashold_db, int sample_rate, int num_channels,
        float atk_sec, float rel_sec, int block_size, int delay_size)
{
    init_default(ths);

    ths->linearThreashold = pow(10, threashold_db / 20);
    ths->attackSec = atk_sec;
    ths->releaseSec = rel_sec;
    ths->incTC = (float)1 / (float)sample_rate;
    ths->numChannels = num_channels;

    ths->audioBlock =
        (float *)malloc(sizeof(float) * block_size * num_channels);

    ths->delaySize = delay_size;
    ths->delayBufferSize = delay_size;

    for (int channel = 0; channel < num_channels; channel++) {
        for (int i = 0; i < MAX_DELAYSIZE+1; i++)
            ths->delayData[channel][i] = 0.0f;
    }

}

int audio_effect_peak_limiter_process_block (AudioEffectPeakLimiter* ths,
        float *inblock, float *outblock, int block_len)
{
    // Look ahead
    float peak;
    float channel_peak = 0.0f;
    float gain;
    int k;
    float peakMax = 0.0f;
#if USE_TRUEPEAK
    float data;
#endif

    if (!inblock)
        return (0);
    memset(ths->audioBlock, 0, block_len * ths->numChannels * sizeof(float));

    for (k = 0; k < block_len; k++) {
        peak = 0.0f;
#ifndef OLD_CODE
        if (ths->peak_pos < 0) {
#endif
            for (int i = 0; i < ths->delaySize; i++) {
                channel_peak =
                    ths->peakData[(k + i + ths->entryIndex) % ths->delayBufferSize];
                if (channel_peak > peak)
                    peak = channel_peak;
            }

#ifndef OLD_CODE
        } else {
            peak = ths->peakData[ths->peak_pos];
        }

        ia_logt("index %d : peak value %f vs %f", k, peak,
                ths->peak_pos < 0 ? 0 : ths->peakData[ths->peak_pos]);
#else
        ia_logt("index %d : peak value %f", k, peak);
#endif
        gain = compute_target_gain(ths, peak);
        ia_logt("index %d : gain value %f", k, gain);
        peakMax = 0;

        for (int channel = 0; channel < ths->numChannels; channel++) {
            if (ths->delaySize > 0) {
                ths->audioBlock[channel*block_len + k] =
                    ths->delayData[channel][(k + ths->entryIndex) % ths->delayBufferSize] * gain;
#if USE_TRUEPEAK
                data =
#endif
                ths->delayData[channel][(k + ths->entryIndex) % ths->delayBufferSize] =
                    inblock[channel*block_len + k];
            }
            else
            { // no delay mode
                ths->audioBlock[channel*block_len + k] =
                    inblock[channel*block_len + k] * gain;
#if USE_TRUEPEAK
                data = inblock[channel*block_len + k];
#endif
            }
#if USE_TRUEPEAK
            // compute true peak if you want
            ia_logt("data value %f", data);
            channel_peak =
                audio_true_peak_meter_next_true_peak(&ths->truePeakMeters[channel], data);
            channel_peak = fabs(channel_peak);
            if (channel_peak > peakMax)
                peakMax = channel_peak;
#else
            channel_peak =
                fabs(ths->delayData[channel][(k + ths->entryIndex) % ths->delayBufferSize]);
            if (channel_peak > peakMax)
                peakMax = channel_peak;
#endif
        }

#ifndef OLD_CODE
        if (ths->peak_pos == ((k + ths->entryIndex) % ths->delayBufferSize))
            ths->peak_pos = -1;
        else if (ths->peak_pos < 0 || ths->peakData[ths->peak_pos] < peakMax)
            ths->peak_pos = (k + ths->entryIndex) % ths->delayBufferSize;
#endif

        ths->peakData[(k + ths->entryIndex) % ths->delayBufferSize] = peakMax;
        ia_logt("index %d : peak max value %.10f", k, peakMax);
    }

    if (ths->delaySize > 0) {
        ths->entryIndex = (ths->entryIndex + block_len) % ths->delayBufferSize;
    }

    // transmit the block and release memory
    memcpy(outblock, ths->audioBlock,
            block_len * ths->numChannels * sizeof(float));
    return (block_len);
}

int init_default(AudioEffectPeakLimiter* ths)
{
    int ret = -1;
    if (ths) {
        audio_effect_peak_limiter_uninit (ths);
        memset(ths, 0x00, sizeof(AudioEffectPeakLimiter));

        ths->currentGain = 1.0;
        ths->targetStartGain = -1.0;
        ths->targetEndGain = -1.0;
        ths->attackSec = -1.0;
        ths->releaseSec = -1.0;
        ths->currentTC = -1.0;
        ths->entryIndex = 0;
        ths->incTC = 0;

        for (int i = 0; i < MAX_DELAYSIZE+1; i++)
            ths->peakData[i] = 0.0f;
#ifndef OLD_CODE
        ths->peak_pos = -1;
#endif

#if USE_TRUEPEAK
        for (int c=0; c<MAX_CHANNELS; ++c) {
            audio_true_peak_meter_init(&ths->truePeakMeters[c]);
        }
#endif
        ret = 0;
    }
    return ret;
}

float compute_target_gain(AudioEffectPeakLimiter *ths, float peak)
{
    float acc_ratio = 0;
    float gain = 0;

    if (ths->currentTC != -1 && ths->currentTC < ths->attackSec) {
        ths->currentTC += ths->incTC;
        acc_ratio = curve_accel(ths->currentTC / ths->attackSec);
        gain = ths->targetStartGain -
            acc_ratio * (ths->targetStartGain - ths->targetEndGain);
        ths->currentGain = gain;
    } else if (ths->currentTC != -1 &&
            ths->currentTC < ths->releaseSec + ths->attackSec) {
        ths->currentTC += ths->incTC;
        acc_ratio =
            curve_accel((ths->currentTC - ths->attackSec) / ths->releaseSec);
        gain = ths->targetEndGain + acc_ratio * (1.0f - ths->targetEndGain);
        ths->currentGain = gain;
    } else {
        ths->currentGain = 1.0;
    }

    if (peak * ths->currentGain > ths->linearThreashold) { // peak detect
        ths->targetStartGain = ths->currentGain;
        ths->targetEndGain = ths->linearThreashold / peak;
        ths->currentTC = 0.0f;
    }

    return ths->currentGain;
}

float curve_accel(float x)
{ // x = 0.0, y = 0.0 --> x = 1.0, y = 1.0
    if (1.0 < x)
        return 1.0f;
    if (x < 0)
        return 0.0f;
    return 1.0f - powf(x - 1, 2.0);
}
