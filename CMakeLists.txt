cmake_minimum_required(VERSION 3.5.1)

project (iamf)
message(STATUS "start build iamf")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3") 
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared library" ON)

include_directories (include)
aux_source_directory (src/common DIR_IAMF_COMMON)
aux_source_directory (src/iamf_dec DIR_IAMF_DEC)
aux_source_directory (src/iamf_dec/opus DIR_IAMF_DEC_OPUS)
aux_source_directory (src/iamf_dec/aac DIR_IAMF_DEC_AAC)
aux_source_directory (src/iamf_dec/pcm DIR_IAMF_DEC_PCM)
aux_source_directory (src/iamf_enc DIR_IAMF_ENC)

set(CODEC_LIB_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/lib")
set(EXTER_LIB_DIR  "${PROJECT_SOURCE_DIR}/dep_external/lib")
set(DMPD_LIB_DIR  "${PROJECT_SOURCE_DIR}/src/iamf_enc/dmpd")
set(INCLUDE_OPUS_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/include/opus")
set(INCLUDE_AAC_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/include/fdk-aac")

include_directories(
    ${INCLUDE_OPUS_DIR}
    ${INCLUDE_AAC_DIR}
    ${PROJECT_SOURCE_DIR}/src/common
    ${PROJECT_SOURCE_DIR}/src/iamf_dec
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/opus
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/aac
    ${PROJECT_SOURCE_DIR}/src/iamf_enc
    ${DMPD_LIB_DIR}/asc/include
    ${DMPD_LIB_DIR}/heq/include
)
link_directories (
    ${CODEC_LIB_DIR}
    ${EXTER_LIB_DIR}
    ${DMPD_LIB_DIR}/asc
    ${DMPD_LIB_DIR}/asc/3rdparty/decimate
    ${DMPD_LIB_DIR}/asc/3rdparty/stft
    ${DMPD_LIB_DIR}/heq
)

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED ${DIR_IAMF_COMMON} ${DIR_IAMF_ENC}
    ${DIR_IAMF_DEC_OPUS} ${DIR_IAMF_DEC_AAC} ${DIR_IAMF_DEC_PCM} ${DIR_IAMF_DEC})
  target_link_libraries (${PROJECT_NAME} opus fdk-aac asc heq flatccrt decimate stft fftw3f )
  add_subdirectory(src/iamf_enc/dmpd)
else()
  add_library(${PROJECT_NAME} STATIC ${DIR_IAMF_COMMON} ${DIR_IAMF_ENC}
    ${DIR_IAMF_DEC_OPUS} ${DIR_IAMF_DEC_AAC} ${DIR_IAMF_DEC_PCM} ${DIR_IAMF_DEC})
  add_subdirectory(src/iamf_enc/dmpd)
endif()



set(IAMF_PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IAMF_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IAMF_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IAMF_defines.h
    )
set_target_properties(iamf PROPERTIES PUBLIC_HEADER "${IAMF_PUBLIC_HEADER}")


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir ${CMAKE_INSTALL_PREFIX}/include)
configure_file(iamf.pc.in iamf.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iamf.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig) 
                                 
if(BUILD_SHARED_LIBS)
  install(TARGETS ${PROJECT_NAME}  
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iamf)
else()
  install(TARGETS ${PROJECT_NAME}  
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iamf)
endif()

