
#ifndef _PROGRESSBAR_H_
#define _PROGRESSBAR_H_


typedef struct {
  int _x;
  void(*startBar)(struct ProgressBar*, char *, int);
  void(*proceedBar)(struct ProgressBar*, int);
  void(*endBar)(struct ProgressBar*, int);
}ProgressBar;
void ProgressBarInit(ProgressBar *thisp);

#endif