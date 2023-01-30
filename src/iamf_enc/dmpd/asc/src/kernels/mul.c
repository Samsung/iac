// This file has three implementation of Add.
#include "kernel.h"

void* MUL_Init(TfLiteContext* context, const char* buffer, size_t length) {
  //Opdata data;//void* data = new OpData;
  //return &data;
  return NULL;
}

void MUL_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus MUL_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus MUL_Eval(TfLiteContext* context, TfLiteNode* node) {

  TfLiteMulParams * params = (TfLiteMulParams*)(node->builtin_data);
  //OpData* data = reinterpret_cast<OpData*>(node->user_data);

  const TfLiteTensor* input1 = GetInput(context, node, 0);
  const TfLiteTensor* input2 = GetInput(context, node, 1);
  TfLiteTensor* output = GetOutput(context, node, 0);

  int size1 = NumElements(input1);
  int size2 = NumElements(input2);

  if(size1 == size2){
    for(int i=0;i<size1;i++){
      output->data.f[i] = input1->data.f[i]*input2->data.f[i];
    }

  }else{

    if(size1 > size2)
    {
      TfLiteTensor* temp;
      temp = input1;
      input1= input2;
      input2 = temp;
    }

    int height  = input2->dims->data[1];
    int width   = input2->dims->data[2];
    int channel = input2->dims->data[3];

    for(int h=0;h<height;h++){
      for(int w=0;w<width;w++){
        for(int c=0;c<channel;c++){
          output->data.f[h*width*channel + w*channel + c] = input1->data.f[c] * input2->data.f[h*width*channel + w*channel + c];
        }
      }

    }
  }

  // for(int i=0; i<size;i++)
  // {
  //   output->data.f[i] = input1->data.f[i] * input2->data.f[i];
  // }
    
  LOGE("Mul_Eval\n");
  return kTfLiteOk;
}



TfLiteRegistration* Register_MUL() {
  static TfLiteRegistration r = {MUL_Init, MUL_Free, MUL_Prepare,
                                 MUL_Eval};
  return &r;
}
