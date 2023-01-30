#include "progressbar.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
void _startbar(ProgressBar *thisp, char *title, int x);
void _proceedbar(ProgressBar *thisp, int x);
void _endbar(ProgressBar *thisp, int x);


void _constructprogressbar(ProgressBar *thisp)
{
  thisp->_x = 0;
}

void _startbar(ProgressBar *thisp, char *title, int x)
{
  // creates a progress bar 40 chars long on the console and moves cursor back to beginning with BS character
  printf("%s[", title);
  _proceedbar(thisp, x);
}

void _proceedbar(ProgressBar *thisp, int x)
{
  int ax;

  ax = (x > 0) ? x : -x;

  // Sets progress bar to a certain percentage x. Progress is given as whole percentage, i.e. 50 % done is given by x = 50
  int xp = (ax * 40) / 100;
  for (int i = 0; i < xp; i++)
  {
    printf("#");
  }
  for (int i = 0; i < 40 - xp; i++)
  {
    printf("-");
  }
  printf("]%d%%", ax);

  if (x >= 0)
  {
    int e;
    if (x < 10) e = 41 + 2;
    else if (x < 100)  e = 41 + 3;
    else e = 41 + 4;
    for (int i = 0; i < e; i++)
    {
      printf("\b");
    }
  }
  else
  {
    printf("\n");
  }
  thisp->_x = ax;
}
void _endbar(ProgressBar *thisp, int x)
{
  // End of progress bar;	Write full bar, then move to next line
  _proceedbar(thisp , -x);
}
void ProgressBarInit(ProgressBar *thisp)
{
  thisp->startBar = _startbar;
  thisp->proceedBar = _proceedbar;
  thisp->endBar = _endbar;
  _constructprogressbar(thisp);
}