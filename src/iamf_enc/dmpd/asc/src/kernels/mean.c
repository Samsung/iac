// This file has three implementation of Add.
#include "kernel.h"


void* MEAN_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void MEAN_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus MEAN_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus MEAN_Eval(TfLiteContext* context, TfLiteNode* node) {
 
  return kTfLiteOk;
}



TfLiteRegistration* Register_MEAN() {
  static TfLiteRegistration r = {MEAN_Init, MEAN_Free, MEAN_Prepare,
                                 MEAN_Eval};
  return &r;
}
