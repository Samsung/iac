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
  OBU_IA_STREAM_INDICATOR = 0,
  OBU_CODEC_SPECIFIC_INFO = 1,
  OBU_IA_STATIC_META = 2,
  OBU_TEMPORAL_DELIMITOR = 3,
  OBU_DEMIXING_INFO = 4,
  OBU_RECON_GAIN_INFO = 5,
  OBU_SUBSTREAM = 6,
  RESERVED,
} AUDIO_OBU_TYPE;
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
uint32_t iac_write_obu_header(AUDIO_OBU_TYPE obu_type,
  int obu_extension, uint8_t *const dst);
uint32_t iac_write_obu_unit(const uint8_t * sampledata, int playload_size,
  uint8_t *const dst, AUDIO_OBU_TYPE obu_type);

#ifdef __cplusplus
}
#endif

#endif //OBU_WRITE_H_
