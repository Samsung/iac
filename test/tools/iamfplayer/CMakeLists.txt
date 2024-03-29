cmake_minimum_required(VERSION 3.1)

project (iamfplayer)

message(status,"+++++++++++iamfplayer+++++++++++++")

option(BINAURALIZER "Enable binaural rendering" OFF)
option(SUPPORT_VERIFIER "Output vlogging file" OFF)

include_directories (include)
aux_source_directory (player DIR_IAMFPLAY_PLAYER)
aux_source_directory (src DIR_IAMFPLAY_SRC)

option(SAMSUNG_TV "For samsung tv."  ON)
if (SAMSUNG_TV)
  add_definitions(-DSAMSUNG_TV)
endif()

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
set(INCLUDE_DEP_EXTERNAL_WAV_DIR  "../../../dep_external/include/wav")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--no-as-need")

if(SUPPORT_VERIFIER)
  message(STATUS "Output vlogging file")
  add_definitions(-DSUPPORT_VERIFIER=1)
endif()


message(status,${INCLUDE_IAMF_DIR})
include_directories(
    ${INCLUDE_IAMF_DIR}
    ${INCLUDE_DEP_EXTERNAL_WAV_DIR}
)
link_directories(
  ${LIB_DIR}
)


add_executable (iamfplayer player/iamfplayer.c
    ${DIR_IAMFPLAY_PLAYER} ${DIR_IAMFPLAY_SRC})

target_link_libraries (iamfplayer iamf m)

