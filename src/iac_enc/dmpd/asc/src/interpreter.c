#include "interpreter.h"
#include "common.h"
#include "subgraph.h"
#include "opresolver.h"
/*
void FlatBufferIntArrayToPointer(T* flat_array, int* dest, int* size) {
  if (flat_array == nullptr || dest == NULL ) {
	    if(size !=NULL) *size = 0;
		return ;
  }
  *size = flat_array->Length();
  for (int i = 0; i < flat_array->Length(); i++) {
    dest[i] = flat_array->Get(i);
  }
  return ;
}*/
//----------------------Interpreter Structure Function-----------------------------

void Print_Interpreter(Interpreter* interpreter)
{
#ifndef DISABLE_DEBUG_LOG
	printf("sub_graph size =%d ,description = %s\n",interpreter->subgraph_size, interpreter->description);
#endif
	for(int i=0;i<interpreter->subgraph_size;i++)
	{
		Print_Subgraph(interpreter->subgraphs+i);
	}
}

TfLiteStatus Init_Interpreter(Interpreter* interpreter)
{
#ifndef DISABLE_DEBUG_LOG
	printf("Interpreter::Init_Interpreter !\n");
#endif
	interpreter->subgraph_size =0;
	AddSubgraphs(interpreter, 1,NULL);	
	interpreter->context_ = &(primary_subgraph(interpreter)->context_);
    return kTfLiteOk;  
}

TfLiteStatus AddSubgraphs(Interpreter* interpreter,int subgraphs_to_add,int* first_new_tensor_index)// first_new_tensor_index not used
{	
#ifndef DISABLE_DEBUG_LOG
	printf("Interpreter::AddSubgraphs !\n");
#endif
	if(subgraphs_to_add<=0)
		return kTfLiteOk;
	
	const size_t base_index = interpreter->subgraph_size;	
	int total_subgraphs_to_add = base_index + subgraphs_to_add;
	
	//Subgraph subgraphs[total_subgraphs_to_add];
	Subgraph* subgraphs = (Subgraph*) malloc(sizeof(Subgraph)*total_subgraphs_to_add);
	if(!subgraphs)return kTfLiteError;
	memset(subgraphs,0,sizeof(Subgraph));
#ifndef DISABLE_DEBUG_LOG	
	printf("Interpreter::AddSubgraphs 1!\n");
#endif	
	//copy old subgraphs to new array
	if(interpreter->subgraphs)
	{
		for (int i = 0; i < base_index; ++i)
		{
			subgraphs[i] = interpreter->subgraphs[i];
		}
	}
#ifndef DISABLE_DEBUG_LOG		
	printf("Interpreter::AddSubgraphs 2!\n");
#endif
	interpreter->subgraph_size = total_subgraphs_to_add;
	interpreter->subgraphs = subgraphs;
    return kTfLiteOk;
}

Subgraph* primary_subgraph(Interpreter* interpreter) 
{
#ifndef DISABLE_DEBUG_LOG
	printf("Interpreter::primary_subgraph !\n");
#endif
	if (!interpreter) {
		printf("Null output pointer passed to InterpreterBuilder.");
		return kTfLiteError;
	}  
	
	if(interpreter->subgraph_size <=0)
		return NULL;
	
	return interpreter->subgraphs;
}	

Delete_Interpreter(Interpreter* interpreter)
{
#ifndef DISABLE_DEBUG_LOG
	printf("Interpreter::Delete_Interpreter !\n");
#endif
	
	for(int i=0;i<interpreter->subgraph_size;i++)
	{
		Subgraph* graph = &(interpreter->subgraphs[i]);
		Delete_Subgraph(graph);
	}
  if (interpreter->subgraphs)
  {
    free(interpreter->subgraphs);
    interpreter->subgraphs = NULL;
  }
  return kTfLiteOk;
	
	//if(interpreter->description) free(interpreter->description);
	
}

//----------------------Interpreter Run Inference Function----------------------------
TfLiteStatus AllocateTensors(Interpreter* interpreter)
{
	return kTfLiteOk;
}

TfLiteStatus Invoke(Interpreter* interpreter)
{
	return kTfLiteOk;
}