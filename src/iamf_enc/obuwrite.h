#ifndef OBU_WRITE_H_
#define OBU_WRITE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*!\brief OBU types. */
#if 0
typedef enum ATTRIBUTE_PACKED {
  OBU_CODEC_SPECIFIC_INFO = 1,
  OBU_IA_STATIC_META = 2,
  OBU_IA_TIMED_META = 3,
  OBU_IA_CODED_DATA = 4,
} AUDIO_OBU_TYPE;
#else
typedef enum ATTRIBUTE_PACKED {
  OBU_IA_Invalid = -1,
  OBU_IA_Codec_Config = 0,
  OBU_IA_Audio_Element = 1,
  OBU_IA_Mix_Presentation = 2,
  OBU_IA_Parameter_Block = 3,
  OBU_IA_Temporal_Delimiter = 4,
  OBU_IA_Sync = 5,
  OBU_IA_Audio_Frame = 8,
  OBU_IA_Audio_Frame_ID0 = 9,
  OBU_IA_Audio_Frame_ID1 = 10,
  OBU_IA_Audio_Frame_ID2 = 11,
  OBU_IA_Audio_Frame_ID3 = 12,
  OBU_IA_Audio_Frame_ID4 = 13,
  OBU_IA_Audio_Frame_ID5 = 14,
  OBU_IA_Audio_Frame_ID6 = 15,
  OBU_IA_Audio_Frame_ID7 = 16,
  OBU_IA_Audio_Frame_ID8 = 17,
  OBU_IA_Audio_Frame_ID9 = 18,
  OBU_IA_Audio_Frame_ID10 = 19,
  OBU_IA_Audio_Frame_ID11 = 20,
  OBU_IA_Audio_Frame_ID12 = 21,
  OBU_IA_Audio_Frame_ID13 = 22,
  OBU_IA_Audio_Frame_ID14 = 23,
  OBU_IA_Audio_Frame_ID15 = 24,
  OBU_IA_Audio_Frame_ID16 = 25,
  OBU_IA_Audio_Frame_ID17 = 26,
  OBU_IA_Audio_Frame_ID18 = 27,
  OBU_IA_Audio_Frame_ID19 = 28,
  OBU_IA_Audio_Frame_ID20 = 29,
  OBU_IA_Audio_Frame_ID21 = 30,
  OBU_IA_Magic_Code = 31

} AUDIO_OBU_TYPE;

// Just for testing
typedef enum ATTRIBUTE_PACKED1 {
  OBU_IA_STREAM_INDICATOR = 0,
  OBU_CODEC_SPECIFIC_INFO = 1,
  OBU_IA_STATIC_META = 2,
  OBU_TEMPORAL_DELIMITOR = 3,
  OBU_DEMIXING_INFO = 4,
  OBU_RECON_GAIN_INFO = 5,
  OBU_SUBSTREAM = 6,
} AUDIO_OBU_TYPE1;
#endif

/*!\brief Algorithm return codes */
typedef enum {
  /*!\brief Operation completed without error */
  CODEC_OK,

  /*!\brief Unspecified error */
  CODEC_ERROR,

} codec_err_t;

int uleb_encode(uint64_t value, uint32_t available, uint8_t *coded_value,
  uint32_t *coded_size);
uint32_t iamf_write_obu_unit(const uint8_t * sampledata, int playload_size,
  AUDIO_OBU_TYPE obu_type, int obu_redundant_copy,
  int obu_trimming_status, int num_samples_to_trim_at_start, int num_samples_to_trim_at_end,
  int obu_extension, uint8_t *const dst);
#ifdef __cplusplus
}
#endif

#endif //OBU_WRITE_H_
