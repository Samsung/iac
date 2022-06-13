#ifndef IMMERSIVE_AUDIO_ENCODER_DEFINES_H
#define IMMERSIVE_AUDIO_ENCODER_DEFINES_H
#include <stdint.h>

////////////////////////// Opus Codec /////////////////////////////////////
#define __ia_check_int(x) (((void)((x) == (int32_t)0)), (int32_t)(x))
#define __ia_check_int_ptr(ptr) ((ptr) + ((ptr) - (int32_t*)(ptr)))

#define IA_OK                0
#define IA_MODE_CELT_ONLY          1002
#define IA_BANDWIDTH_FULLBAND              1105 /**<20 kHz bandpass @hideinitializer*/

#define IA_APPLICATION_AUDIO               2049

#define IA_SET_BITRATE_REQUEST             4002
#define IA_SET_BANDWIDTH_REQUEST           4008
#define IA_SET_VBR_REQUEST                 4006
#define IA_SET_COMPLEXITY_REQUEST          4010
#define IA_GET_LOOKAHEAD_REQUEST           4027

#define IA_SET_RECON_GAIN_FLAG_REQUEST         3000
#define IA_SET_SUBSTREAM_SIZE_FLAG_REQUEST         3001
#define IA_SET_OUTPUT_GAIN_FLAG_REQUEST         3002
#define IA_SET_SCALE_FACTOR_MODE_REQUEST         3003
#define IA_SET_TEMP_DOWNMIX_FILE         3004 // need to remove in the future.


#define IA_SET_FORCE_MODE_REQUEST    11002

#define IA_UNIMPLEMENTED -5
#define IA_BAD_ARG -1

#define IA_SET_BITRATE(x) IA_SET_BITRATE_REQUEST, __ia_check_int(x)
#define IA_SET_BANDWIDTH(x) IA_SET_BANDWIDTH_REQUEST, __ia_check_int(x)
#define IA_SET_VBR(x) IA_SET_VBR_REQUEST, __ia_check_int(x)
#define IA_SET_COMPLEXITY(x) IA_SET_COMPLEXITY_REQUEST, __ia_check_int(x)
#define IA_GET_LOOKAHEAD(x) IA_GET_LOOKAHEAD_REQUEST, __ia_check_int_ptr(x)

#define IA_SET_RECON_GAIN_FLAG(x) IA_SET_RECON_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_SUBSTREAM_SIZE_FLAG(x) IA_SET_SUBSTREAM_SIZE_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_OUTPUT_GAIN_FLAG(x) IA_SET_OUTPUT_GAIN_FLAG_REQUEST, __ia_check_int(x)
#define IA_SET_SCALE_FACTOR_MODE(x) IA_SET_SCALE_FACTOR_MODE_REQUEST, __ia_check_int(x)

#define IA_SET_FORCE_MODE(x) IA_SET_FORCE_MODE_REQUEST, __ia_check_int(x)


/////////////////////////////// AAC Codec ////////////////////////////
#endif /* IMMERSIVE_AUDIO_ENCODER_DEFINES_H */
