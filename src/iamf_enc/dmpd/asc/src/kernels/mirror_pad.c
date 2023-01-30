#include "kernel.h"
#include "assert.h"
const int kUnsetOffset = -1;

typedef struct MirrorPad_OpData {
  const TfLiteTensor* padding_matrix;
  const TfLiteIntArray* input_dims;
  const int* output_dims_num_elements;
  const int* input_dims_num_elements;
  const float* input_data;
  int offset;
  float* output_data;
  int num_dims;
}MirrorPad_OpData;

#define min(a,b) ((a)<(b))?(a):(b)

static inline int GetInputDimension(int padded_dimension, int left_pad, int right_pad,
                             int input_dim_size, int offset) {
  if (padded_dimension < left_pad) {
    const int original_ind = left_pad + offset - 1;
    return original_ind - (min(padded_dimension, original_ind - offset));
  }
  padded_dimension -= left_pad;
  if (padded_dimension >= input_dim_size) {
    padded_dimension -= input_dim_size;
    const int original_ind = input_dim_size - (1 + offset);
    return original_ind - (min(padded_dimension, original_ind));
  }
  return padded_dimension;
}

static inline void GetPadding(const int* data, int offset, int64_t* left_pad,
                       int64_t* right_pad) {
  *left_pad = (int64_t)(*(data + offset * 2));
  *right_pad =(int64_t)(*(data + offset * 2 + 1));
}


int GetFlatIndex(int index, MirrorPad_OpData* eval_data) {
  int flat_index = 0;
  int64_t left_pad = 0, right_pad = 0, dimension_index, index_in_input;
  for (int i = 0; i < eval_data->num_dims; ++i) {
    switch (eval_data->padding_matrix->type) {
      case kTfLiteInt32:
        GetPadding(eval_data->padding_matrix->data.i32, i, &left_pad,
                   &right_pad);
        break;
      case kTfLiteInt64:
        GetPadding(eval_data->padding_matrix->data.i64, i, &left_pad,
                   &right_pad);
        break;
      default:
        break;
    }
    dimension_index = index / (eval_data->output_dims_num_elements[i]);
    index_in_input =GetInputDimension(dimension_index, left_pad, right_pad,
                          eval_data->input_dims->data[i], eval_data->offset);
    flat_index += index_in_input * (eval_data->input_dims_num_elements[i]);
    index %= (eval_data->output_dims_num_elements[i]);
  }
  return flat_index;
}

void MirrorPadWorkerTask(MirrorPad_OpData* eval_data,int start, int end)
{
    float* input_data = eval_data->input_data;
    float* output_data = eval_data->output_data;
    for (int i = start; i < end; ++i) {
      output_data[i] = input_data[GetFlatIndex(i, eval_data)];
    }
}


void* MirrorPad_Init(TfLiteContext* context, const char* buffer, size_t length) {
	MirrorPad_OpData data;//void* data = new OpData;
	return NULL;
}


void MirrorPad_Free(TfLiteContext* context, void* buffer) {
  //delete reinterpret_cast<OpData*>(buffer);
  free( (MirrorPad_OpData*)buffer);
}

TfLiteStatus MirrorPad_Prepare(TfLiteContext* context, TfLiteNode* node) {
  return kTfLiteOk;
}

void MirrorPad_Eval(TfLiteContext* context, TfLiteNode* node) {

  const TfLiteTensor* input_tensor = GetInput(context, node, 0);
  const TfLiteTensor* padding_matrix = GetInput(context, node, 1);;

   TfLiteMirrorPaddingParams* params = (TfLiteMirrorPaddingParams*)(node->builtin_data);

   const int input_dims = NumDimensions(input_tensor);

  TfLiteTensor* output_tensor = GetOutput(context,node,0);

  int *output_dims_num_elements = (int*)malloc(input_dims*sizeof(int));
  int *input_dims_num_elements = (int*)malloc(input_dims*sizeof(int));
  if(!output_dims_num_elements||!input_dims_num_elements)goto FAILED;
  for(int i=0; i< input_dims;i++){
    output_dims_num_elements[i] = 1;
    input_dims_num_elements[i] = 1;
  }
  for (int i = input_dims - 2; i >= 0; i--) {
    output_dims_num_elements[i] =
        output_dims_num_elements[i + 1] * output_tensor->dims->data[i + 1];
    input_dims_num_elements[i] =
        input_dims_num_elements[i + 1] * input_tensor->dims->data[i + 1];
  }


  //const int offset = params->mode != kTfLiteMirrorPaddingReflect ? 0: 1;  
  const int offset = 0;  
  const int output_size = NumElements(output_tensor);

  
  MirrorPad_OpData eval_data;
  eval_data.input_data = GetTensorData(input_tensor);
  eval_data.input_dims = input_tensor->dims;
  eval_data.output_dims_num_elements = output_dims_num_elements;
  eval_data.input_dims_num_elements = input_dims_num_elements;
  eval_data.num_dims = input_dims;
  eval_data.offset = offset;
  eval_data.output_data = GetTensorData(output_tensor);
  eval_data.padding_matrix = padding_matrix;

  MirrorPadWorkerTask(&eval_data,0,output_size);

LOGE("MirrorPad_Eval\n");
FAILED:
  if (output_dims_num_elements)
    free(output_dims_num_elements);

  if (input_dims_num_elements)
    free(input_dims_num_elements);


}




TfLiteRegistration* Register_MIRROR_PAD() {
  static TfLiteRegistration r = {MirrorPad_Init, MirrorPad_Free, MirrorPad_Prepare,
                                 MirrorPad_Eval};
  return &r;
}
