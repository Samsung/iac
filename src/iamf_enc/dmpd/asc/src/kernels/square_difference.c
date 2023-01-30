// This file has three implementation of Add.
#include "kernel.h"

void* SQUARED_DIFFERENCE_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void SQUARED_DIFFERENCE_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus SQUARED_DIFFERENCE_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus SQUARED_DIFFERENCE_Eval(TfLiteContext* context, TfLiteNode* node) {
 
  return kTfLiteOk;
}



TfLiteRegistration* Register_SQUARED_DIFFERENCE() {
  static TfLiteRegistration r = {SQUARED_DIFFERENCE_Init, SQUARED_DIFFERENCE_Free, SQUARED_DIFFERENCE_Prepare,
                                 SQUARED_DIFFERENCE_Eval};
  return &r;
}
