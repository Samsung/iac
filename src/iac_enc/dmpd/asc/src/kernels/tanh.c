// This file has three implementation of Add.
#include "kernel.h"


void* TANH_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void TANH_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus TANH_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus TANH_Eval(TfLiteContext* context, TfLiteNode* node) {
 
  return kTfLiteOk;
}



TfLiteRegistration* Register_TANH() {
  static TfLiteRegistration r = {TANH_Init, TANH_Free, TANH_Prepare,
                                 TANH_Eval};
  return &r;
}
