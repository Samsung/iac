#include <math.h>
#include "kernel.h"

  void* Sqrt_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Sqrt_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Sqrt_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk;   
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Sqrt_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      const TfLiteTensor* input   = GetInput(context, node, 0);
      const TfLiteTensor* output  = GetOutput(context, node, 0);

      output->data.f[0] = sqrt(input->data.f[0]);
      return kTfLiteOk;
  }

  TfLiteRegistration* Register_SQRT() {
  static TfLiteRegistration r = {Sqrt_Init, Sqrt_Free, Sqrt_Prepare,
                                 Sqrt_Eval};
  return &r;
}
