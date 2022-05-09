#ifndef IA_HEQ_H
#define IA_HEQ_H

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
}IA_HEQ;

IA_HEQ *ia_heq_start(int layout, int frame_rate);
int ia_heq_process(IA_HEQ *heq, int16_t *input, int size);
int ia_heq_stop(IA_HEQ *heq);
#endif /* IA_HEQ_H */