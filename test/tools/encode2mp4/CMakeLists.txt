cmake_minimum_required(VERSION 3.1)

project (encode2mp4)

message(status,"+++++++++++encode2mp4+++++++++++++")

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAC_DIR  "${CMAKE_INSTALL_PREFIX}/include/iac")
#add_definitions (-O2)
set(CMAKE_CXX_FLAGS "-fpermissive -std=c++0x")

message(status,${INCLUDE_IAC_DIR})
include_directories(
    ${INCLUDE_IAC_DIR}
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
    obuwrite.c
)

link_directories(
  ${LIB_DIR}
)

add_executable (encode2mp4 ${sources})

#target_link_libraries (mp4opusparser opus)
target_link_libraries (encode2mp4 iac)

