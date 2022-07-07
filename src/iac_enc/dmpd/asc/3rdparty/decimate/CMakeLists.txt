cmake_minimum_required(VERSION 3.1)

set(SRC flitflit.cpp)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -Wno-ignored-attributes") 

add_library(decimate ${SRC})
target_compile_options(decimate PRIVATE "-fPIC")
target_include_directories(decimate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/eigen3)

#install(TARGETS decimate ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/lib)


if(TEST)
    set(TEST_SRC test.c)
    add_executable(decimate_test ${TEST_SRC})
    target_link_libraries(decimate_test decimate)
endif(TEST)
