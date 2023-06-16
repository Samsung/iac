cmake_minimum_required(VERSION 3.1)

project (iamfpackager)

message(status,"+++++++++++iamf+++++++++++++")

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
set(INCLUDE_DEP_EXTERNAL_WAV_DIR  "../../../dep_external/include/wav")
#add_definitions (-O2)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

message(status,${INCLUDE_IAMF_DIR})
include_directories(
    ${INCLUDE_IAMF_DIR}
    ${PROJECT_SOURCE_DIR}
    ${INCLUDE_DEP_EXTERNAL_WAV_DIR}
)

set(sources
    dmemory.c
    main.c
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

