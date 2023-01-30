#ifndef WAVWRITER_H
#define WAVWRITER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_FLOAT 3
#endif

void *wav_write_open(const char *filename, int sample_rate, int bits_per_sample,
                     int channels);
void *wav_write_open2(const char *filename, int format, int sample_rate,
                      int bits_per_sample, int channels);
void wav_write_close(void *obj);
void wav_write_data(void *obj, const unsigned char *data, int length);

#ifdef __cplusplus
}
#endif

#endif

