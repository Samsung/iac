#include "kernel.h"


typedef struct FullyConnectedParams {
  // uint8_t inference params.
  // TODO(b/65838351): Use smaller types if appropriate.
  int32_t input_offset;
  int32_t weights_offset;
  int32_t output_offset;
  int32_t output_multiplier;
  int output_shift;
  // uint8_t, etc, activation params.
  int32_t quantized_activation_min;
  int32_t quantized_activation_max;
  // float activation params.
  float float_activation_min;
  float float_activation_max;
  // Mark the operands as cacheable if they are unchanging, e.g. weights.
  bool lhs_cacheable;
  bool rhs_cacheable;
  int32_t weights_format;
}FullyConnectedParams;


void FullyConnected(
    const FullyConnectedParams* params, const TfLiteIntArray* input_shape,
    const float* input_data, const TfLiteIntArray* weights_shape,
    const float* weights_data, const TfLiteIntArray* bias_shape,
    const float* bias_data, const TfLiteIntArray* output_shape,
    float* output_data) {
  // const float output_activation_min = params.float_activation_min;
  // const float output_activation_max = params.float_activation_max;
  // TODO(b/62193649): This really should be:
  //     const int batches = ArraySize(output_dims, 1);
  // but the current --variable_batch hack consists in overwriting the 3rd
  // dimension with the runtime batch size, as we don't keep track for each
  // array of which dimension is the batch dimension in it.
  const int output_dims_count = output_shape->size;
  const int weights_dims_count = weights_shape->size;
  const int batches = FlatSizeSkipDim(output_shape, output_dims_count - 1);
  const int output_depth = MatchingDim(weights_shape, weights_dims_count - 2,
                                       output_shape, output_dims_count - 1);
  const int accum_depth = weights_shape->data[weights_dims_count - 1];
  for (int b = 0; b < batches; ++b) {
    for (int out_c = 0; out_c < output_depth; ++out_c) {
      float total = 0.f;
      for (int d = 0; d < accum_depth; ++d) {
        total += input_data[b * accum_depth + d] *
                 weights_data[out_c * accum_depth + d];
      }
      float bias_value = 0.0f;
      if (bias_data) {
        bias_value = bias_data[out_c];
      }
      // output_data[out_c + output_depth * b] = ActivationFunctionWithMinMax(
      //     total + bias_value, output_activation_min, output_activation_max);
      output_data[out_c + output_depth * b] = total + bias_value;
    }
  }
}

TfLiteStatus FC_EvalFloat(TfLiteContext* context, TfLiteNode* node,
                       TfLiteFullyConnectedParams* params, void* data,
                       const TfLiteTensor* input, const TfLiteTensor* filter,
                       const TfLiteTensor* bias, TfLiteTensor* output) {
  // float output_activation_min, output_activation_max;
  // CalculateActivationRange(params->activation, &output_activation_min,
  //                          &output_activation_max);
  FullyConnected(NULL, GetTensorShape(input), GetTensorData(input),
      GetTensorShape(filter), GetTensorData(filter),
      GetTensorShape(bias), GetTensorData(bias),
      GetTensorShape(output), GetTensorData(output));

  return kTfLiteOk;
}

  void* FC_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void FC_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus FC_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk;  
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus FC_Eval(TfLiteContext* context, TfLiteNode* node)
  {
    TfLiteFullyConnectedParams* params = (TfLiteFullyConnectedParams*)(node->builtin_data);
    //OpData* data = reinterpret_cast<OpData*>(node->user_data);

    const TfLiteTensor* input =  GetInput(context, node, 0);
    const TfLiteTensor* filter = GetInput(context, node, 1);
    const TfLiteTensor* bias =  (node->inputs->size == 3)? GetInput(context, node, 2): NULL;
    TfLiteTensor* output = GetOutput(context, node, 0);


    FC_EvalFloat(context, node, params, NULL, input, filter,
                                    bias, output);
    
    LOGE("FC_Eval\n");
    return kTfLiteOk;
  }

  TfLiteRegistration* Register_FC() {
  static TfLiteRegistration r = {FC_Init, FC_Free, FC_Prepare,
                                 FC_Eval};
  return &r;
}
