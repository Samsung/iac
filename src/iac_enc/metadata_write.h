#ifndef _METADATA_WRITE_H
#define _METADATA_WRITE_H
#include "stdint.h"

#define  CHANNEL_LAYOUT_MDHR 9
typedef struct {
  uint32_t dialog_onoff;
  uint32_t dialog_level;
  uint16_t LKFSch[CHANNEL_LAYOUT_MDHR];
  uint16_t dmixgain[CHANNEL_LAYOUT_MDHR];

  uint32_t drc_profile;
  uint32_t chsilence[CHANNEL_LAYOUT_MDHR];
  uint8_t scalablefactor[CHANNEL_LAYOUT_MDHR][12];
  uint32_t len_of_4chauxstrm;
  uint32_t lfe_onoff;
  uint32_t lfe_gain;
  uint32_t len_of_6chauxstrm;
  uint32_t dmix_matrix_type;
  uint32_t weight_type;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t coding_type;
  uint32_t nsamples_of_frame;

}Mdhr;

#endif