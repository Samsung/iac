#ifndef _AUDIO_EFFECT_COMPRESSOR_H_
#define _AUDIO_EFFECT_COMPRESSOR_H_

typedef struct AudioEffectCompressor AudioEffectCompressor;

AudioEffectCompressor* audio_effect_compressor_create(void);
void audio_effect_compressor_destroy(AudioEffectCompressor*);

int audio_effect_compressor_set_default_compression_curve(AudioEffectCompressor*);
int audio_effect_compressor_init(AudioEffectCompressor*, int sample_rate, 
        int num_channels, float level_attack_sec, float level_release_sec,
        float gain_attack_sec, float gain_release_sec, int delay_size);

int audio_effect_compressor_process_block(AudioEffectCompressor*,
        float *inblock, float *outblock, int frame_size);
#endif
