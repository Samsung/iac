// This file has three implementation of Add.
#include "kernel.h"
#include <assert.h>

static const int kInputTensor = 0;
static const int kFilterTensor = 1;
static const int kBiasTensor = 2;
static const int kOutputTensor = 0;


typedef struct DepthwiseParams {
  PaddingType padding_type;
  PaddingValues padding_values;
  int16_t stride_width;
  int16_t stride_height;
  int16_t dilation_width_factor;
  int16_t dilation_height_factor;
  int16_t depth_multiplier;
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
  const int32_t* output_multiplier_per_channel;
  const int32_t* output_shift_per_channel;
}DepthwiseParams;




typedef struct Depthwise_Conv_OpData {
  TfLitePaddingValues padding;
  // The scaling factor from input to output (aka the 'real multiplier') can
  // be represented as a fixed point multiplier plus a left shift.
  int32_t output_multiplier;
  int output_shift;
  // The range of the fused activation layer. For example for kNone and
  // uint8_t these would be 0 and 255.
  int32_t output_activation_min;
  int32_t output_activation_max;

  // Per channel output multiplier and shift.
  TfLiteIntArray* per_channel_output_multiplier;
  TfLiteIntArray* per_channel_output_shift;

  // Hybrid per channel temporary tensors.
  int input_quantized_id;
  int scaling_factors_id;
  int input_offset_id;
  int32_t input_quantized_index;
  int32_t scaling_factors_index;
  int32_t input_offset_index;

}Depthwise_Conv_OpData;


// static inline int Offset(const TfLiteIntArray* shape, int i0, int i1, int i2, int i3) {
//   //TFLITE_DCHECK_EQ(shape->size, 4);
  
//   //const int* dims_data = reinterpret_cast<const int*>(shape.DimsDataUpTo5D());
//   const int* dims_data = shape->data;
//   // TFLITE_DCHECK((dims_data[0] == 0 && i0 == 0) ||
//   //               (i0 >= 0 && i0 < dims_data[0]));
//   // TFLITE_DCHECK((dims_data[1] == 0 && i1 == 0) ||
//   //               (i1 >= 0 && i1 < dims_data[1]));
//   // TFLITE_DCHECK((dims_data[2] == 0 && i2 == 0) ||
//   //               (i2 >= 0 && i2 < dims_data[2]));
//   // TFLITE_DCHECK((dims_data[3] == 0 && i3 == 0) ||
//   //               (i3 >= 0 && i3 < dims_data[3]));
//   return ((i0 * dims_data[1] + i1) * dims_data[2] + i2) * dims_data[3] + i3;
// }

static inline void DepthwiseConv(
    const DepthwiseParams* params, const TfLiteIntArray* input_shape,
    const float* input_data, const TfLiteIntArray* filter_shape,
    const float* filter_data, const TfLiteIntArray* bias_shape,
    const float* bias_data, const TfLiteIntArray* output_shape,
    float* output_data) {
  const int stride_width = params->stride_width;
  const int stride_height = params->stride_height;
  const int dilation_width_factor = params->dilation_width_factor;
  const int dilation_height_factor = params->dilation_height_factor;
  const int pad_width = params->padding_values.width;
  const int pad_height = params->padding_values.height;
  const int depth_multiplier = params->depth_multiplier;
  const float output_activation_min = params->float_activation_min;
  const float output_activation_max = params->float_activation_max;
  // TFLITE_DCHECK_EQ(input_shape->size, 4);
  // TFLITE_DCHECK_EQ(filter_shape->size, 4);
  // TFLITE_DCHECK_EQ(output_shape->size, 4);

  
  const int batches       = input_shape->data[0] < output_shape->data[0] ? input_shape->data[0]: output_shape->data[0];
  const int output_depth  = filter_shape->data[3] < output_shape->data[3] ? filter_shape->data[3]: output_shape->data[3];
  //const int batches = MatchingDim(input_shape, 0, output_shape, 0);
  //const int output_depth = MatchingDim(filter_shape, 3, output_shape, 3);
  const int input_height = input_shape->data[1];
  const int input_width = input_shape->data[2];
  const int input_depth = input_shape->data[3];
  const int filter_height = filter_shape->data[1];
  const int filter_width = filter_shape->data[2];
  const int output_height = output_shape->data[1];
  const int output_width = output_shape->data[2];

  // TFLITE_DCHECK_EQ(output_depth, input_depth * depth_multiplier);
  // TFLITE_DCHECK_EQ(bias_shape.FlatSize(), output_depth);

  for (int b = 0; b < batches; ++b) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int ic = 0; ic < input_depth; ++ic) {
          for (int m = 0; m < depth_multiplier; m++) {
            const int oc = m + ic * depth_multiplier;
            const int in_x_origin = (out_x * stride_width) - pad_width;
            const int in_y_origin = (out_y * stride_height) - pad_height;
            float total = 0.f;
            for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
              for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                const int in_x = in_x_origin + dilation_width_factor * filter_x;
                const int in_y =
                    in_y_origin + dilation_height_factor * filter_y;
                // If the location is outside the bounds of the input image,
                // use zero as a default value.
                if ((in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height)) {
                  float input_value =
                      input_data[Offset(input_shape, b, in_y, in_x, ic)];
                  float filter_value = filter_data[Offset(
                      filter_shape, 0, filter_y, filter_x, oc)];
                  total += (input_value * filter_value);
                }
              }
            }
            float bias_value = 0.0f;
            if (bias_data) {
              bias_value = bias_data[oc];
            }
            output_data[Offset(output_shape, b, out_y, out_x, oc)] = total + bias_value;
                // ActivationFunctionWithMinMax(total + bias_value,
                //                              output_activation_min,
                //                              output_activation_max);
          }
        }
      }
    }
  }
}




TfLiteStatus EvalFloat(TfLiteContext* context, TfLiteNode* node,
                       TfLiteDepthwiseConvParams* params, Depthwise_Conv_OpData* data,
                       const TfLiteTensor* input, const TfLiteTensor* filter,
                       const TfLiteTensor* bias, TfLiteTensor* output) {
  float output_activation_min, output_activation_max;
  // CalculateActivationRange(params->activation, &output_activation_min,
  //                          &output_activation_max);

  DepthwiseParams op_params;
  op_params.padding_type = kValid;
  // op_params.padding_values.width = data->padding.width;
  // op_params.padding_values.height = data->padding.height;
  op_params.padding_values.width = 0;
  op_params.padding_values.height = 0;
  
  op_params.stride_width = params->stride_width;
  op_params.stride_height = params->stride_height;
  op_params.dilation_width_factor = params->dilation_width_factor;
  op_params.dilation_height_factor = params->dilation_height_factor;
  op_params.depth_multiplier = params->depth_multiplier;


  // op_params.float_activation_min = output_activation_min;
  // op_params.float_activation_max = output_activation_max;
  // TF_LITE_ENSURE_STATUS(ComputeDepthMultiplier(context, input, filter,&op_params.depth_multiplier));
    DepthwiseConv(
        &op_params, GetTensorShape(input), GetTensorData(input),
        GetTensorShape(filter), GetTensorData(filter),
        GetTensorShape(bias), GetTensorData(bias),
        GetTensorShape(output), GetTensorData(output));
  
  return kTfLiteOk;
}


TfLiteStatus EvalImpl(TfLiteContext* context, TfLiteNode* node) {

  TfLiteDepthwiseConvParams * params = (TfLiteDepthwiseConvParams *)node->builtin_data;
  TfLiteTensor* input  = GetInput(context, node, kInputTensor);
  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);
  TfLiteTensor* filter = GetInput(context, node, kFilterTensor);
  TfLiteTensor* bias =(NumInputs(node) == 3) ? GetInput(context, node, kBiasTensor) : NULL;
   Depthwise_Conv_OpData * data = (Depthwise_Conv_OpData*)(node->user_data);

  switch (input->type) {  // Already know in/out types are same.
    case kTfLiteFloat32:
      if (filter->type == kTfLiteFloat32) {
          return EvalFloat(context, node, params, data, input,filter, bias, output);
      } else {
        TF_LITE_KERNEL_LOG(
            context, "Type %s with filter type %s not currently supported.",
            TfLiteTypeGetName(input->type), TfLiteTypeGetName(filter->type));
        return kTfLiteError;
      }
      break;
    case kTfLiteUInt8:
      return kTfLiteError;
      break;
    case kTfLiteInt8:
      return kTfLiteError;
      break;
    case kTfLiteInt16:
      return kTfLiteError;
      break;
    default:
      return kTfLiteError;
  }
}

void* DEPTHWISE_CONV_2D_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void DEPTHWISE_CONV_2D_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus DEPTHWISE_CONV_2D_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}



TfLiteStatus DEPTHWISE_CONV_2D_Eval(TfLiteContext* context, TfLiteNode* node) {
  LOGE("DEPTHWISE_CONV_2D_Eval\n"); 
  EvalImpl(context,node);
  return kTfLiteOk;
}



TfLiteRegistration* Register_DEPTHWISE_CONV_2D() {
  static TfLiteRegistration r = {DEPTHWISE_CONV_2D_Init, DEPTHWISE_CONV_2D_Free, DEPTHWISE_CONV_2D_Prepare,
                                 DEPTHWISE_CONV_2D_Eval};
  return &r;
}
