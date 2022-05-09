#ifndef TFLITE_COMMON_H
#define TFLITE_COMMON_H

#include "common.h"
#include "tflite_reader.h"
TfLiteStatus MultiplyAndCheckOverflow(size_t a, size_t b, size_t* product) ;

//TfLiteStatus ConvertTensorType(TensorType tensor_type, TfLiteType* type );

//TfLiteStatus ParseOpData(const Operator* op, BuiltinOperator op_type,  void** builtin_data);
TfLiteIntArray* ConvertArrayToTfLiteIntArray(const int rank, const int* dims) ;

bool EqualArrayAndTfLiteIntArray(const TfLiteIntArray* a, const int b_size,
                                 const int* b) ;

TfLiteStatus GetSizeOfType(TfLiteContext* context, const TfLiteType type,
                           size_t* bytes) ;

void Print_TfLiteTensor(TfLiteTensor* tensor);

TfLiteStatus ConvertTensorType(tflite_TensorType_enum_t tensor_type, TfLiteType* type) ;


#endif