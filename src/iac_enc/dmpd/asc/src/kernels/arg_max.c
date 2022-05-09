#include "kernel.h"

  void* ArgMax_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void ArgMax_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus ArgMax_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
      
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus ArgMax_Eval(TfLiteContext* context, TfLiteNode* node)
  {
    const TfLiteTensor* input = GetInput(context, node, 0);
    const TfLiteTensor* axis = GetInput(context, node,1);
    TfLiteTensor* output = GetOutput(context, node, 0);
  // if (IsDynamicTensor(output)) {
  //   TF_LITE_ENSURE_STATUS(ResizeOutput(context, input, axis, output));
  // }
    int size = 1;
    for(int i=0;i<input->dims->size;i++){
      size *= input->dims->data[i];
    }

    int index = 0;
    float max = input->data.f[0];
    for(int i=0; i< size;i++)
    {
      if(input->data.f[i] >max){
          max = input->data.f[i];
          index = i;
      }
    }
    output->data.f[0] = index;
    LOGE("ArgMax_Eval %d\n",index);

  }

  TfLiteRegistration* Register_ARGMAX() {
  static TfLiteRegistration r = {ArgMax_Init, ArgMax_Free, ArgMax_Prepare,
                                 ArgMax_Eval};
  return &r;
}
