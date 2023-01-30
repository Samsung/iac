#include "kernel.h"

  void* Relu_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;//should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Relu_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Relu_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk;      
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Relu_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      TfLiteTensor* input   = GetInput(context,node,0);
      TfLiteTensor* output  = GetOutput(context,node,0);


      int size = 1;

      for(int i=0;i<input->dims->size;i++){
        size *= input->dims->data[i];
      }

      for(int i=0;i<size;i++)
      {
        if(input->data.f[i] > 0){
          output->data.f[i] = input->data.f[i];
        }else{
          output->data.f[i] = 0;
        }
      }

      LOGE("Relu_Eval\n");
      return kTfLiteOk;
  }

  TfLiteRegistration* Register_RELU() {
  static TfLiteRegistration r = {Relu_Init, Relu_Free, Relu_Prepare,
                                 Relu_Eval};
  return &r;
}
