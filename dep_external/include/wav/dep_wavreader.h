/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#ifndef DEP_WAVREADER_H
#define DEP_WAVREADER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEP_WAVE_FORMAT_PCM
#define DEP_WAVE_FORMAT_PCM 1
#define DEP_WAVE_FORMAT_FLOAT 3
#endif

void* dep_wav_read_open(const char* filename);
void dep_wav_read_close(void* obj);
int dep_wav_get_header(void* obj, int* format, int* channels, int* sample_rate,
                   int* bits_per_sample, int* endianness,
                   unsigned int* data_length);
int dep_wav_read_data(void* obj, unsigned char* data, unsigned int length);

#ifdef __cplusplus
}
#endif

#endif