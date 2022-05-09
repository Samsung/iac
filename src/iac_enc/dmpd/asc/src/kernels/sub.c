#include "kernel.h"

void* Sub_Init(TfLiteContext* context, const char* buffer, size_t length)
{
  return NULL;// should return one value
}

// The pointer `buffer` is the data previously returned by an init invocation.
void Sub_Free(TfLiteContext* context, void* buffer)
{

}

// prepare is called when the inputs this node depends on have been resized.
// context->ResizeTensor() can be called to request output tensors to be
// resized.
//
// Returns kTfLiteOk on success.
TfLiteStatus Sub_Prepare(TfLiteContext* context, TfLiteNode* node)
{

    
}

// Execute the node (should read node->inputs and output to node->outputs).
// Returns kTfLiteOk on success.
TfLiteStatus Sub_Eval(TfLiteContext* context, TfLiteNode* node)
{

      TfLiteTensor* input1 = GetInput(context,node,0);
      TfLiteTensor* input2 = GetInput(context,node,1);

      TfLiteTensor* output = GetOutput(context,node,0);

      float* input1_data_ptr = input1->data.raw;
      float* input2_data_ptr = input2->data.raw;

      float* output_data_ptr = output->data.raw;


      int size = 1;
      for(int i=0;i<input1->dims->size;i++){
        size *= input1->dims->data[i];
      }

      for(int i=0; i<size;i++)
      {        
        output_data_ptr[i] = input1_data_ptr[i] - input2_data_ptr[i];
      }

    LOGE("Sub_Eval\n");

}

TfLiteRegistration* Register_SUB() {
static TfLiteRegistration r = {Sub_Init, Sub_Free, Sub_Prepare,
                                Sub_Eval};
return &r;
}
