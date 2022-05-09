#ifndef UTILS_H
#define UTILS_H

#include<stdio.h>
#include<stdlib.h>

#include "wave.h"
#include"common.h"

void dumpWave(Wav* wave);
void dumpChannel(char* buffer,char* name);
void dumpFrame(char *buffer);


#endif 