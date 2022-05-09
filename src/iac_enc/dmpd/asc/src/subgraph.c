#include "common.h"
#include "subgraph.h"
#include "tflite_reader.h"
#include "util.h"

void  Init_Subgraph(Subgraph* subgraph)
{
	subgraph->tensors_size = 0;
	subgraph->nodes_and_registrations_size=0;
}

void Print_Subgraph(Subgraph* subgraph)
{
	printf("tensors_size = %d,nodes_and_registrations_size = %d \n",subgraph->tensors_size,subgraph->nodes_and_registrations_size);
	//print inputs outputs
	
	//print tensors
	for(int i=0;i< subgraph->tensors_size;i++)
	{
		printf("print tensor index = %d\n",i);
		Print_TfLiteTensor( &subgraph->tensors_[i]); //util.h
	}
	//print nodes;
	for(int i=0;i<subgraph->nodes_and_registrations_size;i++)
	{
		printf("op index  %d\n",subgraph->nodes_and_registrations_[i].registration.builtin_code);
	}
}



void Delete_Subgraph(Subgraph* subgraph)
{
	printf("Delete_Subgraph\n");

	//if(subgraph->name_) free(subgraph->name_) ; 
	//TfLiteContext context_ ; //=NULL
	
  if (subgraph->inputs_)
  {
    TfLiteIntArrayFree(subgraph->inputs_);
    subgraph->inputs_ = NULL;
  }
  if (subgraph->outputs_)
  {
    TfLiteIntArrayFree(subgraph->outputs_);
    subgraph->outputs_ = NULL;
  }
  if (subgraph->variables_)
  {
    TfLiteIntArrayFree(subgraph->variables_);
    subgraph->variables_ = NULL;
  }
	
  if (subgraph->tensors_)
  {
    for (int i = 0; i < subgraph->tensors_size; i++)
    {
      TfLiteTensor* tensor = &subgraph->context_.tensors[i];
      TfLiteTensorFree(tensor);
    }
    free(subgraph->tensors_);
    subgraph->tensors_ = NULL;
  }
  if (subgraph->nodes_and_registrations_)
  {
    for (int i = 0; i < subgraph->nodes_and_registrations_size; i++)
    {
      TfLiteNode* node = &(subgraph->nodes_and_registrations_[i]);
      if (node->inputs)
      {
        TfLiteIntArrayFree(node->inputs);
        node->inputs = NULL;
      }
      if (node->outputs)
      {
        TfLiteIntArrayFree(node->outputs);
        node->outputs = NULL;
      }
      if (node->intermediates)
      {
        TfLiteIntArrayFree(node->intermediates);
        node->intermediates = NULL;
      }
      if (node->temporaries)
      {
        TfLiteIntArrayFree(node->temporaries);
        node->temporaries = NULL;
      }
      if (node->user_data)
      {
        free(node->user_data);
        node->user_data = NULL;
      }
      if (node->builtin_data)
      {
        free(node->builtin_data);
        node->builtin_data = NULL;
      }
    }
    free(subgraph->nodes_and_registrations_);
    subgraph->nodes_and_registrations_ = NULL;
  }
}

TfLiteStatus SetInputs(Subgraph* subgraph , int* inputs, int size)
{
	printf("Subgraph::SetInputs\n");
	if(size <=0)
	{
		printf("SetInputs size is %d\n",size);
		return kTfLiteError;
	}
	
  if (!subgraph->inputs_)
  {
		TfLiteIntArrayFree(subgraph->inputs_);
    subgraph->inputs_ = NULL;
  }

#if 0	 // there is memory leak , comment firslty
	TfLiteIntArray* intArray = TfLiteIntArrayCreate(size);
	for(int i=0;i<size;i++)
		intArray->data[i] = inputs[i];
#endif
	return kTfLiteOk;
}
 
TfLiteStatus SetOutputs(Subgraph* subgraph , int* outputs, int size )
{
	printf("Subgraph::SetOutputs\n");
	if(size <=0)
	{
		printf("SetOutputs size is %d\n",size);
		return kTfLiteError;
	}
	
  if (!subgraph->outputs_)
  {
		TfLiteIntArrayFree(subgraph->outputs_);
    subgraph->outputs_ = NULL;
  }

#if 0	 // there is memory leak , comment firslty	
	TfLiteIntArray* intArray = TfLiteIntArrayCreate(size);
	for(int i=0;i<size;i++)
		intArray->data[i] = outputs[i];
#endif
	return kTfLiteOk;
}
TfLiteStatus SetVariables(Subgraph* subgraph ,int* variables, int size )
{
	if(size <=0)
	{
		printf("SetInputs size is %d\n",size);
		return kTfLiteError;
	}
	
  if (!subgraph->variables_)
  {
		TfLiteIntArrayFree(subgraph->variables_);
    subgraph->variables_ = NULL;
  }

	
	TfLiteIntArray* intArray = TfLiteIntArrayCreate(size);
	for(int i=0;i<size;i++)
		intArray->data[i] = variables[i];

	return kTfLiteOk;
}


TfLiteIntArray* getInputs(Subgraph* subgraph )  { return subgraph->inputs_ ; }

TfLiteIntArray* getOutputs(Subgraph* subgraph )  { return subgraph->outputs_ ; }

TfLiteIntArray* getVariables(Subgraph* subgraph)  { return subgraph->variables_ ; }

TfLiteTensor* tensor(Subgraph* subgraph, size_t tensor_index) {
    if (tensor_index < 0 || tensor_index >= subgraph->context_.tensors_size) {
      return NULL;
    }
    return &subgraph->context_.tensors[tensor_index];
}
 
size_t tensors_size(Subgraph* subgraph)  { return subgraph->tensors_size; }

 
size_t nodes_size(Subgraph* subgraph) { return subgraph->nodes_and_registrations_size; }
 
TfLiteTensor* tensors(Subgraph* subgraph) { return subgraph->tensors_; }

TfLiteContext* context(Subgraph* subgraph) { return &subgraph->context_; }


// Init,Prepare,Invoke,Free the given 'node' for execution.
void* OpInit(Subgraph* subgraph,const TfLiteRegistration* op_reg, const char* buffer,size_t length) {
    if (op_reg->init == NULL)
		return NULL;
    return op_reg->init(&subgraph->context_, buffer, length);
}

void OpFree(Subgraph* subgraph,const TfLiteRegistration* op_reg, void* buffer) {
    if (op_reg->free == NULL) return;
    if (buffer) {
      op_reg->free(&subgraph->context_, buffer);
    }
  }

TfLiteStatus OpPrepare(Subgraph* subgraph,const TfLiteRegistration* op_reg, TfLiteNode* node)
{
	if (op_reg->prepare == NULL) {
		return kTfLiteOk;
	}
	return op_reg->prepare(&subgraph->context_, node);
}


TfLiteStatus OpInvoke(Subgraph* subgraph,const TfLiteRegistration* op_reg, TfLiteNode* node) {
    if (op_reg->invoke == NULL) return kTfLiteError;
    return op_reg->invoke(&subgraph->context_, node);
  }

TfLiteStatus ReserveNodes(Subgraph* subgraph, int size)
{
	subgraph->nodes_and_registrations_size = size;
	
	if(subgraph->nodes_and_registrations_ != NULL){
		free(subgraph->nodes_and_registrations_);
	}
	subgraph->nodes_and_registrations_ = (NodeRegistration*)malloc(sizeof(NodeRegistration) * size);//TODO
  
  memset(subgraph->nodes_and_registrations_, 0x00, sizeof(NodeRegistration));
  for (int i = 0; i < size; i++)
  {
    NodeRegistration *nodes_and_registrations_ = &(subgraph->nodes_and_registrations_[i]);
    memset(&(nodes_and_registrations_->node), 0x00, sizeof(TfLiteNode));
    memset(&(nodes_and_registrations_->registration), 0x00, sizeof(TfLiteRegistration));
  }
}

									 
TfLiteStatus AddTensors(Subgraph* subgraph,int tensors_to_add)
                          //int* first_new_tensor_index )//first_new_tensor_index = NULL
{
	printf("Subgraph::AddTensors, ternsor size = %d\n",tensors_to_add);
	if(tensors_to_add <= 0)
		return kTfLiteOk;
	
	if(subgraph->tensors_ !=NULL)
	{
		free(subgraph->tensors_);
	}
	subgraph->tensors_size = tensors_to_add + subgraph->tensors_size ;
	subgraph->tensors_ = ( TfLiteTensor* )malloc( sizeof(TfLiteTensor) * subgraph->tensors_size );
	
	for (size_t i = 0; i < subgraph->tensors_size; i++) {
		memset(&subgraph->tensors_[i], 0, sizeof(subgraph->tensors_[i]));
		subgraph->tensors_[i].buffer_handle = kTfLiteNullBufferHandle;
		memset(&subgraph->tensors_[i].data, 0x00, sizeof(TfLitePtrUnion));
	}
	subgraph->context_.tensors = subgraph->tensors_;
	subgraph->context_.tensors_size = subgraph->tensors_size;

	return kTfLiteOk;
}
  
TfLiteStatus AddNodeWithParameters(Subgraph* subgraph,
									  int* inputs, int input_size,//const std::vector<int>& inputs,
                                     int* outputs, int output_size,//const std::vector<int>& outputs,
                                     int* intermediates, int inter_size,//const std::vector<int>& intermediates,
                                     const char* init_data,//always NULL
                                     size_t init_data_size, //always 0
									 void* builtin_data, //Parameters
                                     const TfLiteRegistration* registration,
                                     int node_index )
									 
{
	printf("Subgraph::AddNodeWithParameters node index=%d\n", node_index);
	if(registration == NULL)
		printf("Subgraph::AddNodeWithParameters error,node index=%d registration=NULL\n",node_index);
	/*
	std::unique_ptr<void, decltype(free)*> builtin_data_deleter(builtin_data,free);
	
	
	if (state_ == kStateInvokableAndImmutable) {
		ReportError("AddNodeWithParameters is disallowed when graph is immutable.");
		return kTfLiteError;
	}
	state_ = kStateUninvokable; 

	TF_LITE_ENSURE_OK(&context_, CheckTensorIndices("node inputs", inputs.data(),
													  inputs.size()));
	TF_LITE_ENSURE_OK(
		  &context_,
		  CheckTensorIndices("node outputs", outputs.data(), outputs.size()));

	int new_node_index = subgraph->nodes_and_registration_size;
	if (node_index) *node_index = new_node_index;
	
	nodes_and_registration_.resize(nodes_and_registration_.size() + 1);*/
	NodeRegistration* node_and_reg = subgraph->nodes_and_registrations_ + node_index;
	TfLiteNode* node = &node_and_reg->node;
	//TFliteNode& node = subgraph->nodes_and_registrations_[i]->node;

  // access_tflite_buffer() is executed once when load module, so can comment them
#if 0
  if (node->inputs)
  {
    TfLiteIntArrayFree(node->inputs);
    node->inputs = NULL;
  }
  if (node->outputs)
  {
    TfLiteIntArrayFree(node->outputs);
    node->outputs = NULL;
  }
  if (node->intermediates)
  {
    TfLiteIntArrayFree(node->intermediates);
    node->intermediates = NULL;
  }
  if (node->temporaries)
  {
    TfLiteIntArrayFree(node->temporaries);
    node->temporaries = NULL;
  }
  if (node->builtin_data)
  {
    free(node->builtin_data);
    node->builtin_data = NULL;
  }
#endif
	  // NOTE, here we are not using move semantics yet, since our internal
	  // representation isn't std::vector, but in the future we would like to avoid
	  // copies, so we want the interface to take r-value references now.
	  node->inputs = TfLiteIntArrayCreateWithData(input_size,inputs);
	  node->outputs = TfLiteIntArrayCreateWithData(output_size,outputs);
	  node->intermediates = TfLiteIntArrayCreateWithData(inter_size,intermediates);
	  node->temporaries = TfLiteIntArrayCreate(0);
	  if (init_data) {
		node->user_data = OpInit(subgraph,registration, init_data, init_data_size);
	  } else {
		node->user_data = OpInit(subgraph,registration, (const char*)builtin_data, 0);
	  }

	  node->builtin_data = builtin_data ;//builtin_data_deleter.release();
	  // TODO(ycling): Filling `custom_initial_data` and `custom_initial_data_size`
	  // properly for nodes generated by ReplaceNodeSubsetsWithDelegateKernels.

	  if (registration->builtin_code == tflite_BuiltinOperator_CUSTOM) {
		// When it's a CUSTOM op, the `custom_options` field in the Flatbuffer
		// `Operator` table is passed in.
		node->custom_initial_data = init_data;
		node->custom_initial_data_size = init_data_size;
	  } else {
		node->custom_initial_data = NULL;
		node->custom_initial_data_size = 0;
	  }

	  	  // Copying of registration is required to support unresolved custom ops.
	  node_and_reg->registration = *registration;
	  //execution_plan_.push_back(node_index);
	  
	  return kTfLiteOk;

}

//util functions
TfLiteQuantizationParams GetLegacyQuantization(
    const TfLiteQuantization quantization) {
  TfLiteQuantizationParams legacy_quantization;
  legacy_quantization.scale = 0;
  legacy_quantization.zero_point = 0;

  // If the quantization type isn't affine, return the empty
  // legacy_quantization.
  if (quantization.type != kTfLiteAffineQuantization) {
    return legacy_quantization;
  }

  TfLiteAffineQuantization* affine_quantization =
      (TfLiteAffineQuantization*)quantization.params;
  if (!affine_quantization || !affine_quantization->scale ||
      !affine_quantization->zero_point ||
      affine_quantization->scale->size != 1 ||
      affine_quantization->zero_point->size != 1) {
    return legacy_quantization;
  }

  // We know its per-layer quantization now.
  legacy_quantization.scale = affine_quantization->scale->data[0];
  legacy_quantization.zero_point = affine_quantization->zero_point->data[0];
  return legacy_quantization;
}

TfLiteStatus BytesRequired(Subgraph* subgraph,TfLiteType type, const int* dims,
                                     size_t dims_size, size_t* bytes) {
  TF_LITE_ENSURE(subgraph->context_, bytes != NULL);
  size_t count = 1;
  for (int k = 0; k < dims_size; k++) {
    size_t old_count = count;
    TF_LITE_ENSURE_MSG(
        subgraph->context_,
        MultiplyAndCheckOverflow(old_count, dims[k], &count) == kTfLiteOk,
        "BytesRequired number of elements overflowed.\n");
  }
  size_t type_size = 0;
  TF_LITE_ENSURE_OK(subgraph->context_, GetSizeOfType(&subgraph->context_, type, &type_size));
  TF_LITE_ENSURE_MSG(
      subgraph->context_, MultiplyAndCheckOverflow(type_size, count, bytes) == kTfLiteOk,
      "BytesRequired number of bytes overflowed.\n");
  return kTfLiteOk;
}


TfLiteStatus SetTensorParametersReadOnly(Subgraph* subgraph,
      int tensor_index, TfLiteType type, const char* name, const size_t rank,
      const int* dims, TfLiteQuantization quantization, const char* buffer,
      size_t bytes, //const Allocation* allocation = nullptr,
      TfLiteSparsity* sparsity)
{
	printf("Subgraph::SetTensorParametersReadOnly,index=%d,name=%s\n",tensor_index,name);
	/*
	// Ensure quantization cleanup on failure.
	ScopedTfLiteQuantization scoped_quantization(&quantization);
	ScopedTfLiteSparsity scoped_sparsity(sparsity);
	if (state_ == kStateInvokableAndImmutable) {
		printf(
			"SetTensorParametersReadOnly is disallowed when graph is immutable.");
		return kTfLiteError;
	  }
	*/
	TF_LITE_ENSURE(subgraph->context_,
					 tensor_index < subgraph->context_.tensors_size && tensor_index >= 0);

	// For most tensors we know exactly how much memory is necessary so we can
	// ensure the buffer is large enough. However, we need to skip string tensors
	// and sparse tensors because their sizes change with the contents.
	// TODO(b/145615516): Extend BytesRequired to check sparse tensors.
	if (type != kTfLiteString && sparsity == NULL) {
		size_t required_bytes;
		TF_LITE_ENSURE_OK(&subgraph->context_, BytesRequired(subgraph, type, dims, rank, &required_bytes));
		TF_LITE_ENSURE_EQ(&subgraph->context_, required_bytes, bytes);
	  }

	  TfLiteTensor* tensor = &subgraph->context_.tensors[tensor_index];
	  if (type == tensor->type &&
		  EqualArrayAndTfLiteIntArray(tensor->dims, rank, dims)) {
		// Fast path which does not invalidate the invokable property.
		TfLiteTensorDataFree(tensor);
		//TfLiteQuantizationFree(&tensor.quantization);
		tensor->data.raw = (const char*)buffer;
		if (!tensor->dims) tensor->dims = ConvertArrayToTfLiteIntArray(rank, dims);
		//tensor.params = GetLegacyQuantization(quantization);
		//tensor.quantization = *scoped_quantization.release();
		//tensor.sparsity = scoped_sparsity.release();
		tensor->allocation_type = kTfLiteMmapRo;
		//tensor.allocation = allocation;
	  } else {
		//state_ = kStateUninvokable;
		TfLiteTensorReset(type, name, ConvertArrayToTfLiteIntArray(rank, dims),
						  GetLegacyQuantization(quantization),
						  (const char*)buffer, bytes, kTfLiteMmapRo,
						  NULL, false, tensor);/*
		void TfLiteTensorReset(TfLiteType type, const char* name, TfLiteIntArray* dims,
                       TfLiteQuantizationParams quantization, char* buffer,
                       size_t size, TfLiteAllocationType allocation_type,
                       const void* allocation, bool is_variable,
                       TfLiteTensor* tensor) */
		// TODO(suharshs): Update TfLiteTensorReset to include the new quantization
		// if there are other required callers.
		//tensor.quantization = *scoped_quantization.release();
		//tensor.sparsity = scoped_sparsity.release();
	  }
	  return kTfLiteOk;
}
  
  
TfLiteStatus SetTensorParametersReadWrite(Subgraph* subgraph,
      int tensor_index, TfLiteType type, const char* name, const size_t rank,
      const int* dims, TfLiteQuantization quantization,
      bool is_variable/* = false*/, const size_t rank_dims_signature,
      const int* dims_signature )
{	
	printf("Subgraph::SetTensorParametersReadWrite,index=%d,name=%s\n",tensor_index,name);
	/*
	ScopedTfLiteQuantization scoped_quantization(&quantization);
	if (state_ == kStateInvokableAndImmutable) {
		printf(
			"SetTensorParametersReadWrite is disallowed when graph is immutable.");
		return kTfLiteError;
	
	}*/
	  
	TF_LITE_ENSURE(&subgraph->context_,tensor_index < subgraph->context_.tensors_size && tensor_index >= 0);
	
	size_t required_bytes = 0;
	if (type != kTfLiteString) {
		// These types will be allocated in our arena so we need to record how
		// many bytes we will need based on the dimensions. String tensors are
		// allocated dynamically and we can't know ahead of time how much space
		// they will require.
		TF_LITE_ENSURE_OK(&subgraph->context_,
						  BytesRequired(subgraph, type, dims, rank, &required_bytes));
	}
	
	TfLiteAllocationType allocation_type = kTfLiteArenaRw;
	if (type == kTfLiteString) {
		if (is_variable) {
		  // We don't have a real use case for string variable tensor.
		  printf("String variable tensor isn't supported.");
		  return kTfLiteError;
		}
		allocation_type = kTfLiteDynamic;
	} else if (is_variable) {
		allocation_type = kTfLiteArenaRwPersistent;
	}
	char* data = malloc(required_bytes);
	TfLiteTensor* tensor = &subgraph->context_.tensors[tensor_index];
	TfLiteTensorReset(type, name, ConvertArrayToTfLiteIntArray(rank, dims),
						GetLegacyQuantization(quantization),
						/*buffer=*/data, required_bytes, allocation_type,
						NULL, is_variable, tensor);
	
	
	  // TODO(suharshs): Update TfLiteTensorReset to include the new quantization
	  // if there are other required callers.
	  //tensor.quantization = *scoped_quantization.release();
	  //tensor.dims_signature =ConvertArrayToTfLiteIntArray(rank_dims_signature, dims_signature);
	return kTfLiteOk;
}