/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file progressbar.c
 * @brief progressbar rendering function
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "progressbar.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void _startbar(ProgressBar *thisp, char *title, int x);
void _proceedbar(ProgressBar *thisp, int x);
void _endbar(ProgressBar *thisp, int x);

void _constructprogressbar(ProgressBar *thisp) { thisp->_x = 0; }

void _startbar(ProgressBar *thisp, char *title, int x) {
  // creates a progress bar 40 chars long on the console and moves cursor back
  // to beginning with BS character
  printf("%s[", title);
  _proceedbar(thisp, x);
}

void _proceedbar(ProgressBar *thisp, int x) {
  int ax;

  ax = (x > 0) ? x : -x;

  // Sets progress bar to a certain percentage x. Progress is given as whole
  // percentage, i.e. 50 % done is given by x = 50
  int xp = (ax * 40) / 100;
  for (int i = 0; i < xp; i++) {
    printf("#");
  }
  for (int i = 0; i < 40 - xp; i++) {
    printf("-");
  }
  printf("]%d%%", ax);

  if (x >= 0) {
    int e;
    if (x < 10)
      e = 41 + 2;
    else if (x < 100)
      e = 41 + 3;
    else
      e = 41 + 4;
    for (int i = 0; i < e; i++) {
      printf("\b");
    }
  } else {
    printf("\n");
  }
  thisp->_x = ax;
}
void _endbar(ProgressBar *thisp, int x) {
  // End of progress bar;	Write full bar, then move to next line
  _proceedbar(thisp, -x);
}
void ProgressBarInit(ProgressBar *thisp) {
  thisp->startBar = _startbar;
  thisp->proceedBar = _proceedbar;
  thisp->endBar = _endbar;
  _constructprogressbar(thisp);
}