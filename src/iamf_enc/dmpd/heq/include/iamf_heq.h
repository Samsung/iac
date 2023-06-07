#ifndef IAMF_HEQ_H
#define IAMF_HEQ_H
#include "queue_plus.h"

typedef enum {
  HEQ_CHANNEL_LAYOUT_100, //1.0.0
  HEQ_CHANNEL_LAYOUT_200, //2.0.0 
  HEQ_CHANNEL_LAYOUT_510, //5.1.0
  HEQ_CHANNEL_LAYOUT_512, //5.1.2
  HEQ_CHANNEL_LAYOUT_514, //5.1.4
  HEQ_CHANNEL_LAYOUT_710, //7.1.0
  HEQ_CHANNEL_LAYOUT_712, //7.1.2
  HEQ_CHANNEL_LAYOUT_714, //7.1.4
  HEQ_CHANNEL_LAYOUT_312, //3.1.2
  HEQ_CHANNEL_LAYOUT_MAX
}HEQ_CHANNEL_LAYOUT;

typedef struct{
  int index;
  double dspOutBuf_rmse_hgt_short;
  double dspOutBuf_rmse_total_long;
  double dspOutBuf_rmse_srd_long;
  double dspOutBuf_rmse_total_short;
}DHE2;

typedef struct {
  double ThreT;
  double ThreS;
  double ThreM;
}THRESHOLD;

typedef struct {
  DHE2 dhe;
  THRESHOLD threshold;
  int layout;
  int frame_rate;
  int fcnt;
  double Wlevel;
  void *fp;
  QueuePlus *pq;
}IAMF_HEQ;

IAMF_HEQ *iamf_heq_start(int layout, int frame_rate, QueuePlus *pq, char* out_file);
int iamf_heq_process(IAMF_HEQ *heq, int16_t *input, int size);
int iamf_heq_stop(IAMF_HEQ *heq);
#endif /* IAMF_HEQ_H */