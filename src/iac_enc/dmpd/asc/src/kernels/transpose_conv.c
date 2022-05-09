// This file has three implementation of Add.
#include "kernel.h"


void* TRANSPOSE_CONV_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void TRANSPOSE_CONV_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus TRANSPOSE_CONV_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus TRANSPOSE_CONV_Eval(TfLiteContext* context, TfLiteNode* node) {
 
  return kTfLiteOk;
}



TfLiteRegistration* Register_TRANSPOSE_CONV() {
   static TfLiteRegistration r = {TRANSPOSE_CONV_Init, TRANSPOSE_CONV_Free, TRANSPOSE_CONV_Prepare,
                                 TRANSPOSE_CONV_Eval};
  return &r;
}
