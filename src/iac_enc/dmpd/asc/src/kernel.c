#include "common.h"
#include "builtin_op_data.h"
#include "kernel.h"




bool HaveSameShapes(const TfLiteTensor* input1, const TfLiteTensor* input2) {
  return TfLiteIntArrayEqual(input1->dims, input2->dims);
}

TfLiteStatus CalculateShapeForBroadcast(TfLiteContext* context,
                                        const TfLiteTensor* input1,
                                        const TfLiteTensor* input2,
                                        TfLiteIntArray** output_shape) {
  int dims1 = NumDimensions(input1);
  int dims2 = NumDimensions(input2);
  int out_dims =dims1> dims2? dims1:dims2;
  if (NumElements(input1) == 0) {
    *output_shape = TfLiteIntArrayCopy(input1->dims);
    return kTfLiteOk;
  }
  TfLiteIntArray* shape = TfLiteIntArrayCreate(out_dims);
  for (int i = 0; i < out_dims; ++i) {
    int d1 = i >= dims1 ? 1 : SizeOfDimension(input1, dims1 - i - 1);
    int d2 = i >= dims2 ? 1 : SizeOfDimension(input2, dims2 - i - 1);
    TF_LITE_ENSURE(context, d1 == d2 || d1 == 1 || d2 == 1);
    shape->data[out_dims - i - 1] = d1>d2?d1:d2;
  }
  *output_shape = shape;
  return kTfLiteOk;
}

TfLiteStatus CalculateShapeForBroadcast_3Inputs(TfLiteContext* context,
                                        const TfLiteTensor* input1,
                                        const TfLiteTensor* input2,
                                        const TfLiteTensor* input3,
                                        TfLiteIntArray** output_shape) {
  int dims1 = NumDimensions(input1);
  int dims2 = NumDimensions(input2);
  int dims3 = NumDimensions(input3);
  int temp = dims1>dims2?dims1:dims2;  
  int out_dims = dims3>temp?dims3:temp;
  TfLiteIntArray* shape = TfLiteIntArrayCreate(out_dims);
  for (int i = 0; i < out_dims; ++i) {
    int d1 = i >= dims1 ? 1 : SizeOfDimension(input1, dims1 - i - 1);
    int d2 = i >= dims2 ? 1 : SizeOfDimension(input2, dims2 - i - 1);
    int d3 = i >= dims3 ? 1 : SizeOfDimension(input3, dims3 - i - 1);
	temp = d1>d2?d1:d2;  
	int max_value = d3>temp?d3:temp;
    //int max_value = std::max(std::max(d1, d2), d3);
    TF_LITE_ENSURE(context, d1 == 1 || d1 == max_value);
    TF_LITE_ENSURE(context, d2 == 1 || d2 == max_value);
    TF_LITE_ENSURE(context, d3 == 1 || d3 == max_value);
    shape->data[out_dims - i - 1] = max_value;
  }
  *output_shape = shape;
  return kTfLiteOk;
}

