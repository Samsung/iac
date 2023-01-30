// This file has three implementation of Add.
#include "kernel.h"

typedef struct Conv_OpData {
  int temp; // added for fixing building error.
}Conv_OpData;


typedef struct ConvParams {
  PaddingType padding_type;
  PaddingValues padding_values;
  // TODO(starka): This was just "stride", so check that width+height is OK.
  int16_t stride_width;
  int16_t stride_height;
  int16_t dilation_width_factor;
  int16_t dilation_height_factor;
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
}ConvParams;

void* CONV2D_Init(TfLiteContext* context, const char* buffer, size_t length) {
	//Opdata data;//void* data = new OpData;
	//return &data;
	return NULL;
}

void CONV2D_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  //free( (OpData*)buffer);
}

TfLiteStatus CONV2D_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}


static inline void Conv(const ConvParams* params, const TfLiteIntArray* input_shape,
                 const float* input_data, const TfLiteIntArray* filter_shape,
                 const float* filter_data, const TfLiteIntArray* bias_shape,
                 const float* bias_data, const TfLiteIntArray* output_shape,
                 float* output_data, const TfLiteIntArray* im2col_shape,
                 float* im2col_data) {
  const int stride_width = params->stride_width;
  const int stride_height = params->stride_height;
  const int dilation_width_factor = params->dilation_width_factor;
  const int dilation_height_factor = params->dilation_height_factor;
  const int pad_width = params->padding_values.width;
  const int pad_height = params->padding_values.height;
  const float output_activation_min = params->float_activation_min;
  const float output_activation_max = params->float_activation_max;
  // TFLITE_DCHECK_EQ(input_shape.DimensionsCount(), 4);
  // TFLITE_DCHECK_EQ(filter_shape.DimensionsCount(), 4);
  // TFLITE_DCHECK_EQ(output_shape.DimensionsCount(), 4);

  (void)im2col_data;   // only used in optimized code.
  (void)im2col_shape;  // only used in optimized code.
  const int batches = MatchingDim(input_shape, 0, output_shape, 0);
  const int input_depth = MatchingDim(input_shape, 3, filter_shape, 3);
  const int output_depth = MatchingDim(filter_shape, 0, output_shape, 3);

  // if (bias_data) {
  //   TFLITE_DCHECK_EQ(bias_shape.FlatSize(), output_depth);
  // }
  const int input_height = input_shape->data[1];
  const int input_width = input_shape->data[2];
  const int filter_height = filter_shape->data[1];
  const int filter_width = filter_shape->data[2];
  const int output_height = output_shape->data[1];
  const int output_width = output_shape->data[2];
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      const int in_y_origin = (out_y * stride_height) - pad_height;
      for (int out_x = 0; out_x < output_width; ++out_x) {
        const int in_x_origin = (out_x * stride_width) - pad_width;
        for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
          float total = 0.f;
          for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
            const int in_y = in_y_origin + dilation_height_factor * filter_y;
            for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
              const int in_x = in_x_origin + dilation_width_factor * filter_x;

              // Zero padding by omitting the areas outside the image.
              const bool is_point_inside_image =
                  (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                  (in_y < input_height);

              if (!is_point_inside_image) {
                continue;
              }

              for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                float input_value = input_data[Offset(input_shape, batch, in_y,
                                                      in_x, in_channel)];
                float filter_value = filter_data[Offset(
                    filter_shape, out_channel, filter_y, filter_x, in_channel)];
                total += (input_value * filter_value);
              }
            }
          }
          float bias_value = 0.0f;
          if (bias_data) {
            bias_value = bias_data[out_channel];
          }
          output_data[Offset(output_shape, batch, out_y, out_x, out_channel)] = total + bias_value;
              // ActivationFunctionWithMinMax(total + bias_value,
              //                              output_activation_min,
              //                              output_activation_max);
        }
      }
    }
  }
}

void CONV2D_EvalFloat(TfLiteContext* context, TfLiteNode* node,
               TfLiteConvParams* params, Conv_OpData* data,
               const TfLiteTensor* input, const TfLiteTensor* filter,
               const TfLiteTensor* bias, TfLiteTensor* im2col,
               TfLiteTensor* hwcn_weights, TfLiteTensor* output) {

     KernelType effective_kernel_type = kReference;              
//   float output_activation_min, output_activation_max;
//   CalculateActivationRange(params->activation, &output_activation_min,
// //                            &output_activation_max);
//   KernelType effective_kernel_type = kernel_type;
//   // Fall back to the optimized path if multi-threaded conv is unsupported.
//   if ((kernel_type == kMultithreadOptimized) &&
//       !data->supports_multithreaded_kernel) {
//     effective_kernel_type = kGenericOptimized;
//   }

//   // When im2col is needed (which is implied when 'im2col_oversized' is true),
//   // the GEMMM-based optimized path requires im2col data be allocated to ensure
//   // the correctness. Therefore, when im2col is disabled because of the
//   // oversized temporary im2col tensor, fallback to a non-optimized path is
//   // needed.
//   // See b/178743262 for the detailed motivation.
//   if (data->im2col_oversized) {
//     effective_kernel_type = kReference;
// #if defined(TFLITE_WITH_MULTITHREADED_EIGEN)
//     // As detailed by tflite::multithreaded_ops::Conv implementation in
//     // multithreaded_conv.h, the Eigen-based execution doesn't need im2col data.
//     // Therefore, we could rely on it as a better-optimized fallback than the
//     // reference one.
//     if (data->supports_multithreaded_kernel) {
//       effective_kernel_type = kMultithreadOptimized;
//     }
// #endif
//   }

  ConvParams op_params;
  op_params.padding_type = params->padding;
  op_params.padding_values.width = 0;
  op_params.padding_values.height = 0;
  op_params.stride_width = params->stride_width;
  op_params.stride_height = params->stride_height;
  op_params.dilation_width_factor = params->dilation_width_factor;
  op_params.dilation_height_factor = params->dilation_height_factor;
  //op_params.float_activation_min = output_activation_min;
  //op_params.float_activation_max = output_activation_max;
  switch (effective_kernel_type) {
    case kReference: {
      Conv(&op_params, GetTensorShape(input),
                          GetTensorData(input), GetTensorShape(filter),
                          GetTensorData(filter), GetTensorShape(bias),
                          GetTensorData(bias), GetTensorShape(output),
                          GetTensorData(output), GetTensorShape(im2col),
                          GetTensorData(im2col));
      break;
    }
    default:
      break;
  }
}



TfLiteStatus Conv2D_EvalImpl(TfLiteContext* context, TfLiteNode* node) {
  TfLiteConvParams*  params = (TfLiteConvParams*)(node->builtin_data);
  //OpData* data = reinterpret_cast<OpData*>(node->user_data);

  
  TfLiteTensor* output = GetOutput(context, node, 0);

  const TfLiteTensor* input = GetInput(context, node, 0);
  const TfLiteTensor* filter = GetInput(context, node, 1);
  bool has_bias = node->inputs->size == 3;
  const TfLiteTensor* bias = has_bias ? GetInput(context, node, 2) : NULL;
  // TfLiteTensor* im2col =data->need_im2col? &context->tensors[node->temporaries->data[data->im2col_index]]: NULL;
  // TfLiteTensor* hwcn_weights =data->need_hwcn_weights? &context->tensors[node->temporaries->data[data->hwcn_weights_index]]: NULL;
  TfLiteTensor* im2col = NULL;
  TfLiteTensor* hwcn_weights = NULL;

  // if (data->need_hwcn_weights && !data->have_weights_been_transposed) {
  //   TransposeFloatTensor(filter, hwcn_weights);
  //   data->have_weights_been_transposed = true;
  //}

  //TFLITE_DCHECK_EQ(input_type, input->type);
  switch(input->type) {  // Already know in/outtypes are same.
    case kTfLiteFloat32:
      // if (filter->type == kTfLiteUInt8 || filter->type == kTfLiteInt8) {
      //   if (data->is_hybrid_per_channel) {
      //     TF_LITE_ENSURE_OK(context, EvalHybridPerChannel<kernel_type>(
      //                                    context, node, params, data, input,
      //                                    filter, bias, im2col, output));
      //   } else {
      //     TfLiteTensor* accum_scratch =
      //         &context->tensors[node->temporaries
      //                               ->data[data->accum_scratch_index]];
      //     TF_LITE_ENSURE_OK(context,
      //                       EvalHybrid<kernel_type>(context, node, params, data,
      //                                               input, filter, bias, im2col,
      //                                               accum_scratch, output));
      //   }
      // } else {
        CONV2D_EvalFloat(context, node, params, NULL/*data*/, input, filter, bias,
                               im2col, hwcn_weights, output);
      //}
      break;
    // case kTfLiteUInt8:
    //   EvalQuantized<kernel_type>(context, node, params, data, input, filter,
    //                              bias, im2col, output);
    //   break;
    // case kTfLiteInt8:
    //   EvalQuantizedPerChannel<kernel_type>(context, node, params, data, input,
    //                                        filter, bias, output, im2col);
    //   break;
    // case kTfLiteInt16:
    //   EvalQuantizedPerChannel16x8<kernel_type>(
    //       context, node, params, data, input, filter, bias, output, im2col);
    //   break;
    default:
      TF_LITE_KERNEL_LOG(context, "Type %s currently not supported.",
                         TfLiteTypeGetName(input->type));
      return kTfLiteError;
  }
  return kTfLiteOk;
}



TfLiteStatus CONV2D_Eval(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input = GetInput(context, node, 0);
  switch (input->type) {
    case kTfLiteFloat32:
      return Conv2D_EvalImpl(context, node);
    // case kTfLiteUInt8:
    //   return EvalImpl<kernel_type, kTfLiteUInt8>(context, node);
    // case kTfLiteInt8:
    //   return EvalImpl<kernel_type, kTfLiteInt8>(context, node);
    // case kTfLiteInt16:
    //   return EvalImpl<kernel_type, kTfLiteInt16>(context, node);
    default:
      TF_LITE_KERNEL_LOG(context, "Type %s not currently supported.",
                         TfLiteTypeGetName(input->type));
      return kTfLiteError;
  }



  LOGE("CONV2D_Eval\n");
  return kTfLiteOk;
}



TfLiteRegistration* Register_CONV2D() {
  static TfLiteRegistration r = {CONV2D_Init, CONV2D_Free, CONV2D_Prepare,
                                 CONV2D_Eval};
  return &r;
}
