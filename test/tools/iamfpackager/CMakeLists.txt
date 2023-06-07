cmake_minimum_required(VERSION 3.1)

project (iamfpackager)

message(status,"+++++++++++iamf+++++++++++++")

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
#add_definitions (-O2)
set(CMAKE_CXX_FLAGS "-fpermissive -std=c++0x")

message(status,${INCLUDE_IAMF_DIR})
include_directories(
    ${INCLUDE_IAMF_DIR}
    ${PROJECT_SOURCE_DIR}
)

set(sources
    dmemory.c
    main.c
    wavreader.c
    wavwriter.c
    mp4mux.c
    bitstreamrw.c
    progressbar.c
    cJSON.c
)

link_directories(
  ${LIB_DIR}
)

add_executable (iamfpackager ${sources})

target_link_libraries (iamfpackager iamf m)

