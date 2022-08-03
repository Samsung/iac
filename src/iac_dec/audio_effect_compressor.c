#include "audio_defines.h"
#include "audio_effect_compressor.h"
#include "audio_effect_compressor2.h"
#include "immersive_audio_utils.h"

#define DRC_MarginDB            -1.0f
#define DRC_OffsetDB            0.0f
#define DRC_Knee0               -16.5f
#define DRC_Knee1               -9.0f
#define DRC_Knee2               -6.0f
#define DRC_Knee3               0.0f
#define DRC_Knee4               0.0f
#define DRC_Ratio0              1.5f
#define DRC_Ratio1              2.0f
#define DRC_Ratio2              3.0f
#define DRC_Ratio3              10.0f
#define DRC_Ratio4              10.0f

struct AudioEffectCompressor
{
    compressionCurve curve0;
    AudioEffectCompressor2 comp[MAX_CHANNELS];
    int sampleRate;
    int numChannels;
};


AudioEffectCompressor* audio_effect_compressor_create (void)
{
    return (AudioEffectCompressor *) ia_mallocz (sizeof(AudioEffectCompressor));
}

void audio_effect_compressor_destroy (AudioEffectCompressor* this)
{
    if (this) free(this);
}

int audio_effect_compressor_set_default_compression_curve (AudioEffectCompressor* this)
{
    if (!this)
        return IA_ERR_INVALID_STATE;

    static compressionCurve default_curve = {
        DRC_MarginDB, DRC_OffsetDB,
        { DRC_Knee0, DRC_Knee1, DRC_Knee2, DRC_Knee3, DRC_Knee4 },
        { DRC_Ratio0, DRC_Ratio1, DRC_Ratio2, DRC_Ratio3, DRC_Ratio4 }
    };
    memcpy (&this->curve0, &default_curve, sizeof(compressionCurve));
    return IA_OK;
}

int audio_effect_compressor_init (AudioEffectCompressor* this,
        int sample_rate, int num_channels,
        float level_attack_sec, float level_release_sec,
        float gain_attack_sec, float gain_release_sec, int delay_size)
{
    this->sampleRate = sample_rate;
    this->numChannels = num_channels;

    for (int ch = 0; ch < num_channels; ch++) {
        audio_effect_compressor2_set_compression_curve (&this->comp[ch], &this->curve0);
        audio_effect_compressor2_init(&this->comp[ch], this->sampleRate, delay_size);
        audio_effect_compressor2_set_attack_release_sec(&this->comp[ch],
                level_attack_sec, level_release_sec, gain_attack_sec, gain_release_sec);
    }

    return IA_OK;
}

int audio_effect_compressor_process_block(AudioEffectCompressor* this,
        float *inblock, float *outblock, int frame_size)
{
    for (int ch = 0; ch < this->numChannels; ch++)
    {
        audio_effect_compressor2_process_block(&this->comp[ch],
                                               inblock + ch*frame_size,
                                               outblock + ch*frame_size,
                                               frame_size);
    }
    return (frame_size);
}
