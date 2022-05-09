#include "kernel.h"

  void* Div_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Div_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Div_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
      
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Div_Eval(TfLiteContext* context, TfLiteNode* node)
  {
    int kInputTensor1 = 0;
    int kInputTensor2 = 1;
    int kOutputTensor = 0;

    const TfLiteTensor* input1 = GetInput(context, node, kInputTensor1);
    const TfLiteTensor* input2 = GetInput(context, node, kInputTensor2);
    const TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

    int size = NumElements(input1);

    for(int i=0;i<size;i++)
    {
      output->data.f[i] = input1->data.f[i]/input2->data.f[0];
    }
  }

  TfLiteRegistration* Register_DIV() {
  static TfLiteRegistration r = {Div_Init, Div_Free, Div_Prepare,
                                 Div_Eval};
  return &r;
}
