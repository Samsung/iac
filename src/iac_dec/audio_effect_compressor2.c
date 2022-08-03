#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "audio_effect_compressor2.h"
#include "immersive_audio_debug.h"
#include "fixedp11_5.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "AECOMP"

static float smoothTargetScale (AudioEffectCompressor2* this,
                                float prev_target_scale, float gain_diff);


//  initParams()
void audio_effect_compressor2_init (AudioEffectCompressor2* this,
                                    int sample_rate_hz, int delay_size)
{
    this->sampleRateHz = (float)sample_rate_hz;

    int kk;
    int n = sizeof(this->outKneeDB) / sizeof(this->outKneeDB[0]);

    // calcuate this->slope values for each region
    for (kk = 0; kk < n; kk++) { // Keeps division out of initParams()
        this->slope[kk] = 1.0f / this->curve0.compressionRatio[kk];
        ia_logi ("idx %d: slope %f", kk, this->slope[kk]);
    }

    this->outKneeDB[0] = (this->curve0.kneeDB[0] < this->curve0.marginDB) ? this->curve0.kneeDB[0] : this->curve0.marginDB;
    ia_logi ("idx 0: outKneeDB %f", this->outKneeDB[0]);
    for (kk = 1; kk < n; kk++) {
        this->outKneeDB[kk] = this->outKneeDB[kk - 1] + this->slope[kk - 1] * (this->curve0.kneeDB[kk] - this->curve0.kneeDB[kk - 1]);

        if (this->curve0.marginDB < this->outKneeDB[kk]) {
            this->outKneeDB[kk] = this->curve0.marginDB;
        }
        ia_logi ("idx %d: outKneeDB %f", kk, this->outKneeDB[kk]);
    }

    this->lastPeak = 0.0f;
    this->lastInputDB = 0.0f;
    this->lastTargetScale = 1.0f;

    this->in_index = 0; // Pointer to next block update entry
    this->delaySize = delay_size;
    for (int i = 0; i < this->delaySize; i++)
        this->delayData[i] = 0.0f;
#ifndef OLD_CODE
    this->peak_pos = -1;
#endif

    audio_effect_compressor2_set_attack_release_sec (this, 0.01f, 0.3f, 0.01f, 0.3f); // levelattack, levelrelease, gainattack, gainrelease
}

// processBlock()
int audio_effect_compressor2_process_block (AudioEffectCompressor2* this,
                                            float *inblock, float *outblock,
                                            int frame_size)
{
    float vpeak;
    float vpeakDB, delayMaxDB;
    float vin_db = 0.0f;
    float vout_db = 0.0f;
    float target_scale;
    int n = sizeof(this->outKneeDB) / sizeof(this->outKneeDB[0]);

    float *audio_block = outblock;
    float a = 0, delayMax;
    int     idx = 0;

    if (!inblock) return (0);

    memset (audio_block, 0, sizeof(float) * frame_size);

    // Find the smoothed envelope, target gain and compressed output
    vpeak = this->lastPeak;
    target_scale = this->lastTargetScale;

#define DB_IDX(i) ((i) % this->delaySize)
    for (int k = 0; k<frame_size; k++) {
        idx = k + this->in_index;

        delayMax = 0;
#ifndef OLD_CODE
        if (this->peak_pos < 0) {
#endif
            for (int i = this->delaySize - 1; i >= round(this->delaySize / 2.0f); i--) {
                a = fabs(this->delayData[DB_IDX(idx + i)]);
                if (a > delayMax)  {
                    delayMax = a;
#ifndef OLD_CODE
                    this->peak_pos = DB_IDX(idx + i);
#endif
                }
            }
#ifndef OLD_CODE
        } else {
            delayMax = fabs(this->delayData[DB_IDX(this->peak_pos)]);
        }
#endif

        vpeakDB = lin2db(vpeak);
        delayMaxDB = lin2db(delayMax);

        if (delayMaxDB >= vpeakDB)
        { // Attack (rising level) --> up trend phase
            vpeakDB = this->alpha * vpeakDB + (1.0f - this->alpha) * delayMaxDB;
        }
        else if ((vpeakDB - delayMaxDB) > 0.01f)
        { // Release (decay for falling level) --> down trend phase
            vpeakDB = this->beta * vpeakDB + (1.0f - this->beta) * delayMaxDB;
        }
        else
            vpeakDB = delayMaxDB;

        // Convert to dB
        vpeak = db2lin(vpeakDB);

        vin_db = vpeakDB;

        // Find gain point.
        float ratio_1;
        if (vin_db > this->curve0.marginDB) {
            ratio_1 = this->slope[n - 1];
            vout_db = this->outKneeDB[n - 1] + ratio_1 * (vin_db - this->curve0.marginDB);
        } else {
            for (int kk = 0; kk < n; kk++) {
                if (vin_db <= this->curve0.kneeDB[kk]) {
                    if (kk == 0) { // vInDB <= this->curve0.kneeDB[0]
                        ratio_1 = 1.0f;
                        vout_db = this->outKneeDB[0] + ratio_1 * (vin_db - this->curve0.kneeDB[0]);
                    } else { // this->curve0.kneeDB[0] < vInDB <= this->curve0.kneeDB[n-1]
                        ratio_1 = this->slope[kk - 1];
                        vout_db = this->outKneeDB[kk - 1] + ratio_1 * (vin_db - this->curve0.kneeDB[kk - 1]);
                    }
                    break;
                }
            }
        }

        // Convert the needed gain back(dB) to a linear scale
        target_scale = smoothTargetScale(this, target_scale, vout_db - vin_db);
        // And apply target gain to signal stream
        if (this->delaySize > 0) {
            audio_block[k] = target_scale * this->delayData[DB_IDX(idx)];
        } else {
            audio_block[k] = target_scale * inblock[k];
        }
        if (this->delaySize > 0) {
            this->delayData[DB_IDX(idx)] = inblock[k];
        }

#ifndef OLD_CODE
        if (this->peak_pos == DB_IDX(idx + this->delaySize - (int)round(this->delaySize / 2.0f)))
            this->peak_pos = -1;
        else if (this->peak_pos < 0 || delayMax < fabs(inblock[k]))
                this->peak_pos = DB_IDX(idx);
#endif
    }
    if (this->delaySize > 0) {
        this->in_index = (this->in_index + frame_size) % this->delaySize;
    }

    this->lastPeak = vpeak;
    this->lastInputDB = vin_db;
    this->lastTargetScale = target_scale;

    return (frame_size);
}

float smoothTargetScale (AudioEffectCompressor2* this,
        float prev_target_scale, float gain_diff)
{
    float one_minus_attack_const = 1.0f - this->attack_const;
    float one_minus_release_const = 1.0f - this->release_const;
    float prev_target_scale_dB = lin2db(prev_target_scale);
    float target_scale_dB = gain_diff;
    float target_scale = 0.0f;

    if (gain_diff == 0.0f) {
        target_scale_dB = 0.0f; // if no difference then target_scale is 0.
    }

    //smooth the gain using the attack or release constants
    if (target_scale_dB < prev_target_scale_dB) {  //we are in the attack phase, this->slope is getting decreased
        target_scale_dB = this->attack_const * prev_target_scale_dB + one_minus_attack_const * target_scale_dB;
    } else if ((target_scale_dB - prev_target_scale_dB) > 0.01f) {   //or, we are in the release phase, this->slope is getting increased
        target_scale_dB = this->release_const * prev_target_scale_dB + one_minus_release_const * target_scale_dB;
    } else {
        target_scale_dB = prev_target_scale_dB;
    }

    target_scale = db2lin(target_scale_dB);
    if (target_scale > 1.0f) { // prevent from expanding signal
        target_scale = 1.0f;
    }

    return (target_scale);
}

// Sets a new compression curve by transferring structure
void audio_effect_compressor2_set_compression_curve (
        AudioEffectCompressor2* this, compressionCurve* comp_curve)
{
    int n = sizeof(this->outKneeDB) / sizeof(this->outKneeDB[0]);
    this->curve0.marginDB = comp_curve->marginDB;
    this->curve0.offsetDB = comp_curve->offsetDB;
    ia_logi("marginDB: %f, offsetDB %f", this->curve0.marginDB,
            this->curve0.offsetDB);
    for (int kk = 0; kk<n; kk++)
    {
        // Also, adjust the input levels for offsetDB value
        this->curve0.kneeDB[kk] = comp_curve->kneeDB[kk] - this->curve0.offsetDB;
        this->curve0.compressionRatio[kk] = comp_curve->compressionRatio[kk];
        if (this->curve0.kneeDB[kk] > this->curve0.marginDB)
            this->curve0.kneeDB[kk] = this->curve0.marginDB;
        ia_logi("idx %d: kneeDB: %f, compressionRatio %f, updated kneeDB %f",
                kk, this->curve0.kneeDB[kk], this->curve0.compressionRatio[kk],
                this->curve0.kneeDB[kk]);
    }
}

//convert time constants from seconds to unitless parameters
void audio_effect_compressor2_set_attack_release_sec (
        AudioEffectCompressor2* this,
        const float level_atk_sec, const float level_rel_sec,
        const float gain_atk_sec, const float gain_rel_sec)
{
    this->gainAttackSec = gain_atk_sec;
    this->gainReleaseSec = gain_rel_sec;

    // level attack/release
    //this->alpha = (float)(this->levelAttackSec / (1.0f + this->levelAttackSec));
    if (level_atk_sec == 0)
        this->alpha = 0.0f;
    else
        this->alpha = expf(-4.0f / (level_atk_sec * this->sampleRateHz));

    /* this->oneMinusAlpha = 1.0f - this->alpha; */
    ia_logi("alpha : %f", this->alpha);
    //this->beta = (float)(this->levelReleaseSec / (1.0f + this->levelReleaseSec));

    if (level_rel_sec == 0)
        this->beta = 0.0f;
    else
        this->beta = expf(-4.0f / (level_rel_sec * this->sampleRateHz));
    ia_logi("beta : %f", this->beta);


    // gain attack/release
    if (gain_atk_sec == 0)
        this->attack_const = 0.0f;
    else
        this->attack_const = expf(-4.0f / (gain_atk_sec * this->sampleRateHz));
    ia_logi("attack const : %f", this->attack_const);

    if (gain_rel_sec == 0)
        this->release_const = 0.0f;
    else
        this->release_const = expf(-4.0f / (gain_rel_sec * this->sampleRateHz));
    ia_logi("release const : %f", this->release_const);
}

