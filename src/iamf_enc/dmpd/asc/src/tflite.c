// Example on how to build a Monster FlatBuffer.

// Note: while some older C89 compilers are supported when
// -DFLATCC_PORTABLE is defined, this particular sample is known not to
// not work with MSVC 2010 (MSVC 2013 is OK) due to inline variable
// declarations. These are easily move to the start of code blocks, but
// since we follow the step-wise tutorial, it isn't really practical
// in this case. The comment style is technically also in violation of C89.

#include <sys/stat.h> 

#ifdef WIN32 
#ifndef _UNISTD_H
#define _UNISTD_H
#include <io.h>
#include <process.h>
#endif /* _UNISTD_H */
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include "kernel.h"
// <string.h> and <assert.h> already included.

// Convenient namespace macro to manage long namespace prefix.
// The ns macro makes it possible to write `ns(Monster_create(...))`
// instead of `MyGame_Sample_Monster_create(...)`
#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(tflite, x) // Specified in the schema.

// A helper to simplify creating vectors from C-arrays.
#define c_vec_len(V) (sizeof(V)/sizeof((V)[0]))

// This allows us to verify result in optimized builds.
#define test_assert(x) do { if (!(x)) { assert(0); return -1; }} while(0)


// This isn't strictly needed because the builder already included the reader,
// but we would need it if our reader were in a separate file.
#include "tflite_reader.h"
#include "interpreter.h"
#include "subgraph.h"
#include "common.h"
#include "builtin_op_data.h"
#include "opresolver.h"
#include "util.h"

#include <time.h>
#include <stdlib.h>

TfLiteFusedActivation parse_activation(ns(ActivationFunctionType_enum_t) activation) {
    switch (activation) {
      case ns(ActivationFunctionType_NONE):
        return kTfLiteActNone;
      case ns(ActivationFunctionType_RELU):
        return kTfLiteActRelu;
      case ns(ActivationFunctionType_RELU_N1_TO_1):
        return kTfLiteActRelu1;
      case ns(ActivationFunctionType_RELU6):
        return kTfLiteActRelu6;
      case ns(ActivationFunctionType_TANH):
        return kTfLiteActTanh;
      case ns(ActivationFunctionType_SIGN_BIT):
        return kTfLiteActSignBit;
    }
    return kTfLiteActNone;
  }

TfLitePadding parse_padding(ns(Padding_enum_t) padding) {  
     switch (padding) {
      case ns(Padding_SAME):
        return kTfLitePaddingSame;
      case ns(Padding_VALID):
        return kTfLitePaddingValid;
    }
    return kTfLitePaddingUnknown;
}

void parse_builtin_op_params(ns(Operator_table_t) operator, ns(BuiltinOperator_enum_t) op_code, void** builtin_data)
{
    switch( op_code ){		
    case ns(BuiltinOperator_CONV_2D) :
    {
        TfLiteConvParams* params = malloc(sizeof(TfLiteConvParams));
        ns(Conv2DOptions_table_t) conv2DOptions = (ns(Conv2DOptions_table_t)) ns(Operator_builtin_options(operator));					
        params->padding = parse_padding( ns(Conv2DOptions_padding(conv2DOptions)));
        params->stride_width =  ns(Conv2DOptions_stride_w(conv2DOptions));
        params->stride_height = ns(Conv2DOptions_stride_h(conv2DOptions));
        params->activation = parse_activation( ns(Conv2DOptions_fused_activation_function(conv2DOptions)));
        params->dilation_width_factor =  ns(Conv2DOptions_dilation_w_factor(conv2DOptions));
        params->dilation_height_factor = ns(Conv2DOptions_dilation_h_factor(conv2DOptions));
                    
        *builtin_data = params;
        
                    
    }
    break;
    case ns(BuiltinOperator_DEPTHWISE_CONV_2D) : 
    {
        TfLiteDepthwiseConvParams *params = malloc(sizeof(TfLiteDepthwiseConvParams)) ;
        ns(DepthwiseConv2DOptions_table_t) depthConvOptions = (ns(DepthwiseConv2DOptions_table_t)) ns(Operator_builtin_options(operator));					
        params->padding = parse_padding( ns(DepthwiseConv2DOptions_padding(depthConvOptions)));
        params->stride_width =  ns(DepthwiseConv2DOptions_stride_w(depthConvOptions));
        params->stride_height = ns(DepthwiseConv2DOptions_stride_h(depthConvOptions));
        params->depth_multiplier = ns(DepthwiseConv2DOptions_depth_multiplier(depthConvOptions));
        params->activation = parse_activation(ns(DepthwiseConv2DOptions_fused_activation_function(depthConvOptions)));
        params->dilation_width_factor =  ns(DepthwiseConv2DOptions_dilation_w_factor(depthConvOptions));
        params->dilation_height_factor = ns(DepthwiseConv2DOptions_dilation_h_factor(depthConvOptions));
                    
        *builtin_data = params;
        
    }
    break;
    case ns(BuiltinOperator_TRANSPOSE_CONV) :
    {
        TfLiteTransposeConvParams* params = malloc(sizeof(TfLiteTransposeConvParams));
        ns(TransposeConvOptions_table_t) transposeConvOptions = (ns(TransposeConvOptions_table_t)) ns(Operator_builtin_options(operator));		
        
        params->padding = parse_padding( ns(TransposeConvOptions_padding(transposeConvOptions)));
        params->stride_width =  ns(Conv2DOptions_stride_w(transposeConvOptions));
        params->stride_height = ns(Conv2DOptions_stride_h(transposeConvOptions));
    
        *builtin_data = params;
        
    }
    break;
    case ns(BuiltinOperator_MUL):
    {
        TfLiteMulParams*params = malloc(sizeof(TfLiteMulParams));
        ns(MulOptions_table_t) mulOptions = (ns(MulOptions_table_t)) ns(Operator_builtin_options(operator));
        
        params->activation = parse_activation(ns(MulOptions_fused_activation_function(mulOptions)));
        *builtin_data = params;
        
    }
    break;
    case ns(BuiltinOperator_ADD):
    {
         TfLiteAddParams* params= malloc(sizeof(TfLiteAddParams));
        ns(AddOptions_table_t) addOptions = (ns(AddOptions_table_t)) ns(Operator_builtin_options(operator));
        
        params->activation = parse_activation(ns(MulOptions_fused_activation_function(addOptions)));
        *builtin_data = params;
        break;
    }
    case ns(BuiltinOperator_MEAN): // no Rigister in Init_OpResolver()
    {
        
        // TfLiteMirrorPaddingParams params ;		
        // ns(MirrorPadOptions_table_t) mirrorPadOptions = (ns(MirrorPadOptions_table_t)) ns(Operator_builtin_options(operator));
        
        // params.mode = ns(MirrorPadOptions_mode(mirrorPadOptions));
        // *builtin_data = &params;
        break;
    }
    case ns(BuiltinOperator_RSQRT): // no Rigister in Init_OpResolver()
    case ns(BuiltinOperator_TANH): // no Rigister in Init_OpResolver()
    case ns(BuiltinOperator_NEG) : // no Rigister in Init_OpResolver()
    case ns(BuiltinOperator_SQUARED_DIFFERENCE): // no Rigister in Init_OpResolver()
    {
        
    }
    break;

    case ns(BuiltinOperator_MIRROR_PAD):
    {
        TfLiteMirrorPaddingParams* params = malloc(sizeof(TfLiteMirrorPaddingParams));
        ns(MirrorPadOptions_table_t) MirrorPadOptions = (ns(MirrorPadOptions_table_t)) ns(Operator_builtin_options(operator));
        params->mode = ns(MirrorPadOptions_mode(MirrorPadOptions));
        *builtin_data = params;
        
    }
    break;

    case ns(BuiltinOperator_MAX_POOL_2D):
    {	
        TfLitePoolParams* params = malloc(sizeof(TfLitePoolParams));
        ns(Pool2DOptions_table_t) poolOptions= (ns(Pool2DOptions_table_t))ns(Operator_builtin_options(operator));

        params->padding       		= ns(Pool2DOptions_padding(poolOptions));
        params->stride_width  		= ns(Pool2DOptions_stride_w(poolOptions));
        params->stride_height 		= ns(Pool2DOptions_stride_h(poolOptions));
        params->filter_width  		= ns(Pool2DOptions_filter_width(poolOptions));
        params->filter_height 		= ns(Pool2DOptions_filter_height(poolOptions));
        params->activation    		= ns(Pool2DOptions_fused_activation_function(poolOptions));		

        *builtin_data = params;
        
    }
    break;

    case ns(BuiltinOperator_RELU): // there is Rigister in Init_OpResolver(), but dose not use builtin_data in invoke
    {
        // TfLiteLeakyReluParams* params = malloc(sizeof(TfLiteLeakyReluParams));
        // ns(LeakyReluOptions_table_t) reluOption= (ns(LeakyReluOptions_table_t))ns(Operator_builtin_options(operator));

        // params->alpha = ns(LeakyReluOptions_alpha(reluOption));
        // *builtin_data = params;
        
    }
    break;
    case ns(BuiltinOperator_ABS): // there is Rigister in Init_OpResolver(), but dose not use builtin_data in invoke
    {
        
    }
    break;
    
    case ns(BuiltinOperator_SUB):
    {
        TfLiteSubParams* params = malloc(sizeof(TfLiteSubParams));
        ns(SubOptions_table_t) subOption= (ns(SubOptions_table_t))ns(Operator_builtin_options(operator));

        params->activation = ns(SubOptions_fused_activation_function(subOption));

        *builtin_data = params;
    }
    break;
    
    case ns(BuiltinOperator_FULLY_CONNECTED): // 
    {
      TfLiteFullyConnectedWeightsFormat *params = malloc(sizeof(TfLiteFullyConnectedWeightsFormat));
      ns(FullyConnectedOptions_table_t) subOption = (ns(FullyConnectedOptions_table_t))ns(Operator_builtin_options(operator));

      *builtin_data = params;
    }
    break;

    case ns(BuiltinOperator_SOFTMAX):
    {
        TfLiteSoftmaxParams * params = malloc(sizeof(TfLiteSoftmaxParams));
        ns(SoftmaxOptions_table_t) softmaxOption= (ns(SoftmaxOptions_table_t))ns(Operator_builtin_options(operator));


        params->beta = ns(SoftmaxOptions_beta(softmaxOption));
        *builtin_data = params;
    }
    break;
    case ns(BuiltinOperator_ARG_MAX):
    {
        TfLiteArgMaxParams * params = malloc(sizeof(TfLiteArgMaxParams));
        ns(ArgMaxOptions_table_t) argmaxOption= (ns(ArgMaxOptions_table_t))ns(Operator_builtin_options(operator));

        params->output_type = ns(ArgMaxOptions_output_type(argmaxOption));
        *builtin_data = params;
    }
    break;
    case ns(BuiltinOperator_STRIDED_SLICE):
    {
        TfLiteStridedSliceParams * params = malloc(sizeof(TfLiteStridedSliceParams));
        ns(StridedSliceOptions_table_t) StridedSliceOption= (ns(StridedSliceOptions_table_t))ns(Operator_builtin_options(operator));

        params->begin_mask       = ns(StridedSliceOptions_begin_mask(StridedSliceOption));        
        params->end_mask         = ns(StridedSliceOptions_end_mask(StridedSliceOption));
        params->ellipsis_mask    = ns(StridedSliceOptions_ellipsis_mask(StridedSliceOption));
        params->new_axis_mask    = ns(StridedSliceOptions_new_axis_mask(StridedSliceOption));
        params->shrink_axis_mask = ns(StridedSliceOptions_shrink_axis_mask(StridedSliceOption));

        *builtin_data = params;
    }
    break;

    case ns(BuiltinOperator_SUM): // there is Rigister in Init_OpResolver(), but dose not use builtin_data in invoke
    {

    }
    break;

    case ns(BuiltinOperator_SQRT): // there is Rigister in Init_OpResolver(), but dose not use builtin_data in invoke
    {

    }
    break;
    case ns(BuiltinOperator_DIV): // there is Rigister in Init_OpResolver(), but dose not use builtin_data in invoke
    {

    }
    break;
    case ns(BuiltinOperator_RESHAPE):
    {
        TfLiteReshapeParams * params = malloc(sizeof(TfLiteReshapeParams));
        ns(ReshapeOptions_table_t) reshapeOption= (ns(ReshapeOptions_table_t))ns(Operator_builtin_options(operator));

        *builtin_data = params;
    }
    break;

    case ns(BuiltinOperator_CONCATENATION):
    {
        TfLiteConcatenationParams* params = malloc(sizeof(TfLiteConcatenationParams));
        ns(ConcatenationOptions_table_t) reshapeOption = (ns(ConcatenationOptions_table_t))ns(Operator_builtin_options(operator));

        *builtin_data = params;
    }
    break;
    
    default:
    {
        LOGE("operator builtin_options not supported %d\n",op_code);
    }
    break;
    }
}

int access_tflite_buffer(const void *buffer,Interpreter* interpreter,OpResolver* opResolver)
{

    // Note that we use the `table_t` suffix when reading a table object
    // as opposed to the `ref_t` suffix used during the construction of
    // the buffer.
    LOGE("\n------------parse model info ------------------------\n");
    ns(Model_table_t) model = ns(Model_as_root(buffer));
    
    test_assert(model != 0);
    
    uint32_t version = ns(Model_version(model));//version:uint;
    LOGE("tflite version is %u\n",version);
    interpreter->version = version;//interpreter.h
    
    flatbuffers_string_t description = ns(Model_description(model));
    LOGE("tflite description is %s\n",description);//description:string;
    interpreter->description = (char*)description;//interpreter.h

    LOGE("\n------------parse metadata------------------------\n");
    // Deprecated, prefer to use metadata field.
    flatbuffers_int32_vec_t metadata_buffer = ns(Model_metadata_buffer(model));//metadata_buffer:[int];
    size_t metadata_len = flatbuffers_int32_vec_len(metadata_buffer);
    for(int i=0;i<metadata_len;i++)
        LOGE("meatadata_buffer index = %d \n",flatbuffers_uint8_vec_at(metadata_buffer, i));
    
    ns(Metadata_vec_t) metadatas = ns(Model_metadata(model));
    size_t metadatas_len = ns(Metadata_vec_len(metadatas));
    for(int i=0;i<metadatas_len;i++)
    {
        ns(Metadata_table_t) metadata = ns(Metadata_vec_at(metadatas,i));
        flatbuffers_string_t metadata_name = ns(Metadata_name(metadata));
        uint32_t metadata_buffer = ns(Metadata_buffer(metadata));// buffer:uint;
        LOGE("meatadata_buffer index = %d, name= %s, buffer = %d \n",i,metadata_name,metadata_buffer );
    }
    

    LOGE("\n------------parse operator_codes------------------\n");
    ns(OperatorCode_vec_t) operator_codes = ns(Model_operator_codes(model));
    size_t opcodes_len = ns(OperatorCode_vec_len(operator_codes));
    LOGE("operator_codes size = %u\n",opcodes_len);

#if 0	
    for(int i=0;i<opcodes_len;i++)
    {  
        ns(OperatorCode_table_t) opcode =  ns(OperatorCode_vec_at(operator_codes, i));
        int8_t builtin_code = ns(OperatorCode_builtin_code(opcode)) ;
        int8_t op_version = ns(OperatorCode_version(opcode));
        flatbuffers_string_t custom_code = ns(OperatorCode_custom_code(opcode));
        LOGE("tflite op code = %d version is %u custom_string = %s\n",builtin_code, op_version,custom_code);	
    }
#endif 
    
    
    LOGE("\n------------parse buffers-----------------------\n");
    ns(Buffer_vec_t) buffers = ns(Model_buffers(model));
    size_t buffers_len = ns(Buffer_vec_len(buffers));
    LOGE("buffers size = %u\n",buffers_len);
#if 1	
    for(int i=0;i<buffers_len;i++)
    {  
        ns(Buffer_table_t) buffer =  ns(Buffer_vec_at(buffers, i));
        flatbuffers_uint8_vec_t data = ns(Buffer_data(buffer));	//data:[ubyte] ;	
        size_t buffer_len = flatbuffers_uint8_vec_len(data);
        LOGE("buffer index = %d, len = %d \n",i,buffer_len); 
    }
#endif 

    

    LOGE("\n------------parse subgraphs----------------------\n");
    ns(SubGraph_vec_t) subgraphs = ns(Model_subgraphs(model));
    size_t subgraphs_len = ns(SubGraph_vec_len(subgraphs));
    LOGE("subgraphs size = %u\n",subgraphs_len);
    //AddSubgraphs(interpreter,subgraphs_len-1,NULL);//interpreter.h
    
    for(int i=0;i<subgraphs_len;i++)
    {
        Subgraph* subgraphStruct = interpreter->subgraphs + i;
        
        ns(SubGraph_table_t) subgraph = ns( SubGraph_vec_at(subgraphs,i));
        flatbuffers_string_t graph_name = ns(SubGraph_name(subgraph));		
        
        flatbuffers_int32_vec_t inputs = ns(SubGraph_inputs(subgraph)); //inputs
        size_t subgraph_inputs_len = flatbuffers_int32_vec_len(inputs);
        SetInputs(subgraphStruct, (int*)inputs, subgraph_inputs_len);//subgraph.h
        
        flatbuffers_int32_vec_t outputs = ns(SubGraph_outputs(subgraph));//outputs
        size_t subgraph_outputs_len = flatbuffers_int32_vec_len(outputs);
        SetOutputs(subgraphStruct,(int*)outputs, subgraph_outputs_len);//subgraph.h

        ns(Tensor_vec_t) tensors = ns(SubGraph_tensors(subgraph));
        size_t subgraph_tensors_len = ns(Tensor_vec_len(tensors));
        AddTensors(subgraphStruct, subgraph_tensors_len);//subgraph.h
        
        for(size_t j=0;j<subgraph_tensors_len;j++)
        {
            ns(Tensor_table_t) tensor = ns(Tensor_vec_at(tensors,j));
            
            int8_t tensor_type = ns(Tensor_type(tensor));//type:TensorType; enum byte 
            uint32_t tensor_buffer = ns(Tensor_buffer(tensor));//buffer:uint;
            flatbuffers_string_t tensor_name = ns(Tensor_name(tensor));//name:string;
            
            flatbuffers_int32_vec_t tensor_shape = ns(Tensor_shape(tensor));//shape:[int]; NHWC
            size_t tensor_shape_size = flatbuffers_int32_vec_len(tensor_shape);
            
            flatbuffers_bool_t tensor_is_variable = ns(Tensor_is_variable(tensor));//is_variable:bool = false;
            
            ns(Buffer_table_t) buffer =  ns(Buffer_vec_at(buffers, tensor_buffer));
            flatbuffers_uint8_vec_t data = ns(Buffer_data(buffer));	//data:[ubyte] ;	
            size_t buffer_len = flatbuffers_uint8_vec_len(data);
            
            LOGE("tensor index = %d, name = %s, tensor_type = %s, tensor_buffer = %u, tensor_is_variable = %s, tensor_buffer_size = %d\n",
                    j, tensor_name, ns(TensorType_name(tensor_type)), tensor_buffer, tensor_is_variable?"true":"false", buffer_len);
                        
            TfLiteQuantization quantization = { kTfLiteNoQuantization, NULL};
            TfLiteType tflite_data_type ;
            ConvertTensorType(tensor_type, &tflite_data_type);
            if(buffer_len ==0){
                SetTensorParametersReadWrite(subgraphStruct, j, tflite_data_type, (char*)tensor_name , tensor_shape_size,
                          (const int*)tensor_shape,   quantization, false /*is_variable*/,
                           /*const Allocation* allocation ,*/
                          0 /* const size_t rank_dims_signature*/,NULL /*const int* dims_signature*/ );
                //Print_TfLiteTensor(&subgraphStruct->tensors_[j]);
            }else{				
                SetTensorParametersReadOnly(subgraphStruct, j, tflite_data_type, (char*)tensor_name , tensor_shape_size,
                          (const int*)tensor_shape,  quantization, (char*)data/*(const char*) buffer*/,
                          buffer_len, //const Allocation* allocation ,
                          NULL/*TfLiteSparsity* sparsity*/ );
                //Print_TfLiteTensor(&subgraphStruct->tensors_[j]);
            }	
        }
        
        ns(Operator_vec_t) operators = ns(SubGraph_operators(subgraph));
        size_t subgraph_operators_len = ns(Operator_vec_len(operators));
        ReserveNodes( subgraphStruct, subgraph_operators_len);
        
        for(int j=0; j<subgraph_operators_len;j++)
        {
            
            ns(Operator_table_t) operator = ns(Operator_vec_at(operators, j));
            uint32_t operator_index = ns(Operator_opcode_index(operator)); //opcode_index:uint;
            
            flatbuffers_int32_vec_t opreator_inputs = ns(Operator_inputs(operator));//inputs:[int];
            size_t opreator_inputs_len = flatbuffers_int32_vec_len(opreator_inputs);
            
            flatbuffers_int32_vec_t opreator_outputs = ns(Operator_outputs(operator));//outputs:[int];
            size_t opreator_outputs_len = flatbuffers_int32_vec_len(opreator_outputs);
            
            flatbuffers_uint8_vec_t opreator_intermediates = ns(Operator_intermediates(operator));//intermediates:[int];			
            size_t opreator_intermediates_len = flatbuffers_int32_vec_len(opreator_intermediates);
            
            ns(OperatorCode_table_t) opcode =  ns(OperatorCode_vec_at(operator_codes, operator_index));
            int8_t builtin_code = ns(OperatorCode_builtin_code(opcode)) ;	
            
            
            if(builtin_code == 32){//CUSTOM			
                LOGE("Custom operation is not surpported\n");
            }else {
            
                void*builtin_data = NULL;
                parse_builtin_op_params(operator, builtin_code, &builtin_data);
                
                TfLiteRegistration* registration = FindOp(opResolver,builtin_code, 1);//version default
                
                AddNodeWithParameters(subgraphStruct, (int*)opreator_inputs,opreator_inputs_len,
                        (int*) opreator_outputs, opreator_outputs_len,//const std::vector<int>& outputs,
                        (int*) opreator_intermediates,opreator_intermediates_len,//const std::vector<int>& intermediates,
                        NULL/*const char* init_data*/,0/*size_t init_data_size*/, builtin_data,
                        registration,j/* node_index*/ );
            }
        
            //LOGE information
            /*
            LOGE("oprator index = %d, opreator name = %s,input size = %d, output size = %d, intermediate size = %d\n",
                    j, ns(BuiltinOperator_name(builtin_code)),opreator_inputs_len, opreator_outputs_len,opreator_intermediates_len );
            LOGE("input :");
            for(int k=0;k<opreator_inputs_len;k++){
                int input_tensor_index = flatbuffers_int32_vec_at(opreator_inputs,k);
                ns(Tensor_table_t) tensor = ns(Tensor_vec_at(tensors,input_tensor_index));
                LOGE("(%d,%s)  ",input_tensor_index, ns(Tensor_name(tensor)));
            }
            LOGE("\noutputs :");
            for(int k=0;k<opreator_outputs_len;k++){
                int output_tensor_index = flatbuffers_int32_vec_at(opreator_outputs,k);
                ns(Tensor_table_t) tensor = ns(Tensor_vec_at(tensors,output_tensor_index));
                LOGE("(%d,%s)  ",output_tensor_index, ns(Tensor_name(tensor)));
            }
            
            LOGE("\nintermediates :");
            for(int k=0;k<opreator_intermediates_len;k++){
                int inter_tensor_index = flatbuffers_int32_vec_at(opreator_intermediates,k);
                ns(Tensor_table_t) tensor = ns(Tensor_vec_at(tensors,inter_tensor_index));
                LOGE("(%d,%s)  ",inter_tensor_index, ns(Tensor_name(tensor)));
            }
            LOGE("\n\n");*/
            
            //custom_options:[ubyte];
            //custom_options_format:CustomOptionsFormat;
            //mutating_variable_inputs:[bool];
        }

        LOGE("subgraph index = %d, graph_name = %s, inputs_len = %d, outputs_len = %d, operators_len = %d, tensors_len = %d\n",
            i, graph_name, subgraph_inputs_len,subgraph_outputs_len,subgraph_operators_len,subgraph_tensors_len);
    }

    
#if 0
    for(int i=0;i<subgraphs_len;i++)
    {
        Subgraph* subgraphStruct = interpreter->subgraphs + i;
        for(int j=0; j<subgraphStruct->nodes_and_registrations_size;j++)
        {
                TfLiteRegistration* op = &(subgraphStruct->nodes_and_registrations_[j].registration);
                TfLiteNode* node = &(subgraphStruct->nodes_and_registrations_[j].node);
                TfLiteContext* context = &(subgraphStruct->context_);
                // if(j == 0){					
                //     const TfLiteTensor* input_tensor = GetInput(context, node, 0);
                //     LOGE("----------------------%s-----------",input_tensor->name);
                //     int size = NumElements(input_tensor);
                //     srand(time(NULL));
                //     for(int i= 0; i< size;i++)
                //     {
                //         input_tensor->data.f[i] = rand()%255;
                //     }
                // }
                OpInvoke(subgraphStruct,op,node);
                // if(j == 0){
                //     const TfLiteTensor* output_tensor = GetOutput(context, node, 0);
                //     TfLiteIntArray*  dims = output_tensor->dims;
                //     LOGE("---------%s--------------\n",output_tensor->name);
                //     int size = NumElements(output_tensor);                
                //     float* ptr = (float*)(output_tensor->data.f);
                //     FILE* fp = fopen("1.txt","w+");
                //     char temp[256];
                //     for(int i=0;i<size;i++){
                //         snprintf(temp,256,"%d\n",(int)(ptr[i]));
                //         fwrite(temp,strlen(temp),1,fp);
                //     }
                //     fclose(fp);
                // }
        }

    }
#endif
    return 0;
}



void write_model_file(const void* buffer,size_t size, const char* filename)
{
    LOGE("write_model_file\n");
    char* buf = (char*)buffer;
    FILE* file = fopen(filename, "w");  //TODO  std::unique_ptr
    fwrite(buf, sizeof(char), size, file);
    fclose(file);		 
}

char* read_model_file(const char* filename)
{
  // Obtain the file size using fstat, or report an error if that fails.
  LOGE("read_model_file\n");
  FILE* file = fopen(filename, "rb");  //TODO  std::unique_ptr
  if (!file) {
    LOGE("Could not open '%s'.", filename);
    return NULL;
  }
  struct stat sb;
// support usage of msvc's posix-like fileno symbol
#ifdef _WIN32
#define FILENO(_x) _fileno(_x)
#else
#define FILENO(_x) fileno(_x)
#endif
  if (fstat(FILENO(file), &sb) != 0) {
    LOGE("Failed to get file size of '%s'.", filename);
    fclose(file);
    return NULL;
  }
#undef FILENO
  size_t buffer_size_bytes_ = sb.st_size;
  //std::unique_ptr<char[]> buffer(new char[buffer_size_bytes_]);
  //char* buffer = new char[buffer_size_bytes_]; //TODO new=>malloc
  char* buffer = (char*)malloc(sizeof(char)*buffer_size_bytes_);
  if (!buffer) {
    LOGE("Malloc of buffer to hold copy of '%s' failed.", filename);
    fclose(file);
    return NULL;
  }
  size_t bytes_read =
      fread(buffer, sizeof(char), buffer_size_bytes_, file);
  if (bytes_read != buffer_size_bytes_) {
    LOGE("Read of '%s' failed (too few bytes read).",filename);
    free (buffer);
    fclose(file);
    return NULL;
  }
  // Versions of GCC before 6.2.0 don't support std::move from non-const
  // char[] to const char[] unique_ptrs.
  
  fclose(file);
  return buffer;
}


//int main(int argc, char *argv[])
void forward(Interpreter* interpreter)
{
    Subgraph* subgraphStruct = interpreter->subgraphs;
    for(int j=0; j<subgraphStruct->nodes_and_registrations_size;j++)
    {
        TfLiteRegistration* op = &(subgraphStruct->nodes_and_registrations_[j].registration);
        TfLiteNode* node = &(subgraphStruct->nodes_and_registrations_[j].node);
        OpInvoke(subgraphStruct,op,node);
    }
}

void setInput(Interpreter* interpreter,float* input)
{
    Subgraph* subgraphStruct = interpreter->subgraphs;
    TfLiteNode* node = &(subgraphStruct->nodes_and_registrations_[0].node);
    TfLiteContext* context = &(subgraphStruct->context_);

    const TfLiteTensor* input_tensor = GetInput(context, node, 0);
    LOGE("----------------------%s-----------",input_tensor->name);
    int size = NumElements(input_tensor);
                    
    for(int i= 0; i< size;i++)
    {
        input_tensor->data.f[i] = input[i];
    }
}

void saveOutput(Interpreter* interpreter,char* dir)
{
    Subgraph* subgraphStruct = interpreter->subgraphs;
    TfLiteContext* context = &(subgraphStruct->context_);
    int size = 0;
    
    for(int i=0;i<subgraphStruct->nodes_and_registrations_size;i++)
    {
        TfLiteNode* node = &(subgraphStruct->nodes_and_registrations_[i].node);
        const TfLiteTensor* tensor = GetOutput(context, node, 0); 
        extern void dump_data(char* name,float* data, int num);
        char prefix[256]={0};
        strncpy(prefix,dir,strlen(dir));
        char temp[256]={"\0"};
        for(int j=0;j<strlen(tensor->name);j++)
        {
            if(tensor->name[j] == '/')
            {
                temp[j] = '_';
            }else{
                temp[j] = tensor->name[j];
            }
        }    
        size = NumElements(tensor);
        dump_data(strncat(prefix,temp,strlen(temp)),tensor->data.f,size);
    }

}

void estimator_getOuput(Interpreter* interpreter,float* fc,float* softmax,int* argmax)
{
    Subgraph* subgraphStruct = interpreter->subgraphs;
    
    TfLiteContext* context = &(subgraphStruct->context_);
    
    int argmax_index  = subgraphStruct->nodes_and_registrations_size -1;
    int softmax_index = subgraphStruct->nodes_and_registrations_size -2;
    int fc_index      = subgraphStruct->nodes_and_registrations_size -3;

    int size = 0;
    float* ptr = NULL;

    TfLiteNode* fc_node = &(subgraphStruct->nodes_and_registrations_[fc_index].node);
    const TfLiteTensor* fc_tensor = GetOutput(context, fc_node, 0);
    LOGE("\n fc_tensor name %s\n",fc_tensor->name);

    size = NumElements(fc_tensor);
    ptr = (float*)(fc_tensor->data.f);
    for(int i=0;i<size;i++){     
         fc[i] = ptr[i];
     }

    TfLiteNode* softmax_node = &(subgraphStruct->nodes_and_registrations_[softmax_index].node);
    const TfLiteTensor* softmax_tensor = GetOutput(context, softmax_node, 0);
    LOGE("\n softmax_tensor name %s\n",softmax_tensor->name);
    size = NumElements(softmax_tensor);
    ptr = (float*)(softmax_tensor->data.f);
    for(int i=0;i<size;i++){         
         softmax[i] = ptr[i];
     }


    TfLiteNode* argmax_node = &(subgraphStruct->nodes_and_registrations_[argmax_index].node);
    const TfLiteTensor* argmax_tensor = GetOutput(context, argmax_node, 0);
     size = NumElements(argmax_tensor);
     ptr = (float*)(argmax_tensor->data.f);
     LOGE("\n argmax_tensor name %s\n",argmax_tensor->name);
     for(int i=0;i<size;i++){
         argmax[i] = ptr[i];
     } 
}


void feature_getOuput(Interpreter* interpreter,float* add)
{
    Subgraph* subgraphStruct = interpreter->subgraphs;
    
    TfLiteContext* context = &(subgraphStruct->context_);
    
    int add_index  = subgraphStruct->nodes_and_registrations_size -1;
    

    int size = 0;
    float* ptr = NULL;

    TfLiteNode* add_node = &(subgraphStruct->nodes_and_registrations_[add_index].node);
    const TfLiteTensor* add_tensor = GetOutput(context, add_node, 0);
    LOGE("\n add_tensor name %s\n",add_tensor->name);

    size = NumElements(add_tensor);
    ptr = (float*)(add_tensor->data.f);
    for(int i=0;i<size;i++){     
         add[i] = ptr[i];
     }
}


//int estimator_graph_len = 0;
//Interpreter estimator_interpreter = {0};
//OpResolver  estimator_opResolver  = {0};

//int feature_graph_len = 0;
//Interpreter feature_interpreter = {0};
//OpResolver  feature_opResolver  ={0};

typedef struct 
{
  Interpreter estimator_interpreter;
  OpResolver  estimator_opResolver;
  char* buffer;
}estimator_tflite;

typedef struct
{
  Interpreter feature_interpreter;
  OpResolver  feature_opResolver;
  char* buffer;
}feature_tflite;

int load_asc_estimator_model(void ** tflite)
{
    estimator_tflite *estimator_tflite_ = (estimator_tflite*)malloc(sizeof(estimator_tflite));
    if (estimator_tflite_)
      *tflite = estimator_tflite_;
    else
      return -1;
    memset(estimator_tflite_, 0x00, sizeof(estimator_tflite_));
    memset(&estimator_tflite_->estimator_interpreter, 0x00, sizeof(Interpreter));
    memset(&estimator_tflite_->estimator_opResolver, 0x00, sizeof(OpResolver));
    Init_Interpreter(&(estimator_tflite_->estimator_interpreter));
    Init_OpResolver(&(estimator_tflite_->estimator_opResolver));
    estimator_tflite_->buffer = read_model_file("estimator_model.tflite");
    if (estimator_tflite_->buffer == NULL)
    {
      printf("estimator_model.tflite reading is failed, please confirm there is the file! \n");
      return -1;
    }
    access_tflite_buffer(estimator_tflite_->buffer,&(estimator_tflite_->estimator_interpreter),&(estimator_tflite_->estimator_opResolver));
    return 0;
}

int unload_asc_estimator_model(void *estimator_tflite_)
{
#ifndef DISABLE_DEBUG_LOG
  printf("unload_asc_estimator_model calling...\n");
#endif
  estimator_tflite * p = (estimator_tflite*)estimator_tflite_;
  if (p)
  {
    if(p->buffer)
      free(p->buffer);

    Delete_Interpreter(&(p->estimator_interpreter));
    Delete_OpResolver(&(p->estimator_opResolver));
    free(p);
  }
  return 0;
}

int load_asc_feature_model(void ** tflite)
{
    feature_tflite *feature_tflite_ = (feature_tflite*)malloc(sizeof(feature_tflite));
    if (feature_tflite_)
      *tflite = feature_tflite_;
    else
      return -1;
    memset(feature_tflite_, 0x00, sizeof(feature_tflite_));
    memset(&feature_tflite_->feature_interpreter, 0x00, sizeof(Interpreter));
    memset(&feature_tflite_->feature_opResolver, 0x00, sizeof(OpResolver));
    Init_Interpreter(&(feature_tflite_->feature_interpreter));
    Init_OpResolver(&(feature_tflite_->feature_opResolver));
    feature_tflite_->buffer = read_model_file("feature_model.tflite");
    if (feature_tflite_->buffer == NULL)
    {
      printf("feature_model.tflite reading is failed, please confirm there is the file! \n");
      return -1;
    }
    access_tflite_buffer(feature_tflite_->buffer,&(feature_tflite_->feature_interpreter),&(feature_tflite_->feature_opResolver));
    return 0;
}

void unload_asc_feature_model(void *feature_tflite_)
{
#ifndef DISABLE_DEBUG_LOG
  printf("unload_asc_feature_model calling...\n");
#endif
  feature_tflite * p = (feature_tflite*)feature_tflite_;
  if (p)
  {
    if (p->buffer)
      free(p->buffer);

    Delete_Interpreter(&(p->feature_interpreter));
    Delete_OpResolver(&(p->feature_opResolver));
    free(p);
  }
  return;
}

//#define DEBUG
void asc_estimator_forward(void *estimator_tflite_, float* input,float* fc_out, float* softmax_out,int* argmax_out)
{
    estimator_tflite * p = (estimator_tflite*)(estimator_tflite_);
    float fc[3] = {0};
    float softmax[3] = {0};
    int argmax = 0;

    setInput(&(p->estimator_interpreter),input);
    forward(&(p->estimator_interpreter));
    estimator_getOuput(&(p->estimator_interpreter),fc,softmax,&argmax);
    #ifdef DEBUG
    saveOutput(&(p->estimator_interpreter),"estimator/");
    #endif 

    fc_out[0] = fc[0];
    fc_out[1] = fc[1];
    fc_out[2] = fc[2];

    softmax_out[0] = softmax[0];
    softmax_out[1] = softmax[1];
    softmax_out[2] = softmax[2];

    *argmax_out = argmax;
}


void asc_feature_model_forward(void *feature_tflite_, float* input,float* fout)
{
    feature_tflite * p = (feature_tflite*)feature_tflite_;
    setInput(&(p->feature_interpreter),input);
    forward(&(p->feature_interpreter));
    feature_getOuput(&(p->feature_interpreter),fout);
    #ifdef DEBUG
    saveOutput(&(p->feature_interpreter),"feature/");
    #endif 
}