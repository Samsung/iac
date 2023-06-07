cmake_minimum_required(VERSION 3.1)

project(asc)

message(STATUS "start build")

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG --std=c99") 
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG --std=c99 -g -fPIC") 

set(FLATCC_INCLUDE_DIR  "../../../../dep_external/include")
set(FLATCC_LIB_DIR  "../../../../dep_external/lib")

ADD_DEFINITIONS(-DDISABLE_DEBUG_LOG)

set(sources
    src/common.c
	src/util.c
	src/opresolver.c
	src/subgraph.c
    src/interpreter.c   
	src/tflite.c
	src/kernel.c
	src/kernels/abs.c
	src/kernels/add.c
	src/kernels/arg_max.c
	src/kernels/conv2d.c
	src/kernels/depthwise_conv_2d.c
	src/kernels/fully_connected.c
	src/kernels/max_pool_2d.c
	src/kernels/mean.c
	src/kernels/mirror_pad.c
	src/kernels/mul.c
	src/kernels/neg.c
	src/kernels/relu.c
	src/kernels/rsqrt.c
	src/kernels/softmax.c
	src/kernels/square_difference.c
	src/kernels/sub.c
	src/kernels/tanh.c
	src/kernels/transpose_conv.c
	src/kernels/strided_slice.c
	src/kernels/sum.c
	src/kernels/sqrt.c
	src/kernels/div.c
	src/kernels/reshape.c
	src/kernels/concatenation.c
	src/asc_dn.c
	src/asc_test.c
	src/asc_common_bs.c
	src/transpose_1.c
	src/transpose_3.c	
	src/mel_matrix.c
	src/stft_abs.c
	src/effect_stft_abs.c
	src/sample_d_data.c
	src/sample_e_data.c
	resampler/asc_resample.c
)

INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}/3rdparty/stft
  ${FLATCC_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/generated
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/resampler
)

LINK_DIRECTORIES(
  ${FLATCC_LIB_DIR}
  ${PROJECT_SOURCE_DIR}/3rdparty/decimate
  ${PROJECT_SOURCE_DIR}/3rdparty/stft
)

if(DISABLE_ASC_EXECUTION)
message(STATUS ${DISABLE_ASC_EXECUTION})
add_library(${PROJECT_NAME} STATIC ${sources})
else()
message(STATUS "Enabale ASC Execution!!!!")
add_executable(${PROJECT_NAME} ${sources})
endif()

TARGET_LINK_LIBRARIES(${PROJECT_NAME} flatccrt decimate stft fftw3f)
add_subdirectory(3rdparty)


