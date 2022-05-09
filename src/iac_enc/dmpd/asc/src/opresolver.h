#ifndef TFLITE_OPRESOLVER_H
#define TFLITE_OPRESOLVER_H

#include "common.h"
#include "subgraph.h"
#include "tflite_reader.h"

typedef struct OpResolver
{
	int register_op_size;
	TfLiteRegistration* registrations;
	int register_op_capasity ;
	
}OpResolver;


//----------kernel reginster function, implemented in each kernel source file------------

TfLiteRegistration* Register_PAD();//tf.pad()
TfLiteRegistration* Register_SQUEEZE();//tf.squeeze	
TfLiteRegistration* Register_MAX_POOL_2D();//tf.nn.max_pool	 
TfLiteRegistration* Register_RELU();// tf.nn.relu	
TfLiteRegistration* Register_CONCATENATION();// tf.concat
TfLiteRegistration* Register_EXPAND_DIMS();//tf.expand_dims
TfLiteRegistration* Register_RESHAPE();//tf.reshape

//ops for color transfer model
TfLiteRegistration* Register_ADD();
TfLiteRegistration* Register_ABS();
TfLiteRegistration* Register_ARGMAX();
TfLiteRegistration* Register_CONV2D();
TfLiteRegistration* Register_DEPTHWISE_CONV_2D();
TfLiteRegistration* Register_FC();
TfLiteRegistration* Register_MAX_POOL();
TfLiteRegistration* Register_MEAN();
TfLiteRegistration* Register_MIRROR_PAD();
TfLiteRegistration* Register_MUL();
TfLiteRegistration* Register_NEG();
TfLiteRegistration* Register_RELU();
TfLiteRegistration* Register_RSQRT();
TfLiteRegistration* Register_SOFTMAX();
TfLiteRegistration* Register_SQUARED_DIFFERENCE();
TfLiteRegistration* Register_SUB();
TfLiteRegistration* Register_TANH();
TfLiteRegistration* Register_TRANSPOSE_CONV();
TfLiteRegistration* Register_STRIDED_SLICE();
TfLiteRegistration* Register_SUM();
TfLiteRegistration* Register_SQRT();
TfLiteRegistration* Register_DIV();
TfLiteRegistration* Register_RESHAPE();
TfLiteRegistration* Register_CONCATENATION();


TfLiteStatus Rigister_Op(OpResolver * op_resolver,tflite_BuiltinOperator_enum_t op/*tflite::BuiltinOperator op*/,  TfLiteRegistration* registration);

TfLiteStatus Init_OpResolver(OpResolver * op_resolver);

TfLiteStatus Delete_OpResolver(OpResolver * op_resolver);

TfLiteStatus Print_OpResolver(OpResolver * op_resolver);

TfLiteRegistration* FindOp(OpResolver * op_resolver, tflite_BuiltinOperator_enum_t op/*tflite::BuiltinOperator op*/, int version)  ;

#endif