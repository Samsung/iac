#include "opresolver.h"

#define MAX_OP_NUM 256

TfLiteStatus Init_OpResolver(OpResolver * op_resolver)
{
#ifndef DISABLE_DEBUG_LOG
	printf("Init_OpResolver\n");
#endif
	op_resolver->register_op_size = 0;
	op_resolver->register_op_capasity = MAX_OP_NUM;//hard coded, ensure register_op_capasity >= register_op_size
	op_resolver->registrations =  (TfLiteRegistration*)malloc(sizeof(TfLiteRegistration)*op_resolver->register_op_capasity);
	memset(op_resolver->registrations, 0x00, sizeof(TfLiteRegistration)*op_resolver->register_op_capasity);

	Rigister_Op(op_resolver,tflite_BuiltinOperator_MIRROR_PAD, Register_MIRROR_PAD());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_DEPTHWISE_CONV_2D, Register_DEPTHWISE_CONV_2D());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_RELU, Register_RELU());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_ABS, Register_ABS());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_SUB, Register_SUB());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_MUL, Register_MUL());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_ADD, Register_ADD());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_MAX_POOL_2D, Register_MAX_POOL());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_CONV_2D, Register_CONV2D());

	Rigister_Op(op_resolver,tflite_BuiltinOperator_FULLY_CONNECTED, Register_FC() );
	Rigister_Op(op_resolver,tflite_BuiltinOperator_SOFTMAX, Register_SOFTMAX() );
	Rigister_Op(op_resolver,tflite_BuiltinOperator_ARG_MAX, Register_ARGMAX() );
	Rigister_Op(op_resolver,tflite_BuiltinOperator_STRIDED_SLICE, Register_STRIDED_SLICE());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_SUM, Register_SUM());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_SQRT, Register_SQRT());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_DIV, Register_DIV());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_RESHAPE, Register_RESHAPE());
	Rigister_Op(op_resolver,tflite_BuiltinOperator_CONCATENATION, Register_CONCATENATION());
	return kTfLiteOk;
}

TfLiteStatus Delete_OpResolver(OpResolver * op_resolver)
{
#ifndef DISABLE_DEBUG_LOG
	printf("Delete_OpResolver\n");
#endif
	if(op_resolver->registrations )
		free(op_resolver->registrations);
    return kTfLiteOk;
}

TfLiteStatus Rigister_Op(OpResolver * op_resolver,tflite_BuiltinOperator_enum_t op,  TfLiteRegistration* new_registration)
{
#ifndef DISABLE_DEBUG_LOG
	printf("Rigister_Op op type = %d\n", op);
#endif
	if(op_resolver->register_op_size < op_resolver->register_op_capasity)
	{
		TfLiteRegistration* registation = op_resolver->registrations + op_resolver->register_op_size;
		
		registation->init = new_registration->init;
		registation->free = new_registration->free;
		registation->prepare = new_registration->prepare;
		registation->invoke = new_registration->invoke;
		registation->builtin_code = op;
		//registation->custom_name = new_registration->custom_name;
		//registation->version = new_registration->version;
		
		op_resolver->register_op_size = op_resolver->register_op_size +1;
		return kTfLiteOk;
	}else{ //TODO: expand capasity 
		return kTfLiteError;
	}
}

TfLiteStatus Print_OpResolver(OpResolver * op_resolver)
{
#ifndef DISABLE_DEBUG_LOG
	printf("register op size = %d\n",op_resolver->register_op_size);
#endif
	for(int i=0;i<op_resolver->register_op_size;i++){
		printf("builtin_code = %d\n", op_resolver->registrations[i].builtin_code);	
	}
	return kTfLiteOk;
}

TfLiteRegistration* FindOp(OpResolver * op_resolver, tflite_BuiltinOperator_enum_t op, int version)  
{
	
	for(int i=0;i<op_resolver->register_op_size;i++)
	{
		TfLiteRegistration* registation = op_resolver->registrations + i;
		if(op == registation->builtin_code)
			return registation;
	}
	return NULL;
}
