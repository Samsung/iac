#include "kernel.h"

typedef struct StridedSliceParams {
  int8_t start_indices_count;
  int32_t start_indices[5];
  int8_t stop_indices_count;
  int32_t stop_indices[5];
  int8_t strides_count;
  int32_t strides[5];

  int16_t begin_mask;
  int16_t ellipsis_mask;
  int16_t end_mask;
  int16_t new_axis_mask;
  int16_t shrink_axis_mask;
}StridedSliceParams;


typedef TfLiteIntArray RuntimeShape;

typedef struct StridedSliceContext {

  const TfLiteStridedSliceParams* params;
  const TfLiteTensor* input;
  const TfLiteTensor* begin;
  const TfLiteTensor* end;
  const TfLiteTensor* strides;
  TfLiteTensor* output;

  // Equivalent input shape after adding axis according to new_axis_mask.
  TfLiteIntArray* effective_input_shape;
  int input_dims;
}StridedSliceContext;


StridedSliceParams BuildStridedSliceParams(StridedSliceContext* op_context) {
  StridedSliceParams op_params;

  // The ellipsis_mask and new_axis_mask in op_params are not used. Those masks
  // are processed here to update begin_mask, end_mask and the index range.
  op_params.begin_mask = 0;
  op_params.ellipsis_mask = 0;
  op_params.end_mask = 0;
  op_params.new_axis_mask = 0;
  op_params.shrink_axis_mask = 0;

  // Count indexes where the new_axis_mask is set but the ellipsis_mask is not.
  //const int begin_count = GetTensorShape(op_context->begin).Dims(0);
  const int begin_count = GetTensorShape(op_context->begin)->data[0];
  int num_add_axis = 0;
  for (int i = 0; i < begin_count; ++i) {
    if (!((1 << i) & op_context->params->ellipsis_mask) &&
        ((1 << i) & op_context->params->new_axis_mask)) {
      num_add_axis++;
    }
  }

  // Calculate the dims of input after adding new axises.
  const int effective_dims = op_context->input_dims + num_add_axis;

  // If begin, end and strides are not fully provided, it means Ellipsis should
  // be expanded to multiple dimensions (Ex: for spec [Ellipsis, 2] on a 3D
  // input, the Ellipsis should be applied for the first 2 dimensions). Besides,
  // If the new_axis_mask and the ellipsis_mask are set at the same index, the
  // new_axis_mask will have no effect.
  int effective_ellipsis_mask = 0, effective_new_axis_mask = 0;
  int ellipsis_start_idx = effective_dims, expanded_ellipsis = 0;
  for (int i = 0; i < effective_dims;) {
    if ((1 << i) & op_context->params->ellipsis_mask) {
      ellipsis_start_idx = i;
      int ellipsis_end_idx = MAX(
          i + 1,
          MIN(i + 1 + num_add_axis + op_context->input_dims - begin_count,
                   effective_dims));
      expanded_ellipsis = ellipsis_end_idx - ellipsis_start_idx - 1;

      // Set bit for effective_ellipsis_mask.
      for (; i < ellipsis_end_idx; ++i) {
        effective_ellipsis_mask |= (1 << i);
      }
      continue;
    }

    if ((1 << (i - expanded_ellipsis)) & op_context->params->new_axis_mask) {
      effective_new_axis_mask |= (1 << i);
    }
    ++i;
  }

  // Calculate effective_input_shape and its corresponding begin, end, strides.
  const int32_t* begin_data = GetTensorData(op_context->begin);
  const int32_t* end_data = GetTensorData(op_context->end);
  const int32_t* strides_data = GetTensorData(op_context->strides);
  const TfLiteIntArray* input_shape = GetTensorShape(op_context->input);
  int added_ellipsis = 0, added_axises = 0;
  op_context->effective_input_shape = TfLiteIntArrayCreate(effective_dims);
  
  for (int i = 0; i < effective_dims; ++i) {
    if ((1 << i) & effective_ellipsis_mask) {
      // If ellipsis_mask, set the begin_mask and end_mask at that index.
      added_ellipsis = MAX(0, i - ellipsis_start_idx);
      op_params.begin_mask |= (1 << i);
      op_params.end_mask |= (1 << i);
      op_params.strides[i] = 1;
    //   op_context->effective_input_shape.SetDim( 
    //       i, input_shape.Dims(i - added_axises));
    op_context->effective_input_shape->data[i] = input_shape->data[i-added_axises];

    } else if ((1 << i) & effective_new_axis_mask) {
      // If new_axis_mask is set, it is equivalent to adding a new dim of 1 to
      // input tensor. Store added shape to effective_input_shape.
      op_params.start_indices[i] = 0;
      op_params.stop_indices[i] = 1;
      op_params.strides[i] = 1;
      //op_context->effective_input_shape.SetDim(i, 1);
      op_context->effective_input_shape->data[i] = 1;
      added_axises++;
    } else if (i >= begin_count + expanded_ellipsis) {
      op_params.start_indices[i] = 0;
      op_params.stop_indices[i] = 0;
      op_params.strides[i] = 1;
      op_params.begin_mask |= (1 << i);
      op_params.end_mask |= (1 << i);
    //   op_context->effective_input_shape.SetDim(
    //       i, input_shape.Dims(i - added_axises));
      op_context->effective_input_shape->data[i] = input_shape->data[i - added_axises];
      
    } else {
      const int orig_idx = i - added_ellipsis;
      op_params.start_indices[i] = begin_data[orig_idx];
      op_params.stop_indices[i] = end_data[orig_idx];
      op_params.strides[i] = strides_data[orig_idx];
      if (op_context->params->begin_mask & (1 << orig_idx)) {
        op_params.begin_mask |= (1 << i);
      }
      if (op_context->params->end_mask & (1 << orig_idx)) {
        op_params.end_mask |= (1 << i);
      }
      if (op_context->params->shrink_axis_mask & (1 << orig_idx)) {
        op_params.shrink_axis_mask |= (1 << i);
      }
    //   op_context->effective_input_shape.SetDim(
    //       i, input_shape.Dims(i - added_axises));
      op_context->effective_input_shape->data[i] = input_shape->data[i - added_axises];    
    }
  }
  op_params.start_indices_count = effective_dims;
  op_params.stop_indices_count = effective_dims;
  op_params.strides_count = effective_dims;

  return op_params;
}

int Clamp(const int v, const int lo, const int hi) {
  //TFLITE_DCHECK(!(hi < lo));
  if (hi < v) return hi;
  if (v < lo) return lo;
  return v;
}

int StartForAxis(const StridedSliceParams* params,
                        const RuntimeShape* input_shape, int axis) {
  const auto begin_mask = params->begin_mask;
  const auto* start_indices = params->start_indices;
  const auto* strides = params->strides;
  //const int axis_size = input_shape.Dims(axis);
  const int axis_size = input_shape->data[axis];
  if (axis_size == 0) {
    return 0;
  }
  // Begin with the specified index.
  int start = start_indices[axis];

  // begin_mask override
  if (begin_mask & 1 << axis) {
    if (strides[axis] > 0) {
      // Forward iteration - use the first element. These values will get
      // clamped below (Note: We could have set them to 0 and axis_size-1, but
      // use lowest() and max() to maintain symmetry with StopForAxis())
      //start = std::numeric_limits<int>::lowest();
      start = 0;
    } else {
      // Backward iteration - use the last element.
      //start = std::numeric_limits<int>::max();
      start = 0x7FFFFFFF;    }
  }

  // Handle negative indices
  if (start < 0) {
    start += axis_size;
  }

  // Clamping
  if (strides[axis] > 0) {
    // Forward iteration
    start = Clamp(start, 0, axis_size);
  } else {
    // Backward iteration
    start = Clamp(start, -1, axis_size - 1);
  }

  return start;
}


int StopForAxis(const StridedSliceParams* params,
                       const RuntimeShape* input_shape, int axis,
                       int start_for_axis) {
  const auto end_mask = params->end_mask;
  const auto shrink_axis_mask = params->shrink_axis_mask;
  const auto* stop_indices = params->stop_indices;
  const auto* strides = params->strides;
  const int axis_size = input_shape->data[axis];
  if (axis_size == 0) {
    return 0;
  }

  // Begin with the specified index
  const bool shrink_axis = shrink_axis_mask & (1 << axis);
  int stop = stop_indices[axis];

  // When shrinking an axis, the end position does not matter (and can be
  // incorrect when negative indexing is used, see Issue #19260). Always use
  // start_for_axis + 1 to generate a length 1 slice, since start_for_axis has
  // already been adjusted for negative indices.
  if (shrink_axis) {
    return start_for_axis + 1;
  }

  // end_mask override
  if (end_mask & (1 << axis)) {
    if (strides[axis] > 0) {
      // Forward iteration - use the last element. These values will get
      // clamped below
      //stop = std::numeric_limits<int>::max();
      stop = 0x7FFFFFFF;
    } else {
      // Backward iteration - use the first element.
      //stop = std::numeric_limits<int>::lowest();
      stop = 0;
    }
  }

  // Handle negative indices
  if (stop < 0) {
    stop += axis_size;
  }

  // Clamping
  // Because the end index points one past the last element, we need slightly
  // different clamping ranges depending on the direction.
  if (strides[axis] > 0) {
    // Forward iteration
    stop = Clamp(stop, 0, axis_size);
  } else {
    // Backward iteration
    stop = Clamp(stop, -1, axis_size - 1);
  }

  return stop;
}


bool LoopCondition(int index, int stop, int stride) {
  // True when we have reached the end of an axis and should loop.
  return stride > 0 ? index >= stop : index <= stop;
}


void StridedSlicePadIndices(StridedSliceParams* p,
                                   int dim_count) {
  // Add indices and mask bits to fully include extra dimensions
//   TFLITE_CHECK_LE(dim_count, 5);
//   TFLITE_CHECK_GE(dim_count, p->start_indices_count);
//   TFLITE_CHECK_EQ(p->start_indices_count, p->stop_indices_count);
//   TFLITE_CHECK_EQ(p->stop_indices_count, p->strides_count);

  const int pad_count = dim_count - p->start_indices_count;

  // Pad indices at start, so move arrays by pad_count.
  for (int i = p->start_indices_count - 1; i >= 0; --i) {
    p->strides[i + pad_count] = p->strides[i];
    p->start_indices[i + pad_count] = p->start_indices[i];
    p->stop_indices[i + pad_count] = p->stop_indices[i];
  }
  for (int i = 0; i < pad_count; ++i) {
    p->start_indices[i] = 0;
    p->stop_indices[i] = 1;
    p->strides[i] = 1;
  }

  // Pad masks with 0s or 1s as required.
  p->shrink_axis_mask <<= pad_count;
  p->ellipsis_mask <<= pad_count;
  p->new_axis_mask <<= pad_count;
  p->begin_mask <<= pad_count;
  p->end_mask <<= pad_count;
  p->begin_mask |= (1 << pad_count) - 1;
  p->end_mask |= (1 << pad_count) - 1;

  p->start_indices_count = dim_count;
  p->stop_indices_count = dim_count;
  p->strides_count = dim_count;
}


void StridedSlice(const StridedSliceParams* op_params,
                  const RuntimeShape* unextended_input_shape,
                  const TfLiteTensor* input,
                  const RuntimeShape* unextended_output_shape,
                  TfLiteTensor* output) 
{
    StridedSliceParams* params_copy = op_params;

    // TFLITE_DCHECK_LE(unextended_input_shape.DimensionsCount(), 5);
    // TFLITE_DCHECK_LE(unextended_output_shape.DimensionsCount(), 5);
    // const RuntimeShape input_shape = TfLiteIntArrayCreateWithData
    //   RuntimeShape::ExtendedShape(5, unextended_input_shape);
    // const RuntimeShape output_shape =
    //   RuntimeShape::ExtendedShape(5, unextended_output_shape);

    RuntimeShape* input_shape = TfLiteIntArrayCreate(5);
    int input_ndims = unextended_input_shape->size;
    for(int i=0;i<5;i++)
    {
        if(i<(5-input_ndims)){
            input_shape->data[i] = 1;    
        }else{
            input_shape->data[i] = unextended_input_shape->data[i+input_ndims -5];
        }
        
    }

    RuntimeShape* output_shape= TfLiteIntArrayCreate(5);
    int output_ndims = unextended_output_shape->size;
    for(int i=0;i<5;i++)
    {
        if(i<(5-output_ndims))
        {
            output_shape->data[i] = 1;
        }else{
            output_shape->data[i] = unextended_output_shape->data[i+output_ndims - 5];
        }
    }

    StridedSlicePadIndices(params_copy, 5);

    const int start_0 = StartForAxis(params_copy, input_shape, 0);
    const int stop_0 = StopForAxis(params_copy, input_shape, 0, start_0);
    const int start_1 = StartForAxis(params_copy, input_shape, 1);
    const int stop_1 = StopForAxis(params_copy, input_shape, 1, start_1);
    const int start_2 = StartForAxis(params_copy, input_shape, 2);
    const int stop_2 = StopForAxis(params_copy, input_shape, 2, start_2);
    const int start_3 = StartForAxis(params_copy, input_shape, 3);
    const int stop_3 = StopForAxis(params_copy, input_shape, 3, start_3);
    const int start_4 = StartForAxis(params_copy, input_shape, 4);
    const int stop_4 = StopForAxis(params_copy, input_shape, 4, start_4);


    float *input_data_ = input->data.f;
    float *output_ptr_ = output->data.f;

      for (int offset_0 = start_0 * input_shape->data[1],
           end_0 = stop_0 * input_shape->data[1],
           step_0 = params_copy->strides[0] * input_shape->data[1];
       !LoopCondition(offset_0, end_0, params_copy->strides[0]);
       offset_0 += step_0) {
    for (int offset_1 = (offset_0 + start_1) * input_shape->data[2],
             end_1 = (offset_0 + stop_1) * input_shape->data[2],
             step_1 = params_copy->strides[1] * input_shape->data[2];
         !LoopCondition(offset_1, end_1, params_copy->strides[1]);
         offset_1 += step_1) {
      for (int offset_2 = (offset_1 + start_2) * input_shape->data[3],
               end_2 = (offset_1 + stop_2) * input_shape->data[3],
               step_2 = params_copy->strides[2] * input_shape->data[3];
           !LoopCondition(offset_2, end_2, params_copy->strides[2]);
           offset_2 += step_2) {
        for (int offset_3 = (offset_2 + start_3) * input_shape->data[4],
                 end_3 = (offset_2 + stop_3) * input_shape->data[4],
                 step_3 = params_copy->strides[3] * input_shape->data[4];
             !LoopCondition(offset_3, end_3, params_copy->strides[3]);
             offset_3 += step_3) {
          for (int offset_4 = offset_3 + start_4, end_4 = offset_3 + stop_4;
               !LoopCondition(offset_4, end_4, params_copy->strides[4]);
               offset_4 += params_copy->strides[4]) {
            //writer->Write(offset_4);
            *output_ptr_++ = input_data_[offset_4];
          }
        }
      }
    }
  }
  if (input_shape)
  {
    TfLiteIntArrayFree(input_shape);
    input_shape = NULL;
  }
  if (output_shape)
  {
    TfLiteIntArrayFree(output_shape);
    output_shape = NULL;
  }
}

void* Strided_Slice_Init(TfLiteContext* context, const char* buffer, size_t length)
{
  return NULL;// should return one value
}

// The pointer `buffer` is the data previously returned by an init invocation.
void Strided_Slice_Free(TfLiteContext* context, void* buffer)
{

}

// prepare is called when the inputs this node depends on have been resized.
// context->ResizeTensor() can be called to request output tensors to be
// resized.
//
// Returns kTfLiteOk on success.
TfLiteStatus Strided_Slice_Prepare(TfLiteContext* context, TfLiteNode* node)
{
    
}

// Execute the node (should read node->inputs and output to node->outputs).
// Returns kTfLiteOk on success.
TfLiteStatus Strided_Slice_Eval(TfLiteContext* context, TfLiteNode* node)
{
     int kInputTensor = 0;
     int kBeginTensor = 1;
     int kEndTensor = 2;
     int kStridesTensor = 3;
     int kOutputTensor = 0;

     StridedSliceContext op_context;
     memset(&op_context, 0x00, sizeof(op_context));

    op_context.params = (TfLiteStridedSliceParams*)(node->builtin_data);
    op_context.input   = GetInput(context, node, kInputTensor);
    op_context.begin   = GetInput(context, node, kBeginTensor);
    op_context.end     = GetInput(context, node, kEndTensor);
    op_context.strides = GetInput(context, node, kStridesTensor);
    op_context.output  = GetOutput(context, node, kOutputTensor);

    op_context.input_dims = NumDimensions(op_context.input);

    StridedSliceParams op_params = BuildStridedSliceParams(&op_context);


    StridedSlice(&op_params,op_context.effective_input_shape,op_context.input,GetTensorShape(op_context.output),op_context.output);

    if (op_context.effective_input_shape) // memory leak,need to free
    {
      TfLiteIntArrayFree(op_context.effective_input_shape);
      op_context.effective_input_shape = NULL;
    }
}


TfLiteRegistration* Register_STRIDED_SLICE() {
static TfLiteRegistration r = {Strided_Slice_Init, Strided_Slice_Free, Strided_Slice_Prepare,
                                Strided_Slice_Eval};
return &r;
}
