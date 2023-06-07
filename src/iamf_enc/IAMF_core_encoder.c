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
 * @file IAMF_core_encoder.c
 * @brief The actural codec encoding(opus aac flac Lpcm...)
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "IAMF_encoder.h"
#include "IAMF_encoder_private.h"

#ifdef CONFIG_OPUS_CODEC
/*****************************************************OPUS********************************************/
// opus headers
#include "opus/opus_multistream.h"
#include "opus/opus_projection.h"

#define __OPUS_MODE_CELT_ONLY 1002
#define __OPUS_SET_FORCE_MODE_REQUEST 11002
#define __OPUS_SET_FORCE_MODE(x) \
  __OPUS_SET_FORCE_MODE_REQUEST, __ia_check_int(x)

static int opus_encode_init(AudioElementEncoder *ae) {
  int ret = 0;
  unsigned char def_stream_map[255] = {0, 1};
  for (int i = 0; i < 255; i++) def_stream_map[i] = i;
  int error = 0;
  int bitrate = 0;
  int stream_count = 0, coupled_stream_count = 0;
  int mapping_family = 0;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (j < ae->ia_core_encoder[i].coupled_stream_count) {
        if ((ae->ia_core_encoder[i].dep_encoder[j] =
                 opus_multistream_surround_encoder_create(
                     ae->input_sample_rate,
                     2,                      // channels
                     mapping_family,         // mapping_family
                     &stream_count,          // streams
                     &coupled_stream_count,  // coupled_streams
                     def_stream_map, OPUS_APPLICATION_AUDIO, &error)) == NULL) {
          ia_loge("can not initialize opus encoder.\n");
          exit(-1);
        }
        bitrate = 128000;
      } else {
        if ((ae->ia_core_encoder[i].dep_encoder[j] =
                 opus_multistream_surround_encoder_create(
                     ae->input_sample_rate,
                     1,                      // channels
                     mapping_family,         // mapping_family
                     &stream_count,          // streams
                     &coupled_stream_count,  // coupled_streams
                     def_stream_map, OPUS_APPLICATION_AUDIO, &error)) == NULL) {
          ia_loge("can not initialize opus encoder.\n");
          exit(-1);
        }
        bitrate = 64000;
      }
      // set param

      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                   OPUS_SET_BITRATE(bitrate));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                   OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                   OPUS_SET_VBR(1));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                   OPUS_SET_COMPLEXITY(10));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                   OPUS_GET_LOOKAHEAD(&(ae->initial_padding)));
      opus_multistream_encoder_ctl(
          ae->ia_core_encoder[i].dep_encoder[j],
          __OPUS_SET_FORCE_MODE(__OPUS_MODE_CELT_ONLY));
      ae->padding = ae->initial_padding;
    }
  }
  return 0;
}

static int opus_encode_ctl(AudioElementEncoder *ae, int request, va_list ap) {
  int ret = IAMF_OK;
  switch (request) {
    case IA_SET_BITRATE_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      int32_t bitrate = 0;
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          if (j < ae->ia_core_encoder[i].coupled_stream_count) {
            bitrate = value;
          } else {
            bitrate = value / 2;
          }
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                       OPUS_SET_BITRATE(bitrate));
        }
      }
    } break;
    case IA_SET_BANDWIDTH_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                       IA_SET_BANDWIDTH(value));
        }
      }
    } break;
    case IA_SET_VBR_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                       IA_SET_VBR(value));
        }
      }
    } break;
    case IA_SET_COMPLEXITY_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder[j],
                                       IA_SET_COMPLEXITY(value));
        }
      }
    } break;
    case IA_GET_LOOKAHEAD_REQUEST: {
      int32_t *value = va_arg(ap, int32_t *);
      opus_multistream_encoder_ctl(ae->ia_core_encoder[0].dep_encoder[0],
                                   OPUS_GET_LOOKAHEAD(value));
    } break;
    default:
      ret = IAMF_ERR_UNIMPLEMENTED;
      break;
  }
  return ret;
bad_arg:
  return IAMF_ERR_BAD_ARG;
}

static int opus_encode_frame(AudioElementEncoder *ae, int cg, int stream,
                             int channels, void *pcm_data,
                             unsigned char *encoded_frame) {
  int encoded_size;
  encoded_size = opus_multistream_encode(
      ae->ia_core_encoder[cg].dep_encoder[stream], pcm_data, ae->frame_size,
      encoded_frame, MAX_PACKET_SIZE);
  return encoded_size;
}

static int opus_encode_close(AudioElementEncoder *ae) {
  int ret = 0;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (ae->ia_core_encoder[i].dep_encoder[j]) {
        opus_multistream_encoder_destroy(ae->ia_core_encoder[i].dep_encoder[j]);
        ae->ia_core_encoder[i].dep_encoder[j] = NULL;
      }
    }
  }
  return ret;
}

static int opus_encode_init2(AudioElementEncoder *ae) {
  int ret = 0;
  unsigned char def_stream_map[255] = {0, 1};
  for (int i = 0; i < 255; i++) def_stream_map[i] = i;
  int error = 0;
  int bitrate = 0;
  int stream_count = 0, coupled_stream_count = 0;
  int mapping_family = 0;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (j < ae->ia_core_encoder[i].coupled_stream_count) {
        if ((ae->ia_core_encoder[i].dep_encoder2[j] =
                 opus_multistream_surround_encoder_create(
                     ae->input_sample_rate,
                     2,                      // channels
                     mapping_family,         // mapping_family
                     &stream_count,          // streams
                     &coupled_stream_count,  // coupled_streams
                     def_stream_map, OPUS_APPLICATION_AUDIO, &error)) == NULL) {
          ia_loge("can not initialize opus encoder.\n");
          exit(-1);
        }
        bitrate = 128000;
      } else {
        if ((ae->ia_core_encoder[i].dep_encoder2[j] =
                 opus_multistream_surround_encoder_create(
                     ae->input_sample_rate,
                     1,                      // channels
                     mapping_family,         // mapping_family
                     &stream_count,          // streams
                     &coupled_stream_count,  // coupled_streams
                     def_stream_map, OPUS_APPLICATION_AUDIO, &error)) == NULL) {
          ia_loge("can not initialize opus encoder.\n");
          exit(-1);
        }
        bitrate = 64000;
      }
      // set param

      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                   OPUS_SET_BITRATE(bitrate));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                   OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                   OPUS_SET_VBR(1));
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                   OPUS_SET_COMPLEXITY(10));
      int32_t preskip = 0;
      opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                   OPUS_GET_LOOKAHEAD(&preskip));
      opus_multistream_encoder_ctl(
          ae->ia_core_encoder[i].dep_encoder2[j],
          __OPUS_SET_FORCE_MODE(__OPUS_MODE_CELT_ONLY));
    }
  }
  return 0;
}

static int opus_encode_ctl2(AudioElementEncoder *ae, int request, va_list ap) {
  int ret = IAMF_OK;
  switch (request) {
    case IA_SET_BITRATE_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      int32_t bitrate = 0;
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          if (j < ae->ia_core_encoder[i].coupled_stream_count) {
            bitrate = value;
          } else {
            bitrate = value / 2;
          }
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                       OPUS_SET_BITRATE(bitrate));
        }
      }
    } break;
    case IA_SET_BANDWIDTH_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                       IA_SET_BANDWIDTH(value));
        }
      }
    } break;
    case IA_SET_VBR_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                       IA_SET_VBR(value));
        }
      }
    } break;
    case IA_SET_COMPLEXITY_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          opus_multistream_encoder_ctl(ae->ia_core_encoder[i].dep_encoder2[j],
                                       IA_SET_COMPLEXITY(value));
        }
      }
    } break;
    default:
      ret = IAMF_ERR_UNIMPLEMENTED;
      break;
  }
  return ret;
bad_arg:
  return IAMF_ERR_BAD_ARG;
}

static int opus_encode_frame2(AudioElementEncoder *ae, int cg, int stream,
                              int channels, void *pcm_data,
                              unsigned char *encoded_frame) {
  int encoded_size;
  encoded_size = opus_multistream_encode(
      ae->ia_core_encoder[cg].dep_encoder2[stream], pcm_data, ae->frame_size,
      encoded_frame, MAX_PACKET_SIZE);
  return encoded_size;
}

static int opus_encode_close2(AudioElementEncoder *ae) {
  int ret = 0;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (ae->ia_core_encoder[i].dep_encoder2[j]) {
        opus_multistream_encoder_destroy(
            ae->ia_core_encoder[i].dep_encoder2[j]);
        ae->ia_core_encoder[i].dep_encoder2[j] = NULL;
      }
    }
  }
  return ret;
}

static int opus_decode_init(AudioElementEncoder *ae) {
  int ret = 0;
  int error = 0;
  unsigned char def_stream_map[255] = {
      0,
  };
  for (int i = 0; i < 255; i++) def_stream_map[i] = i;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_decoder[i].stream_count; j++) {
      if (j < ae->ia_core_decoder[i].coupled_stream_count) {
        ae->ia_core_decoder[i].dep_decoder[j] = opus_multistream_decoder_create(
            ae->input_sample_rate, 2, 1, 1, def_stream_map, &error);
        if (error != 0) {
          ia_loge("opus_decoder_create failed %d", error);
        }
      } else {
        ae->ia_core_decoder[i].dep_decoder[j] = opus_multistream_decoder_create(
            ae->input_sample_rate, 1, 1, 0, def_stream_map, &error);
        if (error != 0) {
          ia_loge("opus_decoder_create failed %d", error);
        }
      }
    }
  }
  return ret;
}

static int opus_decode_frame(AudioElementEncoder *ae, int cg, int stream,
                             int channels, unsigned char *encoded_frame,
                             int encoded_size, int16_t *pcm_data) {
  int decoded_size;
  decoded_size = sizeof(int16_t) * channels * ae->frame_size;
  int ret;
  ret = opus_multistream_decode(ae->ia_core_decoder[cg].dep_decoder[stream],
                                encoded_frame, encoded_size,
                                (int16_t *)pcm_data, decoded_size, 0);
  return ret;
}

static int opus_decode_close(AudioElementEncoder *ae) {
  int ret = 0;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_decoder[i].stream_count; j++) {
      opus_multistream_decoder_destroy(ae->ia_core_decoder[i].dep_decoder[j]);
    }
  }
  return ret;
}
#endif

#ifdef CONFIG_AAC_CODEC
/*****************************************************AAC********************************************/
// aac headers
#include "fdk-aac/aacdecoder_lib.h"
#include "fdk-aac/aacenc_lib.h"

#define FF_PROFILE_AAC_LOW 1

static const char *aac_get_error(AACENC_ERROR err) {
  switch (err) {
    case AACENC_OK:
      return "No error";
    case AACENC_INVALID_HANDLE:
      return "Invalid handle";
    case AACENC_MEMORY_ERROR:
      return "Memory allocation error";
    case AACENC_UNSUPPORTED_PARAMETER:
      return "Unsupported parameter";
    case AACENC_INVALID_CONFIG:
      return "Invalid config";
    case AACENC_INIT_ERROR:
      return "Initialization error";
    case AACENC_INIT_AAC_ERROR:
      return "AAC library initialization error";
    case AACENC_INIT_SBR_ERROR:
      return "SBR library initialization error";
    case AACENC_INIT_TP_ERROR:
      return "Transport library initialization error";
    case AACENC_INIT_META_ERROR:
      return "Metadata library initialization error";
    case AACENC_ENCODE_ERROR:
      return "Encoding error";
    case AACENC_ENCODE_EOF:
      return "End of file";
    default:
      return "Unknown error";
  }
}

static int aac_encode_init(AudioElementEncoder *ae) {
  AACENC_ERROR err;
  int ret = 0;
  int aot = FF_PROFILE_AAC_LOW + 1;  // 2: MPEG-4 AAC Low Complexity.
  int sce = 0, cpe = 0;
  int afterburner = 1;
  int signaling = 0;
  int mode = MODE_UNKNOWN;
  int64_t bit_rate = 0;
  int cutoff = 0;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (j < ae->ia_core_encoder[i].coupled_stream_count) {
        sce = 0;
        cpe = 1;
        mode = MODE_2;
        if ((err = aacEncOpen(&(ae->ia_core_encoder[i].dep_encoder[j]), 0,
                              2)) != AACENC_OK) {
          ia_loge("Unable to open the encoder: %s\n", aac_get_error(err));
          goto error;
        }
      } else {
        sce = 1;
        cpe = 0;
        mode = MODE_1;
        if ((err = aacEncOpen(&(ae->ia_core_encoder[i].dep_encoder[j]), 0,
                              1)) != AACENC_OK) {
          ia_loge("Unable to open the encoder: %s\n", aac_get_error(err));
          goto error;
        }
      }
      // set param
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_AOT, aot)) != AACENC_OK) {
        ia_loge("Unable to set the AOT %d: %s\n", aot, aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_SAMPLERATE,
                                     ae->input_sample_rate)) != AACENC_OK) {
        ia_loge("Unable to set the sample rate %d: %s\n", ae->input_sample_rate,
                aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_CHANNELMODE, mode)) != AACENC_OK) {
        ia_loge("Unable to set channel mode %d: %s\n", mode,
                aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_CHANNELORDER, 1)) != AACENC_OK) {
        ia_loge("Unable to set wav channel order %d: %s\n", mode,
                aac_get_error(err));
        goto error;
      }
#if 0
      bit_rate = (96 * sce + 128 * cpe) * ae->input_sample_rate / 44;
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j], AACENC_BITRATE, bit_rate)) != AACENC_OK) {
        printf("Unable to set the bitrate %lld: %s\n", bit_rate, aac_get_error(err));
        goto error;
      }
#else
      int vbr = 4;
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_BITRATEMODE, vbr)) != AACENC_OK) {
        ia_loge("Unable to set the VBR bitrate mode %d: %s\n", vbr,
                aac_get_error(err));
        goto error;
      }
#endif
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_TRANSMUX, TT_MP4_RAW)) !=
          AACENC_OK) {
        ia_loge("Unable to set the transmux format: %s\n", aac_get_error(err));
        goto error;
      }

      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                     AACENC_AFTERBURNER, afterburner)) !=
          AACENC_OK) {
        ia_loge("Unable to set afterburner to %d: %s\n", afterburner,
                aac_get_error(err));
        goto error;
      }

      if ((err = aacEncEncode(ae->ia_core_encoder[i].dep_encoder[j], NULL, NULL,
                              NULL, NULL)) != AACENC_OK) {
        ia_loge("Unable to initialize the encoder: %s\n", aac_get_error(err));
        goto error;
      }
    }
  }
  AACENC_InfoStruct info = {0};
  if ((err = aacEncInfo(ae->ia_core_encoder[0].dep_encoder[0], &info)) !=
      AACENC_OK) {
    ia_loge("Unable to get encoder info: %s\n", aac_get_error(err));
    goto error;
  }
  ae->initial_padding = info.nDelay;
  ae->padding = ae->initial_padding;
  return ret;

error:
  ret = -1;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (ae->ia_core_encoder[i].dep_encoder[j]) {
        aacEncClose(&(ae->ia_core_encoder[i].dep_encoder[j]));
        ae->ia_core_encoder[i].dep_encoder[j] = 0;
      }
    }
  }
  return ret;
}

static int aac_encode_ctl(AudioElementEncoder *ae, int request, va_list ap) {
  int ret = IAMF_OK;
  AACENC_ERROR err;
  switch (request) {
    case IA_SET_BITRATE_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      int32_t bitrate = 0;
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          if (j < ae->ia_core_encoder[i].coupled_stream_count) {
            bitrate = value;
          } else {
            bitrate = value / 2;
          }
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                         AACENC_BITRATE, bitrate)) !=
              AACENC_OK) {
            ia_loge("Unable to set the bitrate %d: %s\n", bitrate,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_SET_BANDWIDTH_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                         AACENC_BANDWIDTH, value)) !=
              AACENC_OK) {
            ia_loge("Unable to set the encoder bandwidth to %d: %s\n", value,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_SET_VBR_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                         AACENC_BITRATEMODE, value)) !=
              AACENC_OK) {
            ia_loge("Unable to set the VBR bitrate mode %d: %s\n", value,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_SET_COMPLEXITY_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder[j],
                                         AACENC_AOT, value)) != AACENC_OK) {
            ia_loge("Unable to set the AOT %d: %s\n", value,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_GET_LOOKAHEAD_REQUEST: {
      int32_t *value = va_arg(ap, int32_t *);
      AACENC_InfoStruct info = {0};
      if ((err = aacEncInfo(ae->ia_core_encoder[0].dep_encoder[0], &info)) !=
          AACENC_OK) {
        ia_loge("Unable to get encoder info: %s\n", aac_get_error(err));
        goto bad_arg;
      }
      *value = info.nDelay;
    } break;
    default:
      ret = IAMF_ERR_UNIMPLEMENTED;
      break;
  }
  return ret;
bad_arg:
  return IAMF_ERR_BAD_ARG;
}

static int aac_encode_frame(AudioElementEncoder *ae, int cg, int stream,
                            int channels, void *pcm_data,
                            unsigned char *encoded_frame) {
  int encoded_size;
  AACENC_ERROR err;

  int in_identifier = IN_AUDIO_DATA;
  int out_identifier = OUT_BITSTREAM_DATA;
  AACENC_BufDesc in_buf = {0}, out_buf = {0};
  AACENC_InArgs in_args = {0};
  AACENC_OutArgs out_args = {0};
  int in_elem_size = sizeof(int16_t);
  int in_size = channels * in_elem_size * ae->frame_size;
  in_args.numInSamples = ae->frame_size * channels;
  in_buf.numBufs = 1;
  in_buf.bufs = &pcm_data;
  in_buf.bufferIdentifiers = &in_identifier;
  in_buf.bufSizes = &in_size;
  in_buf.bufElSizes = &in_elem_size;

  int out_size, out_elem_size;
  out_size = MAX_PACKET_SIZE;
  out_elem_size = 1;
  out_buf.numBufs = 1;
  out_buf.bufs = &encoded_frame;
  out_buf.bufferIdentifiers = &out_identifier;
  out_buf.bufSizes = &out_size;
  out_buf.bufElSizes = &out_elem_size;

  if ((err = aacEncEncode(ae->ia_core_encoder[cg].dep_encoder[stream], &in_buf,
                          &out_buf, &in_args, &out_args)) != AACENC_OK) {
    if (err == AACENC_ENCODE_EOF) {
      ia_loge("Encoding failed\n");
      return -1;
    } else if (!out_args.numOutBytes)
      return 0;
  }
  encoded_size = out_args.numOutBytes;
  return encoded_size;
}

static int aac_encode_close(AudioElementEncoder *ae) {
  AACENC_ERROR err;
  int ret = 0;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      aacEncClose(&(ae->ia_core_encoder[i].dep_encoder[j]));
      ae->ia_core_encoder[i].dep_encoder[j] = 0;
    }
  }
  return ret;
}

static int aac_encode_init2(AudioElementEncoder *ae) {
  AACENC_ERROR err;
  int ret = 0;
  int aot = FF_PROFILE_AAC_LOW + 1;  // 2: MPEG-4 AAC Low Complexity.
  int sce = 0, cpe = 0;
  int afterburner = 1;
  int signaling = 0;
  int mode = MODE_UNKNOWN;
  int64_t bit_rate = 0;
  int cutoff = 0;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (j < ae->ia_core_encoder[i].coupled_stream_count) {
        sce = 0;
        cpe = 1;
        mode = MODE_2;
        if ((err = aacEncOpen(&(ae->ia_core_encoder[i].dep_encoder2[j]), 0,
                              2)) != AACENC_OK) {
          ia_loge("Unable to open the encoder: %s\n", aac_get_error(err));
          goto error;
        }
      } else {
        sce = 1;
        cpe = 0;
        mode = MODE_1;
        if ((err = aacEncOpen(&(ae->ia_core_encoder[i].dep_encoder2[j]), 0,
                              1)) != AACENC_OK) {
          ia_loge("Unable to open the encoder: %s\n", aac_get_error(err));
          goto error;
        }
      }
      // set param
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_AOT, aot)) != AACENC_OK) {
        ia_loge("Unable to set the AOT %d: %s\n", aot, aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_SAMPLERATE,
                                     ae->input_sample_rate)) != AACENC_OK) {
        ia_loge("Unable to set the sample rate %d: %s\n", ae->input_sample_rate,
                aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_CHANNELMODE, mode)) != AACENC_OK) {
        ia_loge("Unable to set channel mode %d: %s\n", mode,
                aac_get_error(err));
        goto error;
      }
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_CHANNELORDER, 1)) != AACENC_OK) {
        ia_loge("Unable to set wav channel order %d: %s\n", mode,
                aac_get_error(err));
        goto error;
      }
#if 0
      bit_rate = (96 * sce + 128 * cpe) * ae->input_sample_rate / 44;
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j], AACENC_BITRATE, bit_rate)) != AACENC_OK) {
        ia_loge("Unable to set the bitrate %lld: %s\n", bit_rate, aac_get_error(err));
        goto error;
      }
#else
      int vbr = 4;
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_BITRATEMODE, vbr)) != AACENC_OK) {
        ia_loge("Unable to set the VBR bitrate mode %d: %s\n", vbr,
                aac_get_error(err));
        goto error;
      }
#endif
      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_TRANSMUX, TT_MP4_RAW)) !=
          AACENC_OK) {
        ia_loge("Unable to set the transmux format: %s\n", aac_get_error(err));
        goto error;
      }

      if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                     AACENC_AFTERBURNER, afterburner)) !=
          AACENC_OK) {
        ia_loge("Unable to set afterburner to %d: %s\n", afterburner,
                aac_get_error(err));
        goto error;
      }

      if ((err = aacEncEncode(ae->ia_core_encoder[i].dep_encoder2[j], NULL,
                              NULL, NULL, NULL)) != AACENC_OK) {
        ia_loge("Unable to initialize the encoder: %s\n", aac_get_error(err));
        goto error;
      }
    }
  }
  return ret;

error:
  ret = -1;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (ae->ia_core_encoder[i].dep_encoder2[j]) {
        aacEncClose(&(ae->ia_core_encoder[i].dep_encoder2[j]));
        ae->ia_core_encoder[i].dep_encoder2[j] = 0;
      }
    }
  }
  return ret;
}

static int aac_encode_ctl2(AudioElementEncoder *ae, int request, va_list ap) {
  int ret = IAMF_OK;
  AACENC_ERROR err;
  switch (request) {
    case IA_SET_BITRATE_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      int32_t bitrate = 0;
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          if (j < ae->ia_core_encoder[i].coupled_stream_count) {
            bitrate = value;
          } else {
            bitrate = value / 2;
          }
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                         AACENC_BITRATE, bitrate)) !=
              AACENC_OK) {
            ia_loge("Unable to set the bitrate %d: %s\n", bitrate,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_SET_BANDWIDTH_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                         AACENC_BANDWIDTH, value)) !=
              AACENC_OK) {
            ia_loge("Unable to set the encoder bandwidth to %d: %s\n", value,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_SET_VBR_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                         AACENC_BITRATEMODE, value)) !=
              AACENC_OK) {
            ia_loge("Unable to set the VBR bitrate mode %d: %s\n", value,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    case IA_SET_COMPLEXITY_REQUEST: {
      int32_t value = va_arg(ap, int32_t);
      for (int i = 0; i < ae->channel_groups; i++) {
        for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
          // set param
          if ((err = aacEncoder_SetParam(ae->ia_core_encoder[i].dep_encoder2[j],
                                         AACENC_AOT, value)) != AACENC_OK) {
            ia_loge("Unable to set the AOT %d: %s\n", value,
                    aac_get_error(err));
            return -1;
          }
        }
      }
    } break;
    default:
      ret = IAMF_ERR_UNIMPLEMENTED;
      break;
  }
  return ret;
bad_arg:
  return IAMF_ERR_BAD_ARG;
}

static int aac_encode_frame2(AudioElementEncoder *ae, int cg, int stream,
                             int channels, void *pcm_data,
                             unsigned char *encoded_frame) {
  int encoded_size;
  AACENC_ERROR err;

  int in_identifier = IN_AUDIO_DATA;
  int out_identifier = OUT_BITSTREAM_DATA;
  AACENC_BufDesc in_buf = {0}, out_buf = {0};
  AACENC_InArgs in_args = {0};
  AACENC_OutArgs out_args = {0};
  int in_elem_size = sizeof(int16_t);
  int in_size = channels * in_elem_size * ae->frame_size;
  in_args.numInSamples = ae->frame_size * channels;
  in_buf.numBufs = 1;
  in_buf.bufs = &pcm_data;
  in_buf.bufferIdentifiers = &in_identifier;
  in_buf.bufSizes = &in_size;
  in_buf.bufElSizes = &in_elem_size;

  int out_size, out_elem_size;
  out_size = MAX_PACKET_SIZE;
  out_elem_size = 1;
  out_buf.numBufs = 1;
  out_buf.bufs = &encoded_frame;
  out_buf.bufferIdentifiers = &out_identifier;
  out_buf.bufSizes = &out_size;
  out_buf.bufElSizes = &out_elem_size;

  if ((err = aacEncEncode(ae->ia_core_encoder[cg].dep_encoder2[stream], &in_buf,
                          &out_buf, &in_args, &out_args)) != AACENC_OK) {
    if (err == AACENC_ENCODE_EOF) {
      ia_loge("Encoding failed\n");
      return -1;
    } else if (!out_args.numOutBytes)
      return 0;
  }
  encoded_size = out_args.numOutBytes;
  return encoded_size;
}

static int aac_encode_close2(AudioElementEncoder *ae) {
  AACENC_ERROR err;
  int ret = 0;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      aacEncClose(&(ae->ia_core_encoder[i].dep_encoder2[j]));
      ae->ia_core_encoder[i].dep_encoder2[j] = 0;
    }
  }
  return ret;
}

static int aac_decode_init(AudioElementEncoder *ae) {
  int ret = 0;
  AAC_DECODER_ERROR err;
  /*
  object type; f(5)
  frequency index; f(4)
  channel configuration; f(4)
  GASpecificConfig{
  frameLengthFlag; f(1)
  dependsOnCoreCoder; f(1)
  extensionFlag; f(1)
  }
  */
  // UCHAR extra_data_s[10] = { 0x11,0x88 }; //single stream
  // UCHAR extra_data_c[10] = { 0x11,0x90 };; //coupled stream
  UINT extra_data_size = 2;
  UCHAR *extra_data_s = (UCHAR *)malloc(extra_data_size * sizeof(extra_data_s));
  UCHAR *extra_data_c = (UCHAR *)malloc(extra_data_size * sizeof(extra_data_s));
  extra_data_s[0] = 0x11;
  extra_data_s[1] = 0x88;
  extra_data_c[0] = 0x11;
  extra_data_c[1] = 0x90;
  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_decoder[i].stream_count; j++) {
      ae->ia_core_decoder[i].dep_decoder[j] = aacDecoder_Open(TT_MP4_RAW, 1);
      if (aacDecoder_SetParam(ae->ia_core_decoder[i].dep_decoder[j],
                              AAC_CONCEAL_METHOD, 1) != AAC_DEC_OK) {
        ia_loge("Unable to set error concealment method\n");
        return -1;
      }
#if 1
      if (!ae->ia_core_decoder[i].dep_decoder[j]) {
        ia_loge("Error opening decoder\n");
      }
      if (j < ae->ia_core_decoder[i].coupled_stream_count) {
        if ((err = aacDecoder_ConfigRaw(ae->ia_core_decoder[i].dep_decoder[j],
                                        &extra_data_c, &extra_data_size)) !=
            AAC_DEC_OK) {
          ia_loge("Unable to set extradata\n");
          return -1;
        }
      } else {
        if ((err = aacDecoder_ConfigRaw(ae->ia_core_decoder[i].dep_decoder[j],
                                        &extra_data_s, &extra_data_size)) !=
            AAC_DEC_OK) {
          ia_loge("Unable to set extradata\n");
          return -1;
        }
      }
#endif
    }
  }
  if (extra_data_s) free(extra_data_s);
  if (extra_data_c) free(extra_data_c);
  return ret;
}

static int aac_decode_frame(AudioElementEncoder *ae, int cg, int stream,
                            int channels, unsigned char *encoded_frame,
                            int encoded_size, int16_t *pcm_data) {
  int decoded_size;
  decoded_size = sizeof(int16_t) * channels * ae->frame_size;
  int ret;
  AAC_DECODER_ERROR err;
  UINT valid = encoded_size;
  err = aacDecoder_Fill(ae->ia_core_decoder[cg].dep_decoder[stream],
                        &encoded_frame, &encoded_size, &valid);
  if (err != AAC_DEC_OK) {
    ia_loge("aacDecoder_Fill() failed: %x\n", err);
    return -1;
  }
  err = aacDecoder_DecodeFrame(ae->ia_core_decoder[cg].dep_decoder[stream],
                               (INT_PCM *)pcm_data, decoded_size, 0);
  if (err != AAC_DEC_OK) {
    ia_loge("aacDecoder_DecodeFrame() failed: %x\n", err);
    return -1;
  }
  ret = decoded_size;
  return ret;
}

static int aac_decode_close(AudioElementEncoder *ae) {
  int ret = 0;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_decoder[i].stream_count; j++) {
      if (ae->ia_core_decoder[i].dep_decoder[j])
        aacDecoder_Close(ae->ia_core_decoder[i].dep_decoder[j]);
    }
  }
  return ret;
}
#endif

/*****************************************************LPCM********************************************/

static int pcm_encode_init(AudioElementEncoder *ae) { return 0; }

static int pcm_encode_ctl(AudioElementEncoder *ae, int request, va_list ap) {
  return 0;
}

static int pcm_encode_frame(AudioElementEncoder *ae, int cg, int stream,
                            int channels, void *pcm_data,
                            unsigned char *encoded_frame) {
  int in_elem_size = ae->bits_per_sample / 8;
  int in_size = channels * in_elem_size * ae->frame_size;
  memcpy(encoded_frame, (unsigned char *)pcm_data, in_size);
  return in_size;
}

static int pcm_encode_close(AudioElementEncoder *ae) { return 0; }

static int pcm_encode_init2(AudioElementEncoder *ae) { return 0; }

static int pcm_encode_ctl2(AudioElementEncoder *ae, int request, va_list ap) {
  return 0;
}

static int pcm_encode_frame2(AudioElementEncoder *ae, int cg, int stream,
                             int channels, void *pcm_data,
                             unsigned char *encoded_frame) {
  int in_elem_size = ae->bits_per_sample / 8;
  int in_size = channels * in_elem_size * ae->frame_size;
  memcpy(encoded_frame, (unsigned char *)pcm_data, in_size);
  return in_size;
}

static int pcm_encode_close2(AudioElementEncoder *ae) { return 0; }

static int pcm_decode_init(AudioElementEncoder *ae) { return 0; }

static int pcm_decode_frame(AudioElementEncoder *ae, int cg, int stream,
                            int channels, unsigned char *encoded_frame,
                            int encoded_size, int16_t *pcm_data) {
  int decoded_size;
  decoded_size = sizeof(int16_t) * channels * ae->frame_size;
  memcpy((unsigned char *)pcm_data, encoded_frame, decoded_size);
  return decoded_size;
}

static int pcm_decode_close(AudioElementEncoder *ae) { return 0; }

/*****************************************************FLAC********************************************/
// Flac headers
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"

static FLAC__StreamEncoderWriteStatus flac_encoder_progress_callback(
    const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes,
    uint32_t samples, uint32_t current_frame, void *client_data) {
  (void)encoder, (void)samples, (void)current_frame;
  // fprintf(stderr, "Encoded buffer: %x, bytes: %d, samples: %d, current_frame:
  // %d  \n", buffer[0], bytes, samples, current_frame);
  if (((IAPacket *)client_data)->data) {
    memcpy(((IAPacket *)client_data)->data, buffer, bytes);
    ((IAPacket *)client_data)->packet_size = bytes;
  }
  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderMetadataCallback flac_encoder_metadata_callback(
    const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata,
    void *client_data) {
  (void)encoder, (void)metadata, (void)client_data;
  // fprintf(stderr, "metadata type: %d\n", metadata->type);
}

static int flac_encode_init(AudioElementEncoder *ae) {
  int ret = 0;
  unsigned char def_stream_map[255] = {0, 1};
  for (int i = 0; i < 255; i++) def_stream_map[i] = i;
  int error = 0;
  int stream_count = 0, coupled_stream_count = 0;
  int mapping_family = 0;
  int channels = 1;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      FLAC__bool ok = 1;
      FLAC__StreamEncoderInitStatus init_status = {0};
      FLAC__StreamMetadata_VorbisComment_Entry entry;
      if (j < ae->ia_core_encoder[i].coupled_stream_count) {
        if ((ae->ia_core_encoder[i].dep_encoder[j] =
                 FLAC__stream_encoder_new()) == NULL) {
          ia_loge("can not initialize flac encoder.\n");
          exit(-1);
        }
        channels = 2;
      } else {
        if ((ae->ia_core_encoder[i].dep_encoder[j] =
                 FLAC__stream_encoder_new()) == NULL) {
          ia_loge("can not initialize flac encoder.\n");
          exit(-1);
        }
        channels = 1;
      }
      // set param

      ok &= FLAC__stream_encoder_set_verify(
          ae->ia_core_encoder[i].dep_encoder[j], true);
      ok &= FLAC__stream_encoder_set_compression_level(
          ae->ia_core_encoder[i].dep_encoder[j], 5);
      ok &= FLAC__stream_encoder_set_channels(
          ae->ia_core_encoder[i].dep_encoder[j], channels);
      ok &= FLAC__stream_encoder_set_bits_per_sample(
          ae->ia_core_encoder[i].dep_encoder[j], ae->bits_per_sample);
      ok &= FLAC__stream_encoder_set_sample_rate(
          ae->ia_core_encoder[i].dep_encoder[j], ae->input_sample_rate);
      ok &= FLAC__stream_encoder_set_blocksize(
          ae->ia_core_encoder[i].dep_encoder[j], ae->frame_size);

      if (!ok) {
        ia_loge("flac setting failed.\n");
        return -1;
      }

      /* initialize encoder */
      if (ok) {
        // init_status = FLAC__stream_encoder_init_file(encoder, argv[2],
        // flac_encoder_progress_callback, /*client_data=*/NULL);
        init_status = FLAC__stream_encoder_init_stream(
            ae->ia_core_encoder[i].dep_encoder[j],
            flac_encoder_progress_callback, 0, 0,
            flac_encoder_metadata_callback,
            &(ae->ia_core_encoder[i].ia_packet[j]));
        if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
          ia_loge("ERROR: initializing encoder: %s\n",
                  FLAC__StreamEncoderInitStatusString[init_status]);
          ok = false;
        }
      }
    }
  }
  ae->initial_padding = ae->frame_size;  // flac first encoded frame is output
                                         // after inputing second frame.
  ae->padding = 0;
  return 0;
}

static int flac_encode_ctl(AudioElementEncoder *ae, int request, va_list ap) {
  return 0;
}

static int flac_encode_frame(AudioElementEncoder *ae, int cg, int stream,
                             int channels, void *pcm_data,
                             unsigned char *encoded_frame) {
  FLAC__bool ok = true;
  unsigned char *buffer = (unsigned char *)pcm_data;
  ae->ia_core_encoder[cg].ia_packet[stream].data = encoded_frame;
  FLAC__int32 *pcm =
      (FLAC__int32 *)malloc(ae->frame_size * 2 * sizeof(FLAC__int32));
  for (int i = 0; i < ae->frame_size * channels; i++) {
    /* inefficient but simple and works on big- or little-endian machines */
    pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)buffer[2 * i + 1] << 8) |
                           (FLAC__int16)buffer[2 * i]);
  }

  ok = FLAC__stream_encoder_process_interleaved(
      ae->ia_core_encoder[cg].dep_encoder[stream], pcm, ae->frame_size);
  if (pcm) {
    free(pcm);
    pcm = NULL;
  }
  ae->ia_core_encoder[cg].ia_packet[stream].data =
      NULL;  // reset the data pointer
  return ae->ia_core_encoder[cg].ia_packet[stream].packet_size;
}

static int flac_encode_close(AudioElementEncoder *ae) {
  FLAC__bool ok = true;

  for (int i = 0; i < ae->channel_groups; i++) {
    for (int j = 0; j < ae->ia_core_encoder[i].stream_count; j++) {
      if (ae->ia_core_encoder[i].dep_encoder[j]) {
        ok &=
            FLAC__stream_encoder_finish(ae->ia_core_encoder[i].dep_encoder[j]);
        FLAC__stream_encoder_delete(ae->ia_core_encoder[i].dep_encoder[j]);
        ae->ia_core_encoder[i].dep_encoder[j] = NULL;
      }
    }
  }
  return ok;
}

static int flac_encode_init2(AudioElementEncoder *ae) { return 0; }

static int flac_encode_ctl2(AudioElementEncoder *ae, int request, va_list ap) {
  return 0;
}

static int flac_encode_frame2(AudioElementEncoder *ae, int cg, int stream,
                              int channels, void *pcm_data,
                              unsigned char *encoded_frame) {
  return 0;
}

static int flac_encode_close2(AudioElementEncoder *ae) { return 0; }

static int flac_decode_init(AudioElementEncoder *ae) { return 0; }

static int flac_decode_frame(AudioElementEncoder *ae, int cg, int stream,
                             int channels, unsigned char *encoded_frame,
                             int encoded_size, int16_t *pcm_data) {
  return 0;
}

static int flac_decode_close(AudioElementEncoder *ae) { return 0; }

encode_creator_t dep_encoders[] = {
#ifdef CONFIG_OPUS_CODEC
    {IAMF_CODEC_OPUS, opus_encode_init, opus_encode_ctl, opus_encode_frame,
     opus_encode_close},
#endif
#ifdef CONFIG_AAC_CODEC
    {IAMF_CODEC_AAC, aac_encode_init, aac_encode_ctl, aac_encode_frame,
     aac_encode_close},
#endif
#ifdef CONFIG_FLAC_CODEC
    {IAMF_CODEC_FLAC, flac_encode_init, flac_encode_ctl, flac_encode_frame,
     flac_encode_close},
#endif
    {IAMF_CODEC_PCM, pcm_encode_init, pcm_encode_ctl, pcm_encode_frame,
     pcm_encode_close},
    {-1, NULL, NULL}};

encode_creator_t dep_encoders2[] = {
#ifdef CONFIG_OPUS_CODEC
    {IAMF_CODEC_OPUS, opus_encode_init2, opus_encode_ctl2, opus_encode_frame2,
     opus_encode_close2},
#endif
#ifdef CONFIG_AAC_CODEC
    {IAMF_CODEC_AAC, aac_encode_init2, aac_encode_ctl2, aac_encode_frame2,
     aac_encode_close2},
#endif
#ifdef CONFIG_FLAC_CODEC
    {IAMF_CODEC_FLAC, flac_encode_init2, flac_encode_ctl2, flac_encode_frame2,
     flac_encode_close2},
#endif
    {IAMF_CODEC_PCM, pcm_encode_init2, pcm_encode_ctl2, pcm_encode_frame2,
     pcm_encode_close2},
    {-1, NULL, NULL}};

decode_creator_t dep_decoders[] = {
#ifdef CONFIG_OPUS_CODEC
    {IAMF_CODEC_OPUS, opus_decode_init, opus_decode_frame, opus_decode_close},
#endif
#ifdef CONFIG_AAC_CODEC
    {IAMF_CODEC_AAC, aac_decode_init, aac_decode_frame, aac_decode_close},
#endif
#ifdef CONFIG_FLAC_CODEC
    {IAMF_CODEC_FLAC, flac_decode_init, flac_decode_frame, flac_decode_close},
#endif
    {IAMF_CODEC_PCM, pcm_decode_init, pcm_decode_frame, pcm_decode_close},
    {-1, NULL, NULL}};