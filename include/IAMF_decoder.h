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
 * @file IAMF_decoder.h
 * @brief IAMF decoder APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DECODER_H
#define IAMF_DECODER_H

#include <stdint.h>

#include "IAMF_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@}*/
/**\name Immersive audio decoder functions */
/**@{*/

typedef struct IAMF_Decoder *IAMF_DecoderHandle;

typedef struct {
  int count;
  char **labels;
} IAMF_Labels;

/**
 * @brief     Open an iamf decoder.
 * @return    return an iamf decoder handle.
 */
IAMF_DecoderHandle IAMF_decoder_open(void);

/**
 * @brief     Close an iamf decoder.
 * @param     [in] handle : iamf decoder handle.
 */
int IAMF_decoder_close(IAMF_DecoderHandle handle);

/**
 * @brief     Configurate an iamf decoder.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] data : the bitstream.
 * @param     [in] size : the size in bytes of bitstream.
 * @param     [out] rsize : the size in bytes of bitstream that has been
 * consumed.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_configure(IAMF_DecoderHandle handle, const uint8_t *data,
                           uint32_t size, uint32_t *rsize);

/**
 * @brief     Decode bitstream.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] data : the bitstream.
 * @param     [in] size : the size in bytes of bitstream.
 * @param     [out] rsize : the size in bytes of bitstream that has been
 * consumed.
 * @param     [out] pcm : output signal.
 * @return    the number of decoded samples or @ref IAErrCode.
 */
int IAMF_decoder_decode(IAMF_DecoderHandle handle, const uint8_t *data,
                        int32_t size, uint32_t *rsize, void *pcm);

/**
 * @brief     Get mix presentation labels.
 * @param     [in] handle : iamf decoder handle.
 * @return    @ref IAMF_Labels or 0.
 */
IAMF_Labels *IAMF_decoder_get_mix_presentation_labels(
    IAMF_DecoderHandle handle);

/**
 * @brief     Set a mix presentation label.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] label : a human-friendly label (@ref
 * IAMF_decoder_get_mix_presentation_labels) to describe mix presentation.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_mix_presentation_label(IAMF_DecoderHandle handle,
                                            const char *label);
/**
 * @brief     Set sound system output layout.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] ss : the sound system (@ref IAMF_SoundSystem).
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_output_layout_set_sound_system(IAMF_DecoderHandle handle,
                                                IAMF_SoundSystem ss);

/**
 * @brief     Set binaural output layout.
 * @param     [in] handle : iamf decoder handle.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_output_layout_set_binaural(IAMF_DecoderHandle handle);

/**
 * @brief     Get the number of channels of the sound system.
 * @param     [in] ss : the sound system (@ref IAMF_SoundSystem).
 * @return    the number of channels.
 */
int IAMF_layout_sound_system_channels_count(IAMF_SoundSystem ss);

/**
 * @brief     Get the number of channels of binaural pattern.
 * @return    the number of channels.
 */
int IAMF_layout_binaural_channels_count();

/**
 * @brief     Get the codec capability of iamf.Need to free string manually.
 * @return    the supported codec string.
 */
char *IAMF_decoder_get_codec_capability();

/**
 * @brief     Set peak threshold value to limiter.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] db : peak threshold in dB.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_peak_limiter_set_threshold(IAMF_DecoderHandle handle,
                                            float db);

/**
 * @brief     Get peak threshold value.
 * @param     [in] handle : iamf decoder handle.
 * @return    Peak threshold in dB.
 */
float IAMF_decoder_peak_limiter_get_threshold(IAMF_DecoderHandle handle);

// only for tizen

typedef struct IAMF_Param {
  int parameter_length;
  uint32_t parameter_definition_type;
  union {
    uint32_t dmixp_mode;
  };
} IAMF_Param;

typedef enum IAMF_SoundMode {
  IAMF_SOUND_MODE_NONE = -2,
  IAMF_SOUND_MODE_NA = -1,
  IAMF_SOUND_MODE_STEREO,
  IAMF_SOUND_MODE_MULTICHANNEL,
  IAMF_SOUND_MODE_BINAURAL
} IAMF_SoundMode;

typedef struct IAMF_extradata {
  IAMF_SoundSystem output_sound_system;
  uint32_t number_of_samples;
  uint32_t bitdepth;
  uint32_t sampling_rate;
  IAMF_SoundMode output_sound_mode;

  int num_loudness_layouts;
  IAMF_Layout *loudness_layout;
  IAMF_LoudnessInfo *loudness;

  uint32_t num_parameters;
  IAMF_Param *param;
} IAMF_extradata;

int IAMF_decoder_set_pts(IAMF_DecoderHandle handle, uint32_t pts,
                         uint32_t time_base);
int IAMF_decoder_get_last_metadata(IAMF_DecoderHandle handle, uint32_t *pts,
                                   IAMF_extradata *metadata);
#ifdef __cplusplus
}
#endif

#endif /* IAMF_DECODER_H */
