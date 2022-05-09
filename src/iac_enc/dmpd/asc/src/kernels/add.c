// This file has three implementation of Add.
#include "kernel.h"

/*
enum KernelType {
  kReference,
  kGenericOptimized,  // Neon-free
  kNeonOptimized,
};*/

int kInputTensor1 = 0;
int kInputTensor2 = 1;
int kOutputTensor = 0;

typedef struct Add_OpData {
  // These fields are used in both the general 8-bit -> 8bit quantized path,
  // and the special 16-bit -> 16bit quantized path
  int input1_shift;
  int input2_shift;
  int32_t output_activation_min;
  int32_t output_activation_max;

  // These fields are used only in the general 8-bit -> 8bit quantized path
  int32_t input1_multiplier;
  int32_t input2_multiplier;
  int32_t output_multiplier;
  int output_shift;
  int left_shift;
  int32_t input1_offset;
  int32_t input2_offset;
  int32_t output_offset;
}Add_OpData;

void* Add_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Add_OpData data;//void* data = new OpData;
	return NULL;
}

void Add_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  free( (Add_OpData*)buffer);
}

TfLiteStatus Add_Prepare(TfLiteContext* context, TfLiteNode* node) {
  TfLiteAddParams* params = (TfLiteAddParams*)(node->builtin_data);
  Add_OpData* data = (Add_OpData*)(node->user_data);

  //TF_LITE_ENSURE_EQ(context, NumInputs(node), 2);
  //TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  const TfLiteTensor* input1 = GetInput(context, node, kInputTensor1);
  const TfLiteTensor* input2 = GetInput(context, node, kInputTensor2);
  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

  TF_LITE_ENSURE_EQ(context, input1->type, input2->type);
  output->type = input2->type;

  const bool requires_broadcast = !HaveSameShapes(input1, input2);

  TfLiteIntArray* output_size = NULL;
  if (requires_broadcast) {
    TF_LITE_ENSURE_OK(context, CalculateShapeForBroadcast(
                                   context, input1, input2, &output_size));
  } else {
    output_size = TfLiteIntArrayCopy(input1->dims);
  }
  
  if (output->type != kTfLiteFloat32)	//Quantization not support
  {
	  LOGE("Add Prepare error: output type is not kTfLiteFloat32\n");
  } 
  return context->ResizeTensor(context, output, output_size);
}


void EvalAdd(TfLiteContext* context, TfLiteNode* node, TfLiteAddParams* params,
             const Add_OpData* data, const TfLiteTensor* input1,
             const TfLiteTensor* input2, TfLiteTensor* output) {
  /*tflite::ArithmeticParams op_params;
  const bool need_broadcast = optimized_ops::ProcessBroadcastShapes(
  GetTensorShape(input1), GetTensorShape(input2), &op_params);

  float output_activation_min, output_activation_max;                \
  CalculateActivationRange(params->activation, &output_activation_min,   \
                           &output_activation_max);                      \
  SetActivationParams(output_activation_min, output_activation_max,      \
                      &op_params); */                                      \
  // const bool need_broadcast = false;
  // if (output->type == kTfLiteInt32) {
  //   //if (kernel_type == kReference) {
  //     if (need_broadcast) {
  //       //TF_LITE_ADD(reference_ops, BroadcastAdd4DSlow, int32_t);
  //     } else {
  //       //TF_LITE_ADD(reference_ops, Add, int32_t);
  //     }
    
  // }
  int size = NumElements(input1);

  for(int i=0;i<size;i++){
    output->data.f[i] = input1->data.f[i] + input2->data.f[i];
  }
}



TfLiteStatus Add_Eval(TfLiteContext* context, TfLiteNode* node) {
  TfLiteAddParams* params = (TfLiteAddParams*)(node->builtin_data);
  Add_OpData* data = (Add_OpData*)(node->user_data);

  LOGE("Add_Eval\n");

  const TfLiteTensor* input1 = GetInput(context, node, kInputTensor1);
  const TfLiteTensor* input2 = GetInput(context, node, kInputTensor2);
  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

  if (output->type == kTfLiteFloat32 || output->type == kTfLiteInt32) {
    EvalAdd(context, node, params, data, input1, input2, output);
  }/* else if (output->type == kTfLiteUInt8 || output->type == kTfLiteInt8 ||
             output->type == kTfLiteInt16) {
    TF_LITE_ENSURE_OK(context,
                      EvalAddQuantized(context, node, params, data,
                                                    input1, input2, output));
  } */else {
    LOGE("Inputs and outputs not all float|uint8|int16 types.");
    return kTfLiteError;
  }

  return kTfLiteOk;
}

TfLiteRegistration* Register_ADD_GENERIC_OPT() {
  static TfLiteRegistration r = {Add_Init, Add_Free, Add_Prepare,
                                 Add_Eval};
  return &r;
}

TfLiteRegistration* Register_ADD() {
  return Register_ADD_GENERIC_OPT();
}
