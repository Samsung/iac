// This file has three implementation of Add.
#include "kernel.h"


void* RSQRT_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void RSQRT_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus RSQRT_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus RSQRT_Eval(TfLiteContext* context, TfLiteNode* node) {
 
  return kTfLiteOk;
}



TfLiteRegistration* Register_RSQRT() {
  static TfLiteRegistration r = {RSQRT_Init, RSQRT_Free, RSQRT_Prepare,
                                 RSQRT_Eval};
  return &r;
}
