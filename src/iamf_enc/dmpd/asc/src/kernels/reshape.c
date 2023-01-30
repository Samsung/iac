#include "kernel.h"

  void* Reshape_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Reshape_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Reshape_Prepare(TfLiteContext* context, TfLiteNode* node)
  {


    

    return kTfLiteOk;
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Reshape_Eval(TfLiteContext* context, TfLiteNode* node)
  {
    int kInputTensor = 0;
    int kShapeTensor = 1;
    int kOutputTensor = 0;

    TfLiteTensor* input  = GetInput(context, node, kInputTensor);
    TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

    int input_ndims = NumDimensions(input);
    int input_size  = NumElements(input);
    int output_ndims = NumDimensions(output);
    int output_size  = NumElements(output);

    memcpy(output->data.raw, input->data.raw, input->bytes);
    return kTfLiteOk;

  }

  TfLiteRegistration* Register_RESHAPE() {
  static TfLiteRegistration r = {Reshape_Init, Reshape_Free, Reshape_Prepare,
                                 Reshape_Eval};
  return &r;
}
