#ifndef OBU_WRITE_H_
#define OBU_WRITE_H_

#include <stdint.h>

/*!\brief OBU types. */
typedef enum ATTRIBUTE_PACKED {
  OBU_CODEC_SPECIFIC_INFO = 1,
  OBU_IA_STATIC_META = 2,
  OBU_IA_TIMED_META = 3,
  OBU_IA_CODED_DATA = 4,
} AUDIO_OBU_TYPE;

/*!\brief Algorithm return codes */
typedef enum {
  /*!\brief Operation completed without error */
  CODEC_OK,

  /*!\brief Unspecified error */
  CODEC_ERROR,

} codec_err_t;

int uleb_encode(uint64_t value, uint32_t available, uint8_t *coded_value,
  uint32_t *coded_size);
uint32_t opus_write_obu_header(AUDIO_OBU_TYPE obu_type,
  int obu_extension, uint8_t *const dst);
uint32_t opus_write_metadata_obu(uint8_t *const *metadata, int playload_size,
  uint8_t *const dst);
uint32_t opus_write_codeddata_obu(const uint8_t *const sampledata, int playload_size,
  uint8_t *const dst);
#endif
