#include "kernel.h"

typedef enum FusedActivationFunctionType{
  kNone1,
  kRelu6,
  kRelu1,
  kRelu
}FusedActivationFunctionType;

typedef struct PoolParams {
  FusedActivationFunctionType activation;
  PaddingType padding_type;
  PaddingValues padding_values;
  int stride_height;
  int stride_width;
  int filter_height;
  int filter_width;
  // uint8_t, etc, activation params.
  int32_t quantized_activation_min;
  int32_t quantized_activation_max;
  // float activation params.
  float float_activation_min;
  float float_activation_max;
}PoolParams;


static inline void MaxPool(const PoolParams* params, const TfLiteIntArray* input_shape,
                    const float* input_data, const TfLiteIntArray* output_shape,
                    float* output_data) {
  // TFLITE_DCHECK_EQ(input_shape.DimensionsCount(), 4);
  // TFLITE_DCHECK_EQ(output_shape.DimensionsCount(), 4);
  const int batches = MatchingDim(input_shape, 0, output_shape, 0);
  const int depth = MatchingDim(input_shape, 3, output_shape, 3);
  const int input_height = input_shape->data[1];
  const int input_width = input_shape->data[2];
  const int output_height = output_shape->data[1];
  const int output_width = output_shape->data[2];
  const int stride_height = params->stride_height;
  const int stride_width = params->stride_width;
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int channel = 0; channel < depth; ++channel) {
          const int in_x_origin =
              (out_x * stride_width) - params->padding_values.width;
          const int in_y_origin =
              (out_y * stride_height) - params->padding_values.height;
          // Compute the boundaries of the filter region clamped so as to
          // ensure that the filter window fits in the input array.
          const int filter_x_start = MAX(0, -in_x_origin);
          const int filter_x_end   =  MIN(params->filter_width, input_width - in_x_origin);
          const int filter_y_start = MAX(0, -in_y_origin);
          const int filter_y_end   =MIN(params->filter_height, input_height - in_y_origin);
          //float max = std::numeric_limits<float>::lowest();
          float max = -3.40282e+38;
          int32_t max_x = 0;
          int32_t max_y = 0;
          for (int filter_y = filter_y_start; filter_y < filter_y_end;
               ++filter_y) {
            for (int filter_x = filter_x_start; filter_x < filter_x_end;
                 ++filter_x) {
              const int in_x = in_x_origin + filter_x;
              const int in_y = in_y_origin + filter_y;
              //max = MAX(max,input_data[Offset(input_shape, batch, in_y, in_x, channel)]);
			  float cur =
                  input_data[Offset(input_shape, batch, in_y, in_x, channel)];
              if (cur > max) {
                max = cur;
                max_x = in_x;
                max_y = in_y;
              }
            }
          }
		  int32_t output_idx =Offset(output_shape, batch, out_y, out_x, channel);
          output_data[output_idx] = max;
        }
      }
    }
  }
}

void MaxEvalFloat(TfLiteContext* context, TfLiteNode* node,
                  TfLitePoolParams* params, void* data,
                  const TfLiteTensor* input, TfLiteTensor* output) {
  //float activation_min, activation_max;
  // CalculateActivationRange(params->activation, &activation_min,
  //                          &activation_max);

  PoolParams op_params;
  op_params.stride_height = params->stride_height;
  op_params.stride_width = params->stride_width;
  op_params.filter_height = params->filter_height;
  op_params.filter_width = params->filter_width;
  op_params.padding_values.height = 0;
  op_params.padding_values.width = 0;
  // op_params.float_activation_min = activation_min;
  // op_params.float_activation_max = activation_max;
  MaxPool(&op_params, GetTensorShape(input), GetTensorData(input), \
                GetTensorShape(output), GetTensorData(output));
  // if (kernel_type == kReference) {
  //   TF_LITE_MAX_POOL(reference_ops);
  // } else {
  //   TF_LITE_MAX_POOL(optimized_ops);
  // }
}




TfLiteStatus MaxEval(TfLiteContext* context, TfLiteNode* node) {
  TfLitePoolParams* params = (TfLitePoolParams*)(node->builtin_data);
  //OpData* data = reinterpret_cast<OpData*>(node->user_data);

  TfLiteTensor* output = GetOutput(context, node, 0);
  const TfLiteTensor* input = GetInput(context, node, 0);
  switch (input->type) {  // Already know in/out types are same.
    case kTfLiteFloat32:
      MaxEvalFloat(context, node, params, NULL /*data*/, input, output);
      break;
    // case kTfLiteUInt8:
    //   MaxEvalQuantizedUInt8<kernel_type>(context, node, params, data, input,
    //                                      output);
    //   break;
    // case kTfLiteInt8:
    //   MaxEvalQuantizedInt8<kernel_type>(context, node, params, data, input,
    //                                     output);
    //   break;
    // case kTfLiteInt16:
    //   MaxEvalQuantizedInt16<kernel_type>(context, node, params, data, input,
    //                                      output);
    //   break;
    default:
      TF_LITE_KERNEL_LOG(context, "Type %s not currently supported.",
                         TfLiteTypeGetName(input->type));
      return kTfLiteError;
  }
  return kTfLiteOk;
}

  void* Max_Pool_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Max_Pool_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Max_Pool_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
      
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Max_Pool_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      MaxEval(context,node);
      LOGE("Max_Pool_Eval\n");

  }

  TfLiteRegistration* Register_MAX_POOL() {
  static TfLiteRegistration r = {Max_Pool_Init, Max_Pool_Free, Max_Pool_Prepare,
                                 Max_Pool_Eval};
  return &r;
}
