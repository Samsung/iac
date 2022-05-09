#ifndef TFLITE_SUBGRAPH_H
#define TFLITE_SUBGRAPH_H

#include "common.h"

typedef struct NodeRegistration{
	TfLiteNode node;
	TfLiteRegistration registration;
} NodeRegistration;

typedef struct Subgraph {	
	char* name_ ; //=NULL
	TfLiteContext context_ ; //=NULL
	TfLiteIntArray* inputs_ ; //=NULL
	TfLiteIntArray* outputs_ ; //=NULL
	TfLiteIntArray* variables_ ; //=NULL
	
	TfLiteTensor* tensors_; //=NULL
	int tensors_size;//=0
	
	NodeRegistration* nodes_and_registrations_;	
	int nodes_and_registrations_size ;//=0
	
} Subgraph;

//constructor & decontructor
void Print_Subgraph(Subgraph* subgraph);

void Init_Subgraph(Subgraph* subgraph);

void Delete_Subgraph(Subgraph* subgraph);


//get set functions
TfLiteStatus SetInputs(Subgraph* subgraph , int* inputs, int size);
 
TfLiteStatus SetOutputs(Subgraph* subgraph , int* outputs, int size );

TfLiteStatus SetVariables(Subgraph* subgraph ,int* variables, int size );


TfLiteIntArray* getInputs(Subgraph* subgraph ) ;

TfLiteIntArray* getOutputs(Subgraph* subgraph ) ;

TfLiteIntArray* getVariables(Subgraph* subgraph) ;

TfLiteTensor* tensor(Subgraph* subgraph, size_t tensor_index) ;
 
size_t tensors_size(Subgraph* subgraph) ;

 
size_t nodes_size(Subgraph* subgraph) ;
 
TfLiteTensor* tensors(Subgraph* subgraph) ;

TfLiteContext* context(Subgraph* subgraph);


// Init,Prepare,Invoke,Free the given 'node' for execution.
void* OpInit(Subgraph* subgraph,const TfLiteRegistration* op_reg, const char* buffer,size_t length);

void OpFree(Subgraph* subgraph,const TfLiteRegistration* op_reg, void* buffer) ;

TfLiteStatus OpPrepare(Subgraph* subgraph,const TfLiteRegistration* op_reg, TfLiteNode* node);

TfLiteStatus OpInvoke(Subgraph* subgraph,const TfLiteRegistration* op_reg, TfLiteNode* node) ;
 
 TfLiteStatus BytesRequired(Subgraph* subgraph,TfLiteType type, const int* dims,
                                     size_t dims_size, size_t* bytes);

TfLiteQuantizationParams GetLegacyQuantization(
    const TfLiteQuantization quantization);								 
//Subgrap contruct functions
TfLiteStatus ReserveNodes(Subgraph* subgraph, int size);
TfLiteStatus AddTensors(Subgraph* subgraph,int tensors_to_add);

TfLiteStatus AddNodeWithParameters(Subgraph* subgraph,
									 int* inputs, int input_size,//const std::vector<int>& inputs,
                                     int* outputs, int output_size,//const std::vector<int>& outputs,
                                     int* intermidiates, int inter_size,//const std::vector<int>& intermediates,
                                     const char* init_data,
                                     size_t init_data_size, void* builtin_data,
                                     const TfLiteRegistration* registration,
                                     int node_index );
									 



TfLiteStatus SetTensorParametersReadOnly(Subgraph* subgraph,
      int tensor_index, TfLiteType type, const char* name, const size_t rank,
      const int* dims, TfLiteQuantization quantization, const char* buffer,
      size_t bytes, //const Allocation* allocation ,
      TfLiteSparsity* sparsity );

 
TfLiteStatus SetTensorParametersReadWrite(Subgraph* subgraph,
      int tensor_index, TfLiteType type, const char* name, const size_t rank,
      const int* dims, TfLiteQuantization quantization,
      bool is_variable, const size_t rank_dims_signature ,//rank_dims_signature = 0
      const int* dims_signature );
	  
#endif