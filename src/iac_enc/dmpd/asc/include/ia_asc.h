#ifndef IA_ASC_H
#define IA_ASC_H
#include <stdint.h>

typedef enum {
  ASC_CHANNEL_LAYOUT_100, //1.0.0
  ASC_CHANNEL_LAYOUT_200, //2.0.0 
  ASC_CHANNEL_LAYOUT_510, //5.1.0
  ASC_CHANNEL_LAYOUT_512, //5.1.2
  ASC_CHANNEL_LAYOUT_514, //5.1.4
  ASC_CHANNEL_LAYOUT_710, //7.1.0
  ASC_CHANNEL_LAYOUT_712, //7.1.2
  ASC_CHANNEL_LAYOUT_714, //7.1.4
  ASC_CHANNEL_LAYOUT_312, //3.1.2
  ASC_CHANNEL_LAYOUT_MAX
}ASC_CHANNEL_LAYOUT;

typedef struct {
  void *asc_estimator_feature;
  int start;
  float * data_fs;
  uint32_t ents;
  uint32_t max_ents;
  int layout;
  void *fp;
}IA_ASC;

IA_ASC * ia_asc_start(int layout);
int frame_based_process(IA_ASC *asc);
int ia_asc_stop(IA_ASC *asc);
#endif /* IA_ASC_H */
