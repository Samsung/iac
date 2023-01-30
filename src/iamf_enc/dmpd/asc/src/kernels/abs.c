#include "kernel.h"
#include <stdlib.h>
  void* Abs_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;//should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Abs_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Abs_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk; 
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Abs_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      TfLiteTensor* input = GetInput(context,node,0);
      TfLiteTensor* output = GetOutput(context,node,0);

      float* input_data_ptr = input->data.raw;
      float* output_data_ptr = output->data.raw;

      int size = 1;
      for(int i=0;i<input->dims->size;i++){
        size *= input->dims->data[i];
      }

      for(int i=0; i<size;i++)
      {        
        output_data_ptr[i] = input_data_ptr[i] > 0 ? input_data_ptr[i]: -input_data_ptr[i];
      }
      
      LOGE("Abs_Eval\n");
      return kTfLiteOk;
  }

  TfLiteRegistration* Register_ABS() {
  static TfLiteRegistration r = {Abs_Init, Abs_Free, Abs_Prepare,
                                 Abs_Eval};
  return &r;
}
