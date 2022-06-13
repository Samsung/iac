cmake_minimum_required(VERSION 3.1)

project (mp4iacplay)

message(status,"+++++++++++mp4iacplay+++++++++++++")
add_definitions (-O2)
include_directories (mp4iacplay)


include_directories (include)
aux_source_directory (player DIR_MP4IACPLAY_PLAYER)
aux_source_directory (src DIR_MP4IACPLAY_SRC)

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAC_DIR  "${CMAKE_INSTALL_PREFIX}/include/iac")
#add_definitions (-O2)
set(CMAKE_CXX_FLAGS "-fpermissive -std=c++0x")

message(status,${INCLUDE_IAC_DIR})
include_directories(
    ${INCLUDE_IAC_DIR}
)
link_directories(
  ${LIB_DIR}
)


add_executable (mp4iacplayer player/mp4iacplayer.c
    ${DIR_MP4IACPLAY_PLAYER} ${DIR_MP4IACPLAY_SRC})

target_link_libraries (mp4iacplayer iac m)

