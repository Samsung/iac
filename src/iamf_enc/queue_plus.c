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
 * @file queue_plus.c
 * @brief Define new queue to process push and pop operaton
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "queue_plus.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int QueueInit(QueuePlus *pq, ValueType type, int size, int coeffs) {
  pq->_head = pq->_tail = NULL;
  pq->_size = size;
  pq->_coeffs = coeffs;
  pq->_type = type;
  pq->_length = 0;
  return 0;
}

int QueueDestroy(QueuePlus *pq) {
  QueueNode *cur = pq->_head;
  while (cur) {
    QueueNode *next = cur->_next;
    if (cur->_data) free(cur->_data);
    free(cur);
    cur = next;
  }
  pq->_head = pq->_tail = NULL;
  pq->_length = 0;
  return 0;
}

int QueueLength(QueuePlus *pq) { return pq->_length; }

int QueuePush(QueuePlus *pq, void *input) {
  QueueNode *newnode = (QueueNode *)malloc(sizeof(QueueNode));
  if (newnode == NULL) {
    printf("newnode alloate failed\n");
    return -1;
  }
  memset(newnode, 0x00, sizeof(QueueNode));
  switch (pq->_type) {
    case kUInt8:
      newnode->_data =
          (void *)malloc(sizeof(uint8_t) * pq->_coeffs * pq->_size);
      memcpy(newnode->_data, input, sizeof(uint8_t) * pq->_coeffs * pq->_size);
      break;
    case kInt16:
      newnode->_data =
          (void *)malloc(sizeof(int16_t) * pq->_coeffs * pq->_size);
      memcpy(newnode->_data, input, sizeof(int16_t) * pq->_coeffs * pq->_size);
      break;
    case kInt32:
      newnode->_data =
          (void *)malloc(sizeof(int32_t) * pq->_coeffs * pq->_size);
      memcpy(newnode->_data, input, sizeof(int32_t) * pq->_coeffs * pq->_size);
      break;
    case kFloat:
      newnode->_data = (void *)malloc(sizeof(float) * pq->_coeffs * pq->_size);
      memcpy(newnode->_data, input, sizeof(float) * pq->_coeffs * pq->_size);
      break;
    default:
      printf("wrong value type!!!\n");
      break;
  }
  if (pq->_head == NULL) {
    pq->_head = pq->_tail = newnode;
  } else {
    pq->_tail->_next = newnode;
    pq->_tail = newnode;
  }
  pq->_length++;
  return 0;
}

int QueuePush2(QueuePlus *pq, void *input, int size) {
  QueueNode *newnode = (QueueNode *)malloc(sizeof(QueueNode));
  if (newnode == NULL) {
    printf("newnode alloate failed\n");
    return -1;
  }
  memset(newnode, 0x00, sizeof(QueueNode));
  switch (pq->_type) {
  case kUInt8:
    newnode->_data =
      (void *)malloc(sizeof(uint8_t) * pq->_coeffs * pq->_size);
    memset(newnode->_data, 0x00, sizeof(uint8_t) * pq->_coeffs * pq->_size);
    memcpy(newnode->_data, input, sizeof(uint8_t) * pq->_coeffs * size);
    break;
  case kInt16:
    newnode->_data =
      (void *)malloc(sizeof(int16_t) * pq->_coeffs * pq->_size);
    memset(newnode->_data, 0x00, sizeof(int16_t) * pq->_coeffs * pq->_size);
    memcpy(newnode->_data, input, sizeof(int16_t) * pq->_coeffs * size);
    break;
  case kInt32:
    newnode->_data =
      (void *)malloc(sizeof(int32_t) * pq->_coeffs * pq->_size);
    memset(newnode->_data, 0x00, sizeof(int32_t) * pq->_coeffs * pq->_size);
    memcpy(newnode->_data, input, sizeof(int32_t) * pq->_coeffs * size);
    break;
  case kFloat:
    newnode->_data = (void *)malloc(sizeof(float) * pq->_coeffs * pq->_size);
    memset(newnode->_data, 0x00, sizeof(float) * pq->_coeffs * pq->_size);
    memcpy(newnode->_data, input, sizeof(float) * pq->_coeffs * size);
    break;
  default:
    printf("wrong value type!!!\n");
    break;
  }
  if (pq->_head == NULL) {
    pq->_head = pq->_tail = newnode;
  }
  else {
    pq->_tail->_next = newnode;
    pq->_tail = newnode;
  }
  pq->_length++;
  return 0;
}

int QueuePop(QueuePlus *pq, void *output, int size) {
  if (size > pq->_size) {
    printf("pop size is illegal, should not be more than pq->_size\n");
    return -1;
  }
  if (pq->_head == NULL) {
    return 0;
  }
  if (size <= 0)
    return 0;
  QueueNode *next = pq->_head->_next;

  if (pq->_size - pq->_head->_offset > size) {
    int offset = pq->_head->_offset;
    switch (pq->_type) {
      case kUInt8:
        memcpy(output, (uint8_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(uint8_t) * pq->_coeffs * size);
        break;
      case kInt16:
        memcpy(output, (int16_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(int16_t) * pq->_coeffs * size);
        break;
      case kInt32:
        memcpy(output, (int32_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(int32_t) * pq->_coeffs * size);
        break;
      case kFloat:
        memcpy(output, (float *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(float) * pq->_coeffs * size);
        break;
      default:
        printf("wrong value type!!!\n");
        break;
    }
    pq->_head->_offset += size;
    return size;
  } else if (pq->_size - pq->_head->_offset == size) {
    int offset = pq->_head->_offset;
    switch (pq->_type) {
      case kUInt8:
        memcpy(output, (uint8_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(uint8_t) * pq->_coeffs * size);
        break;
      case kInt16:
        memcpy(output, (int16_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(int16_t) * pq->_coeffs * size);
        break;
      case kInt32:
        memcpy(output, (int32_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(int32_t) * pq->_coeffs * size);
        break;
      case kFloat:
        memcpy(output, (float *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(float) * pq->_coeffs * size);
        break;
      default:
        printf("wrong value type!!!\n");
        break;
    }
    if (pq->_head->_data) free(pq->_head->_data);
    free(pq->_head);
    pq->_head = next;
    if (pq->_head == NULL) {
      pq->_tail = NULL;
    }
    pq->_length--;
    return size;
  } else if (pq->_size - pq->_head->_offset < size) {
    int offset = pq->_head->_offset;
    int copy_size = pq->_size - pq->_head->_offset;
    switch (pq->_type) {
      case kUInt8:
        memcpy(output, (uint8_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(uint8_t) * pq->_coeffs * copy_size);
        break;
      case kInt16:
        memcpy(output, (int16_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(int16_t) * pq->_coeffs * copy_size);
        break;
      case kInt32:
        memcpy(output, (int32_t *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(int32_t) * pq->_coeffs * copy_size);
        break;
      case kFloat:
        memcpy(output, (float *)pq->_head->_data + pq->_coeffs * offset,
               sizeof(float) * pq->_coeffs * copy_size);
        break;
      default:
        printf("wrong value type!!!\n");
        break;
    }
    if (pq->_head->_data) free(pq->_head->_data);
    free(pq->_head);
    pq->_head = next;
    if (pq->_head == NULL) {
      pq->_tail = NULL;
      return copy_size;
    }
    pq->_length--;

    offset = copy_size;
    pq->_head->_offset = size - copy_size;
    copy_size = pq->_head->_offset;
    switch (pq->_type) {
      case kUInt8:
        memcpy((uint8_t *)output + pq->_coeffs * offset,
               (uint8_t *)pq->_head->_data,
               sizeof(uint8_t) * pq->_coeffs * copy_size);
        break;
      case kInt16:
        memcpy((int16_t *)output + pq->_coeffs * offset,
               (int16_t *)pq->_head->_data,
               sizeof(int16_t) * pq->_coeffs * copy_size);
        break;
      case kInt32:
        memcpy((int32_t *)output + pq->_coeffs * offset,
               (int32_t *)pq->_head->_data,
               sizeof(int32_t) * pq->_coeffs * copy_size);
        break;
      case kFloat:
        memcpy((float *)output + pq->_coeffs * offset,
               (float *)pq->_head->_data,
               sizeof(float) * pq->_coeffs * copy_size);
        break;
      default:
        printf("wrong value type!!!\n");
        break;
    }
    return size;
  }
  return 0;
}