#ifndef IAMF_ASC_H
#define IAMF_ASC_H
#include <stdint.h>
#include "queue_plus.h"

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
  float * data_fs;
  uint32_t shift;
  uint32_t frames;
  uint32_t max_frames;
  int sample_rate;
  int layout;
  uint32_t frame_size;
  void *fp;
  QueuePlus *pq;
  void *resampler;
  uint32_t num;
  uint32_t den;
}IAMF_ASC;

IAMF_ASC * iamf_asc_start(int layout, int frame_size, int sample_rate, QueuePlus *pq, char* out_file);
int iamf_asc_process(IAMF_ASC *asc, int16_t *input, int size);
int iamf_asc_stop(IAMF_ASC *asc);
#endif /* IAMF_ASC_H */
