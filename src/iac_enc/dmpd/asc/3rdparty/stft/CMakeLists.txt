cmake_minimum_required(VERSION 3.1)

set(SRC stft.c hann_matrix.c window.c)

add_library(stft ${SRC})
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99") 

target_compile_options(stft PRIVATE "-fPIC")
target_link_libraries(stft fftw3f)
target_include_directories(stft PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)

#install(TARGETS stft ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/lib)

if(TEST)
    set(TEST_SRC stft_test.c transpose.c)
    add_executable(stft_test ${TEST_SRC})
    target_link_libraries(stft_test stft)
endif(TEST)



