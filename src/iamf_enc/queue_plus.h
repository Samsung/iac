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
 * @file queue_plus.h
 * @brief Define new queue to process push and pop operaton
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef QUEUE_PLUS_H
#define QUEUE_PLUS_H
#include "stdint.h"

typedef enum {
  kUnknown,
  kInt8,
  kUInt8,
  kInt16,
  kUInt16,
  kInt32,
  kUInt32,
  kFloat,
  kMax,
} ValueType;

typedef void *QDataType;

typedef struct QueueNode {
  QDataType _data;
  int _offset;
  struct QueueNode *_next;
} QueueNode;

typedef struct QueuePlus {
  QueueNode *_head;
  QueueNode *_tail;
  ValueType _type;
  int _size;
  int _coeffs;
  int _length;
} QueuePlus;

int QueueInit(QueuePlus *pq, ValueType type, int size, int coeffs);
int QueuePush(QueuePlus *pq, void *input);
int QueuePush2(QueuePlus *pq, void *input, int size);
int QueuePop(QueuePlus *pq, void *output, int size);
int QueueLength(QueuePlus *pq);
int QueueDestroy(QueuePlus *pq);
#endif  // QUEUE_PLUS_H