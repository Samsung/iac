#include "kernel.h"

typedef struct ConcatenationParams {
  int8_t axis;
  const int32_t* input_zeropoint;
  const float* input_scale;
  uint16_t inputs_count;
  int32_t output_zeropoint;
  float output_scale;
}ConcatenationParams;

typedef TfLiteIntArray RuntimeShape;

inline void Concatenation(ConcatenationParams* params,
                          RuntimeShape* input_shapes,
                          float* input_data,
                          RuntimeShape* output_shape,
                          float* output_data) {
  int axis = params->axis;
  int inputs_count = params->inputs_count;
  //const int concat_dimensions = output_shape.DimensionsCount();
  const int concat_dimensions = output_shape->size;
  
  //

  int64_t concat_size = 0;
  for (int i = 0; i < inputs_count; i++) {
    //TFLITE_DCHECK_EQ(input_shapes[i]->DimensionsCount(), concat_dimensions);
    for (int j = 0; j < concat_dimensions; j++) {
      if (j != axis) {
        //MatchingDim(*input_shapes[i], j, output_shape, j);
        MatchingDim(input_shapes+i, j, output_shape, j);
      }
    }
    //concat_size += input_shapes[i]->Dims(axis);
    concat_size += (input_shapes+i)->data[axis];
  }
  //TFLITE_DCHECK_EQ(concat_size, output_shape.Dims(axis));
  int64_t outer_size = 1;
  for (int i = 0; i < axis; ++i) {
    //outer_size *= output_shape.Dims(i);
      outer_size *= output_shape->data[i];
  }
  // For all input arrays,
  // FlatSize() = outer_size * Dims(axis) * base_inner_size;
  int64_t base_inner_size = 1;
  for (int i = axis + 1; i < concat_dimensions; ++i) {
    //base_inner_size *= output_shape.Dims(i);
    base_inner_size *= output_shape->data[i];
  }

  //Scalar* output_ptr = output_data;
  float* output_ptr = output_data;
  for (int k = 0; k < outer_size; k++) {
    for (int i = 0; i < inputs_count; ++i) {
      //const int copy_size = input_shapes[i]->Dims(axis) * base_inner_size;
      const int copy_size = (input_shapes+i)->data[axis] * base_inner_size;
      //const Scalar* input_ptr = input_data[i] + k * copy_size;
      //const float* input_ptr = input_data[i] + k * copy_size;
      //memcpy(output_ptr, input_ptr, copy_size * sizeof(Scalar));
      output_ptr += copy_size;
    }
  }
}

  void* Concatenation_Init(TfLiteContext* context, const char* buffer, size_t length)
  {
    return NULL;// should return one value
  }

  // The pointer `buffer` is the data previously returned by an init invocation.
  void Concatenation_Free(TfLiteContext* context, void* buffer)
  {

  }

  // prepare is called when the inputs this node depends on have been resized.
  // context->ResizeTensor() can be called to request output tensors to be
  // resized.
  //
  // Returns kTfLiteOk on success.
  TfLiteStatus Concatenation_Prepare(TfLiteContext* context, TfLiteNode* node)
  {
    return kTfLiteOk;      
  }

  // Execute the node (should read node->inputs and output to node->outputs).
  // Returns kTfLiteOk on success.
  TfLiteStatus Concatenation_Eval(TfLiteContext* context, TfLiteNode* node)
  {
      TfLiteConcatenationParams* params = (TfLiteConcatenationParams*)(node->builtin_data);
      int axis = params->axis;      
      
      int input_num = node->inputs->size;

      TfLiteTensor* output = GetOutput(context, node, 0);

      if (axis < 0) axis += output->dims->size;

      float * output_ptr = output->data.f;
      for(int i=0;i<input_num;i++){
          TfLiteTensor* input = GetInput(context,node,i);
          int size = NumElements(input);
          for(int j=0;j<size;j++){
              *output_ptr++ = input->data.f[j];
          }
      }
      return kTfLiteOk;
  }

  TfLiteRegistration* Register_CONCATENATION() {
  static TfLiteRegistration r = {Concatenation_Init, Concatenation_Free, Concatenation_Prepare,
                                 Concatenation_Eval};
  return &r;
}
