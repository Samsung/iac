#ifndef TFLITE_INTERPRETER_H
#define TFLITE_INTERPRETER_H

#include "subgraph.h"
#include "common.h"
#include "util.h"

typedef struct Interpreter {	
	TfLiteContext* context_;
	
	Subgraph* subgraphs;// = NULL;
	int subgraph_size ;//= 0;
	
	//char** metadata_key = NULL;
	//char** metadata_value = NULL;
	//int metadata_size = 0;
	int version ;
	char* description;
} Interpreter;

void Print_Interpreter(Interpreter* interpreter);

TfLiteStatus Init_Interpreter(Interpreter* interpreter);

TfLiteStatus AddSubgraphs(Interpreter* interpreter, int subgraphs_to_add,int* first_new_tensor_index);
					
Subgraph* primary_subgraph(Interpreter* interpreter) ;

Delete_Interpreter(Interpreter* interpreter);

//----------------------Utility Function-------------------------------=
//void FlatBufferIntArrayToPointer(T* flat_array, int* dest, int* size);

//----------------------Parse Model Function-----------------------------
/*	
TfLiteStatus Parse(Interpreter* interpreter,  const char* buffer);

TfLiteStatus Parse(Interpreter* interpreter,  const ::tflite::Model* model_)

TfLiteStatus ParseNodes(
      const flatbuffers::Vector<flatbuffers::Offset<Operator>>* operators,
      Subgraph* subgraph,const ::tflite::Model* model_);
 
TfLiteStatus ParseTensors(
      const flatbuffers::Vector<flatbuffers::Offset<Buffer>>* buffers,
      const flatbuffers::Vector<flatbuffers::Offset<Tensor>>* tensors,
      Subgraph* subgraph,const ::tflite::Model* model_);
  
TfLiteStatus ParseQuantization(Interpreter* interpreter, const QuantizationParameters* src_quantization,
                                 TfLiteQuantization* quantization,
                                 const std::vector<int>& dims);
TfLiteStatus ParseSparsity(Interpreter* interpreter, const SparsityParameters* src_sparsity,
                             TfLiteSparsity** sparsity);
*/



//----------------------Interpreter Run Inference Function----------------------------


TfLiteStatus AllocateTensors(Interpreter* interpreter);

TfLiteStatus Invoke(Interpreter* interpreter);

#endif