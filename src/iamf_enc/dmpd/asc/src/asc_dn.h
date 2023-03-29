/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file asc_dn.h
 * @brief audio scene classification
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef ASC_DN_H
#define ASC_DN_H

float* asc_preprocess(float* input, int size);
float * asc_log_mstft_transform(void * asc_estimator_feature_, float* input, int channel, float * mscale_matrix, int nfft, int nhop);
void inference_asc_estimator(void * asc_estimator_feature_, float * input,float* fc_out, float* softmax_out, int* argmax_out);
void inference_asc_feature(void * asc_estimator_feature_, float* input,float* fout);
int inference_asc_create(void **inference);
int inference_asc_destroy(void* inference);


#endif 