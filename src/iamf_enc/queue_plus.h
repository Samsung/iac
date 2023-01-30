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
}ValueType;

typedef void* QDataType;

typedef struct QueueNode
{
  QDataType _data;
  int _offset;
  struct QueueNode * _next;
}QueueNode;

typedef struct QueuePlus
{
  QueueNode* _head;
  QueueNode* _tail;
  ValueType _type;
  int _size;
  int _coeffs;
  int _length;
}QueuePlus;

int QueueInit(QueuePlus *pq, ValueType type, int size, int coeffs);
int QueuePush(QueuePlus *pq, void * input);
int QueuePop(QueuePlus *pq, void * output, int size);
int QueueLength(QueuePlus *pq);
int QueueDestroy(QueuePlus *pq);
#endif //QUEUE_PLUS_H