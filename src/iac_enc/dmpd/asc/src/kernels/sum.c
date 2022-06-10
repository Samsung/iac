#include "kernel.h"

  void* Sum_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Sum_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Sum_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk;   
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Sum_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      const TfLiteTensor* input   = GetInput(context, node, 0);
      const TfLiteTensor* output  = GetOutput(context, node, 0);
      int size = NumElements(input);

      float sum = 0;
      for(int i=0;i<size;i++)
      {
        sum+= input->data.f[i];
      }

      output->data.f[0] = sum;
      return kTfLiteOk;
  }

  TfLiteRegistration* Register_SUM() {
  static TfLiteRegistration r = {Sum_Init, Sum_Free, Sum_Prepare,
                                 Sum_Eval};
  return &r;
}
