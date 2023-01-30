#ifndef TFLITE_KERNEL_H_
#define TFLITE_KERNEL_H_

#include "common.h"
#include "builtin_op_data.h"

static inline int NumInputs(const TfLiteNode* node) { return node->inputs->size; }
static inline int NumOutputs(const TfLiteNode* node) { return node->outputs->size; }
static inline int NumIntermediates(const TfLiteNode* node) {return node->intermediates->size;}

static inline int NumDimensions(const TfLiteTensor* t) { return t->dims->size; }
static inline int SizeOfDimension(const TfLiteTensor* t, int dim) { return t->dims->data[dim];}

static inline const TfLiteTensor* GetInput(TfLiteContext* context,
                                    const TfLiteNode* node, int index) {
  return &context->tensors[(node->inputs->data[index])];
}

static inline TfLiteTensor* GetOutput(TfLiteContext* context, const TfLiteNode* node,
                               int index) {
  return &context
              ->tensors[(node->outputs->data[index])];
}

static inline TfLiteTensor* GetVariableInput(TfLiteContext* context,
                                      const TfLiteNode* node, int index) {
  TfLiteTensor* tensor =
      &context->tensors[(node->inputs->data[index])];
  return (tensor->is_variable) ? tensor : NULL;
}
static inline int64_t NumElements_Array(const TfLiteIntArray* dims) {
  int64_t count = 1;
  for (int i = 0; i < dims->size; ++i) {
    count *= dims->data[i];
  }
  return count;
}

static inline int64_t NumElements(const TfLiteTensor* t) {
  return NumElements_Array(t->dims);
}

// static inline int64_t NumElements_Tensor(const TfLiteTensor* t) {
//   return NumElements(t->dims);
// }

bool HaveSameShapes(const TfLiteTensor* input1, const TfLiteTensor* input2);

TfLiteStatus CalculateShapeForBroadcast(TfLiteContext* context,
                                        const TfLiteTensor* input1,
                                        const TfLiteTensor* input2,
                                        TfLiteIntArray** output_shape);

TfLiteStatus CalculateShapeForBroadcast_3Inputs(TfLiteContext* context,
                                        const TfLiteTensor* input1,
                                        const TfLiteTensor* input2,
                                        const TfLiteTensor* input3,
                                        TfLiteIntArray** output_shape) ;
#endif  // TFLITE_KERNEL_H_
