// This file has three implementation of Add.
#include "kernel.h"


void* NEG_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void NEG_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus NEG_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus NEG_Eval(TfLiteContext* context, TfLiteNode* node) {
 
  return kTfLiteOk;
}



TfLiteRegistration* Register_NEG() {
  static TfLiteRegistration r = {NEG_Init, NEG_Free, NEG_Prepare,
                                 NEG_Eval};
  return &r;
}
