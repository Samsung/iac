#include "kernel.h"
#include  <math.h>

// int FlatSizeSkipDim(TfLiteIntArray* shape, int skip_dim) {
  
//   int flat_size = 1;
//   for (int i = 0; i < shape->size; ++i) {
//     flat_size *= (i == skip_dim) ? 1 : shape->data[i];
//   }
//   return flat_size;
// }

// int MatchingFlatSizeSkipDim(TfLiteIntArray* shape, int skip_dim) {  
//   return FlatSizeSkipDim(shape, skip_dim);
// }

  void* Softmax_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Softmax_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Softmax_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk;      
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Softmax_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      TfLiteTensor* input_tensor = GetInput(context,node,0);
      TfLiteTensor* output_tensor = GetOutput(context,node,0);
      TfLiteIntArray* input_shape = input_tensor->dims;
      TfLiteIntArray* output_shape = output_tensor->dims;
      TfLiteSoftmaxParams *  params  = (TfLiteSoftmaxParams *)(node->builtin_data);
      float* input_data = input_tensor->data.raw;
      float* output_data = output_tensor->data.raw;

      const int trailing_dim = input_shape->size - 1;
      const int outer_size = MatchingFlatSizeSkipDim(input_shape, trailing_dim);
      const int depth = MatchingDim(input_shape, trailing_dim, output_shape, trailing_dim);

  for (int i = 0; i < outer_size; ++i) {
    // Find max element value which we'll use to ensure numerical stability
    // taking advantage of the following equality:
    // exp(x[i])/sum(exp(x[i])) == exp(x[i]+C)/sum(exp(x[i]+C))
    //float max = std::numeric_limits<float>::lowest();
    float max = 0;
    for (int c = 0; c < depth; ++c) {
      max = MAX(max, input_data[i * depth + c]);
    }

    // Compute sum.
    float sum = 0.f;
    for (int c = 0; c < depth; ++c) {
      const float exp_c = exp(( input_data[i * depth + c] - max) * (params->beta));
      output_data[i * depth + c] = exp_c;
      sum += exp_c;
    }

    // Compute result.
    for (int c = 0; c < depth; ++c) {
      output_data[i * depth + c] = output_data[i * depth + c] / sum;
    }
  }

      LOGE("Softmax_Eval\n");
      return kTfLiteOk;

  }

  TfLiteRegistration* Register_SOFTMAX() {
  static TfLiteRegistration r = {Softmax_Init, Softmax_Free, Softmax_Prepare,
                                 Softmax_Eval};
  return &r;
}
